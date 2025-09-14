#ifndef TPWTIMEMODE_H_INCLUDED
#define TPWTIMEMODE_H_INCLUDED

#include "RtcDS1302.h"
#include "tpwCore.h"
#include "tpwTimeCommons.h"

using namespace TPWCore;


// ///////////////////// Time mode //////////////////////

enum ShowTimeState {
  // View states
  SHOW_HOUR,
  SHOW_DATE,
  SHOW_WEEKDAY,
  SHOW_YEAR,
  // Configuration states
  SET_ENTER,
  SET_HOUR,
  SET_MINUTES,
  SET_FORMAT,
  SET_MONTH,
  SET_DAY,
  SET_YEAR,
  SET_DISPLAY,
  SET_BRIGHTNESS,
};

class TPWTime : public Mode {
  public:
    RtcDateTime now;
    int hour = 0;
    int minute = 0;
    int day = 1;
    int month = 1;
    int year = 1066;
    // 0 = Sunday, 1 = Monday, ... 6 = Saturday
    uint8_t dayOfWeek = 1;
  
    TPWTime(Core *core) : Mode(core) {
      _configMode = false;
      _state = SHOW_HOUR;
      _nextTick = 0;
      _tickState = false;
    }

    void handleLoop() {
      handleLoop(LOOP);
    }
  
    void handleLoop(ModeEvent event) {
      if (event == ENTERED) {
        // Initialize mode
        _lastAction = _core->currentMillis;
        _nextTick = _core->currentMillis + 1000;
        _tickState = false;
        _readDateVars();
        _showHour();
      } else if (
        _core->buttons[BUTTON_SET].isPressed() ||
        _core->buttons[BUTTON_MODE].isPressed() ||
        _core->buttons[BUTTON_DOWN].isPressed() ||
        _core->buttons[BUTTON_UP].isPressed()
      ) {
        _lastAction = _core->currentMillis;
      }
      if (_configMode) {
        _handleSetTimeMode();
      } else {
        _handleShowTimeMode();
      }
      
    }

  private:
    bool _configMode;
    ShowTimeState _state;
    // State variables
    unsigned long _lastAction;
    unsigned long _nextTick;
    bool _tickState;
    bool _updateDisplay;

    void _readDateVars() {
      now = _core->Rtc->GetDateTime();
      hour = now.Hour();
      minute = now.Minute();
      day = now.Day();
      month = now.Month();
      year = now.Year();
      dayOfWeek = now.DayOfWeek();
    }

    // ----------------------------------------------------------
    //                  SHOW TIME MODE
    // ----------------------------------------------------------
    void _showHour() {
      _state = SHOW_HOUR;
      int hourDisplay;
      hourDisplay = (hour * 100) + minute % 100;
      setNumberSegs(_core->display->digitValues, hourDisplay, 2, true);
    }
    
    void _handleShowTimeMode() {
      int dateDisplay;
      
      if (_core->buttons[BUTTON_SET].wasLongPressed()) {
        _configMode = true;
        _nextTick = _core->currentMillis + SHOW_SET_TIME;
        _state = SET_ENTER;
        _updateDisplay = true;
        return;
      } else if (_core->buttons[BUTTON_SET].wasReleased()) {
        switch (_state) {
          case SHOW_HOUR:
            _state = SHOW_DATE;
            // Date
            if (_core->dateFormat == true) {
              dateDisplay = (now.Day() * 100) + now.Month();
            } else {
              dateDisplay = (now.Month() * 100) + now.Day();
            }
            setNumberSegs(_core->display->digitValues, dateDisplay, 2, true);
            break; 
          case SHOW_DATE:
            _state = SHOW_WEEKDAY;
            // Weekday
            _core->display->digitValues[0] = weekDays[dayOfWeek][0];
            _core->display->digitValues[1] = weekDays[dayOfWeek][1];
            _core->display->digitValues[2] = weekDays[dayOfWeek][2];
            _core->display->digitValues[3] = weekDays[dayOfWeek][3];
            break; 
          case SHOW_WEEKDAY:
            _state = SHOW_YEAR;
            setNumberSegs(_core->display->digitValues, now.Year(), -1, true);
            break; 
          case SHOW_YEAR:
            _showHour();
            break;
          default:
            _configMode = false;
            _showHour();
        }
      } else if (_core->buttons[BUTTON_MODE].wasReleased()) {
        if (_state == SHOW_HOUR) {
          _core->goToNext();
          return;
        } else {
          _state = SHOW_HOUR;
        }
      }

      if (_core->buttons[BUTTON_UP].wasLongPressed() || _core->buttons[BUTTON_UP].isRepeating()) {
        _core->setBrightness(_core->getBrightness() + 1);
      } else if (_core->buttons[BUTTON_DOWN].wasLongPressed() || _core->buttons[BUTTON_DOWN].isRepeating()) {
        _core->setBrightness(_core->getBrightness() - 1);
      }

      // Display time components
      if (
        _state == SHOW_HOUR &&
        _nextTick < _core->currentMillis
      ) {
        _nextTick = _nextTick + 500;
        _tickState = !_tickState;
        if (_tickState) {
          _core->display->digitValues[1] = _core->display->digitValues[1] | 0b10000000;
        } else {
          _core->display->digitValues[1] = _core->display->digitValues[1] & 0b01111111;
        }
      }
    
      // Check for inactivity → sleep
      if (_core->currentMillis - _lastAction > _core->awakeInterval*1000) {
        _core->goToSleep();
      }
    }

    void _handleSetTimeMode() {
      static int setHour;
      static int setMinute;
      static int setDay;
      static int setMonth;
      static int setYear;
      static bool setDateFormat;
      float brightness;
    
      int maxDays;
    
      if (_core->buttons[BUTTON_MODE].wasLongPressed()) {
        _showHour();
        return;
      }
    
      if (_state != SET_ENTER) {
        if (_core->buttons[BUTTON_SET].wasReleased()) {
          maxDays = getMonthDays(setMonth, setYear);
          if (setDay > maxDays) {
            setDay = maxDays;
          }
          _core->Rtc->SetDateTime(RtcDateTime(setYear, setMonth, setDay, setHour, setMinute, now.Second()));
          _readDateVars();
          _core->dateFormat = setDateFormat;
          _showHour();
        }
      }
    
      // Handle inputs
      switch (_state) {
        case SET_ENTER:
          if (_nextTick < _core->currentMillis) {
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
            _state = SET_HOUR;
            _readDateVars();
            setHour = hour;
            setMinute = minute;
            setDay = day;
            setMonth = month;
            setYear = year; 
            setDateFormat = _core->dateFormat;
          }
          break;
        case SET_HOUR:
          if (_core->buttons[BUTTON_MODE].wasReleased()) {
            _state = SET_MINUTES;
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
            break;
          }    
          if (_core->buttons[BUTTON_UP].wasReleased() || _core->buttons[BUTTON_UP].isRepeating()) {
            setHour = (setHour + 1) % 24;
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
          }
          if (_core->buttons[BUTTON_DOWN].wasReleased() || _core->buttons[BUTTON_DOWN].isRepeating()) {
            setHour = (setHour + 23) % 24;
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
          }
          break;
        case SET_MINUTES:
          if (_core->buttons[BUTTON_MODE].wasReleased()) {
            _state = SET_DAY;
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
            break;
          }    
          if (_core->buttons[BUTTON_UP].wasReleased() || _core->buttons[BUTTON_UP].isRepeating()) {
            setMinute = (setMinute + 1) % 60;
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
          }
          if (_core->buttons[BUTTON_DOWN].wasReleased() || _core->buttons[BUTTON_DOWN].isRepeating()) {
            setMinute = (setMinute + 59) % 60;
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
          }    
          break;
        case SET_DAY:
          if (_core->buttons[BUTTON_MODE].wasReleased()) {
            _state = SET_MONTH;
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
            break;
          }
          // Get max days in setMonth for setYear
          maxDays = getMonthDays(setMonth, setYear);
          if (_core->buttons[BUTTON_UP].wasReleased() || _core->buttons[BUTTON_UP].isRepeating()) {
            setDay = (setDay + 1) % maxDays;
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
          }
          if (_core->buttons[BUTTON_DOWN].wasReleased() || _core->buttons[BUTTON_DOWN].isRepeating()) {
            setDay = (setDay + maxDays - 1) % maxDays;
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
          }
          break;
        case SET_MONTH:
          if (_core->buttons[BUTTON_MODE].wasReleased()) {
            _state = SET_YEAR;
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
            break;
          }
          if (_core->buttons[BUTTON_UP].wasReleased() || _core->buttons[BUTTON_UP].isRepeating()) {
            setMonth = (setMonth + 1) % 12;
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
          }
          if (_core->buttons[BUTTON_DOWN].wasReleased() || _core->buttons[BUTTON_DOWN].isRepeating()) {
            setMonth = (setMonth + 11) % 12;
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
          }    
          break;
        case SET_YEAR:
          if (_core->buttons[BUTTON_MODE].wasReleased()) {
            _state = SET_FORMAT;
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
            break;
          }    
          if (_core->buttons[BUTTON_UP].wasReleased() || _core->buttons[BUTTON_UP].isRepeating()) {
            setYear = setYear + 1;
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
          }
          if (_core->buttons[BUTTON_DOWN].wasReleased() || _core->buttons[BUTTON_DOWN].isRepeating()) {
            setYear = setYear - 1;
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
          }    
          break;
        case SET_FORMAT:
          if (_core->buttons[BUTTON_MODE].wasReleased()) {
            _state = SET_DISPLAY;
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
            break;
          }    
          // Set day/month order format
          if (_core->buttons[BUTTON_UP].wasReleased() || _core->buttons[BUTTON_DOWN].wasReleased()) {
            setDateFormat = !setDateFormat;
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
          }
          break;
        case SET_DISPLAY:
          if (_core->buttons[BUTTON_MODE].wasReleased()) {
            _state = SET_BRIGHTNESS;
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
            break;
          }    
          if (_core->buttons[BUTTON_UP].wasReleased() || _core->buttons[BUTTON_UP].isRepeating()) {
            _core->awakeInterval = _core->awakeInterval + 1;
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
          }
          if (_core->buttons[BUTTON_DOWN].wasReleased() || _core->buttons[BUTTON_DOWN].isRepeating()) {
            _core->awakeInterval = _core->awakeInterval - 1;
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
          }
          if (_core->awakeInterval < 3) {
            _core->awakeInterval = 3;
          }
          if (_core->awakeInterval > 9) {
            _core->awakeInterval = 9;
          }
          break;
        case SET_BRIGHTNESS:
          if (_core->buttons[BUTTON_MODE].wasReleased()) {
            _state = SET_HOUR;
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
            break;
          }    
          // Set display brightness
          if (_core->buttons[BUTTON_UP].wasReleased() || _core->buttons[BUTTON_UP].isRepeating()) {
            _core->setBrightness(_core->getBrightness() + 1);
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
          }
          if (_core->buttons[BUTTON_DOWN].wasReleased() || _core->buttons[BUTTON_DOWN].isRepeating()) {
            _core->setBrightness(_core->getBrightness() - 1);
            _updateDisplay = true;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
          }
          break;
        default:
          _configMode = false;
          _state = SHOW_HOUR;
          return;
      }
    
      if (_core->currentMillis > _nextTick) {
        _updateDisplay = true;
        _nextTick = _core->currentMillis + BLINK_INTERVAL;
        _tickState = !_tickState;
      }

      if (_updateDisplay) {
        _updateDisplay = false;
        // Handle display
        switch (_state) {
          case SET_ENTER:
            _core->display->digitValues[0] = 0x6D; // S
            _core->display->digitValues[1] = 0x79; // E
            _core->display->digitValues[2] = 0x78; // t
            _core->display->digitValues[3] = 0x00;
            break;
          case SET_HOUR:
            setNumberSegs(_core->display->digitValues, (setHour * 100) + setMinute, 2, true);
            if (_tickState) {
               _core->display->digitValues[0] = 0;
               _core->display->digitValues[1] = 0;
            }
            break;
          case SET_MINUTES:
            setNumberSegs(_core->display->digitValues, (setHour * 100) + setMinute, 2, true);
      
            if (_tickState) {
               _core->display->digitValues[2] = 0;
               _core->display->digitValues[3] = 0;
            }
            break;
          case SET_DAY:
            _core->display->digitValues[0] = 0x5E; // 'd'
            _core->display->digitValues[1] = 0x6E; // 'y'
            _core->display->digitValues[2] = digitsSegments[(int)(setDay / 10)];
            _core->display->digitValues[3] = digitsSegments[setDay % 10];
            if (_tickState) {
               _core->display->digitValues[2] = 0;
               _core->display->digitValues[3] = 0;
            }
            break;
          case SET_MONTH:
            _core->display->digitValues[0] = 0x37; // 'M'
            _core->display->digitValues[1] = 0x5C; // 'o'
            _core->display->digitValues[2] = digitsSegments[(int)(setMonth / 10)];
            _core->display->digitValues[3] = digitsSegments[setMonth % 10];
            if (_tickState) {
               _core->display->digitValues[2] = 0;
               _core->display->digitValues[3] = 0;
            }
            break;
          case SET_YEAR:
            setNumberSegs(_core->display->digitValues, setYear, -1, true);
            if (_tickState) {
              _core->display->digitValues[0] = 0;
              _core->display->digitValues[1] = 0;
              _core->display->digitValues[2] = 0;
              _core->display->digitValues[3] = 0;
            }
            break;
          case SET_FORMAT:
            if (setDateFormat) {
              // dY.Mo
              _core->display->digitValues[0] = 0x5E;
              _core->display->digitValues[1] = 0xEE;
              _core->display->digitValues[2] = 0x37;
              _core->display->digitValues[3] = 0x5C;
            } else {
              // Mo.dY
              _core->display->digitValues[0] = 0x37;
              _core->display->digitValues[1] = 0xDC;
              _core->display->digitValues[2] = 0x5E;
              _core->display->digitValues[3] = 0x6E;
            }
            break;
          case SET_DISPLAY:
            // Set display time
            _core->display->digitValues[0] = 0x6D; // 'S'
            _core->display->digitValues[1] = 0x38; // 'L'
            _core->display->digitValues[2] = 0x73; // 'P'
            if (_tickState) {
              _core->display->digitValues[3] = 0;
            } else {
              _core->display->digitValues[3] = digitsSegments[(int)(_core->awakeInterval % 10)];
            }
            break;
          case SET_BRIGHTNESS:
            // Set display segments
            brightness = _core->getBrightness();
            _core->display->digitValues[0] = 0;
            if (_tickState) {
              _core->display->digitValues[1] = 0;
              _core->display->digitValues[2] = 0;
              _core->display->digitValues[3] = 0;
            } else {
              _core->display->digitValues[1] = digitsSegments[(int)(brightness / 100)];
              _core->display->digitValues[2] = digitsSegments[(int)(brightness / 10) % 10];
              _core->display->digitValues[3] = digitsSegments[(int)brightness % 10];
            }
            break;
          default:
            _configMode = false;
            _state = SHOW_HOUR;
            return;
  
        }
      }
    }
};

#endif
