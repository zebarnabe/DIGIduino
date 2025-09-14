#ifndef TPWSUNMODE_H_INCLUDED
#define TPWSUNMODE_H_INCLUDED

#include "RtcDS1302.h"
#include "tpwCore.h"
#include "tpwTimeCommons.h"

using namespace TPWCore;

// ///////////////////// Sun mode //////////////////////

enum SunState {
  SUN_ENTER=0,
  SUN_NOON=1,
  SUN_SET=2,
  SUN_RISE=3,
  // Configuration states
  SET_LOCALE=4,
  SET_TZ_OFFSET_HOUR=5,
  SET_TZ_OFFSET_MINUTE=6,
  SET_LAT_DEG=7,
  SET_LAT_MIN=8,
  SET_LAT_SEC=9,
  SET_LONG_DEG=10,
  SET_LONG_MIN=11,
  SET_LONG_SEC=12
};

struct Coord {
  bool positive;
  int deg;
  byte minutes;
  byte seconds;
};

float getDecimalCoord(Coord coordinate) {
  float decimals = coordinate.deg + (float)coordinate.minutes / 60.0 + (float)coordinate.seconds / 3600.0;
  if (coordinate.positive) {
    return decimals;
  }
  return -decimals;
}

float toDegrees(float radians) {
  return radians * 180 / PI;
} 

float toRadians(float radians) {
  return radians * PI / 180.0;
} 

void dayFracToTime(float frac, byte* hour, byte* minute) {
  *hour = (byte)(frac * 24 / 24);
  *minute = (byte)(frac * 1440 / 1440) - (*hour) * 60;
}

void getSolarDates(
    int year,
    byte month,
    byte day,
    float latitude,
    float longitude,
    float offset_minutes,
    float* noon,
    float* sunrise,
    float* sunset
) {
  long jd = getJulianDay(year, month, day);
  long jd0 = getJulianDay(year, 1, 1);
  long jd1 = getJulianDay(year+1, 1, 1);
  long jd2000 = getJulianDay(2000, 1, 1);
  float yearFrac = (float)(jd - jd0)/(jd1-jd0);
  float D = 6.24004077 + 0.01720197 * (jd2000 - jd);
  float solarOffset = -7.659*sin(D) + 9.863*sin(2*D+3.5932);

  // fraction of the day
  float solarNoon = (720-4*longitude-solarOffset+offset_minutes)/1440;
  float solarDeclination = asin(
    sin(-0.409105177) * 
    cos(2*PI*(yearFrac+0.027379257) + 0.0334 * sin(2*PI*(yearFrac-0.005475851)))
  );
  
  float HASunrise = acos(
    cos(1.585334919) / (cos(toRadians(latitude))*cos(solarDeclination)) - tan(toRadians(latitude))*tan(solarDeclination)
  );

  *sunrise = solarNoon - HASunrise/(PI*2);
  *sunset = solarNoon + HASunrise/(PI*2);
  *noon = solarNoon;
}

class TPWSun : public Mode {
  public:
    TPWSun(Core *core) : Mode(core) {
      _configMode = false;
      _state = SUN_ENTER;
      _nextTick = 0;
      _tickState = false;
      _tz_offset_minutes = 0;
      _latitude = {true, 51, 28, 40};
      _longitude = {false, 0, 0, 5};
    }

    void handleLoop() {
      handleLoop(LOOP);
    }
  
    void handleLoop(ModeEvent event) {
      if (event == ENTERED) {
        // compute the solar details only when the mode is entered
        RtcDateTime now = _core->Rtc->GetDateTime();
        int day = now.Day();
        int month = now.Month();
        int year = now.Year();
        getSolarDates(
          year,
          month,
          day,
          getDecimalCoord(_latitude),
          getDecimalCoord(_longitude),
          _tz_offset_minutes,
          &_noon,
          &_sunrise,
          &_sunset
        );
        // Initialize mode
        _state = SUN_ENTER;
        _nextTick = _core->currentMillis + SHOW_SET_TIME;
        _tickState = false;
        _updateDisplay = true;
      }

      if (_configMode) {
        _handleSetLocaleMode(false);
      } else {
        _handleSunMode();
      }
      
    }

  private:
    bool _configMode;
    SunState _state;
    float _noon;
    float _sunrise;
    float _sunset;
    int _tz_offset_minutes; // UTC Offset in minutes for the current time
    Coord _latitude;
    Coord _longitude;    

    // State variables
    unsigned long _nextTick;
    bool _tickState;
    bool _updateDisplay;

    void _handleSunMode() {
      int displayHour;
      int displayMinutes;
    
      if (_core->buttons[BUTTON_MODE].wasLongPressed()) {
        _core->goToBase();
        return;
      }
      if (_core->buttons[BUTTON_MODE].wasReleased()) {
        _core->goToNext();
        return;
      }
    
      if (_core->buttons[BUTTON_SET].wasLongPressed()) {
        _configMode = true;
        _state = SET_LOCALE;
        _handleSetLocaleMode(true);
        return;
      }

      if (_state == SUN_ENTER) {
        if (_nextTick < _core->currentMillis) {
          _state = SUN_NOON;
          _nextTick = _core->currentMillis + SHOW_SET_TIME;
          _tickState = true;
          _updateDisplay = true;
        }
      } else if (_state == SUN_NOON || _state == SUN_RISE || _state == SUN_SET){
        // Cycle through the SUN_NOON SUN_RISE and SUN_SET states
        if (_core->buttons[BUTTON_UP].wasReleased()) {
            _state = (SunState)(((_state+1) % 3) + 1);
            _nextTick = _core->currentMillis + SHOW_SET_TIME;
            _tickState = true;
            _updateDisplay = true;
        }
        if (_core->buttons[BUTTON_DOWN].wasReleased() || _core->buttons[BUTTON_SET].wasReleased()) {
          _state  = (SunState)((_state % 3) + 1);
          _nextTick = _core->currentMillis + SHOW_SET_TIME;
          _tickState = true;
          _updateDisplay = true;
        }
      } else {
        _state = SUN_ENTER;
        _nextTick = _core->currentMillis + SHOW_SET_TIME;
        _tickState = false;
        _updateDisplay = true;
      }

      if (_nextTick < _core->currentMillis) {
        _nextTick = _core->currentMillis + SHOW_SET_TIME;
        _tickState = !_tickState;
        _updateDisplay = true;
      }
      
      // display
      if (_updateDisplay) {
        _updateDisplay = false;
        switch (_state) {
          case SUN_ENTER:
            _core->display->digitValues[0] = 0x6D; //S
            _core->display->digitValues[1] = 0x1C; //u
            _core->display->digitValues[2] = 0x54; //n
            _core->display->digitValues[3] = 0;
            break;
          case SUN_NOON:
            if (_tickState) {
              _core->display->digitValues[0] = 0x54; // n
              _core->display->digitValues[1] = 0x5C; // o
              _core->display->digitValues[2] = 0x5C; // o
              _core->display->digitValues[3] = 0x54; // n
            } else {
              displayHour = (int)(_noon * 24) % 24;
              displayMinutes = ((int)(_noon * 1440) - displayHour*60) % 60;
              setNumberSegs(_core->display->digitValues, displayHour*100 + displayMinutes, -1, true);
            }
            break;
          case SUN_SET:
            if (_tickState) {
              _core->display->digitValues[0] = 0x6D; // S
              _core->display->digitValues[1] = 0x6D; // S
              _core->display->digitValues[2] = 0x79; // E
              _core->display->digitValues[3] = 0x78; // t
            } else {
              displayHour = (int)(_sunset * 24) % 24;
              displayMinutes = ((int)(_sunset * 1440) - displayHour*60) % 60;
              setNumberSegs(_core->display->digitValues, displayHour*100 + displayMinutes, -1, true);
            }
            break;
          case SUN_RISE:
            if (_tickState) {
              _core->display->digitValues[0] = 0x50; // r
              _core->display->digitValues[1] = 0x10; // i
              _core->display->digitValues[2] = 0x6D; // S
              _core->display->digitValues[3] = 0x79; // E
            } else {
              displayHour = (int)(_sunrise * 24) % 24;
              displayMinutes = ((int)(_sunrise * 1440) - displayHour*60) % 60;
              setNumberSegs(_core->display->digitValues, displayHour*100 + displayMinutes, -1, true);
            }
            break;
          default:
            // Something very wrong happenned for this to run
            _configMode = false;
            _state = SUN_ENTER;
            _nextTick = _core->currentMillis + SHOW_SET_TIME;
            _tickState = false;
        }
      }
    }

    void _handleSetLocaleMode(bool start) {
      static int set_tz_offset_hours;
      static byte set_tz_offset_minutes;
      static Coord set_latitude;
      static Coord set_longitude;

      if (_state == SET_LOCALE && start) {
        // Fetch current configuration at the start
        _tickState = false;
        _nextTick = _core->currentMillis + SHOW_SET_TIME;
        _updateDisplay = true;
    
        set_tz_offset_hours = (int)(_tz_offset_minutes/60);
        set_tz_offset_minutes = (byte)(_tz_offset_minutes % 60);
    
        set_latitude.positive = _latitude.positive;
        set_latitude.deg      = _latitude.deg;
        set_latitude.minutes  = _latitude.minutes;
        set_latitude.seconds  = _latitude.seconds;
    
        set_longitude.positive = _longitude.positive;
        set_longitude.deg      = _longitude.deg;
        set_longitude.minutes  = _longitude.minutes;
        set_longitude.seconds  = _longitude.seconds;
      }
    
      if (_core->buttons[BUTTON_MODE].wasLongPressed()) {
        _core->goToBase();
        return;
      }
    
      if (_core->buttons[BUTTON_SET].wasReleased()) {
        // Save changes
        _tz_offset_minutes = set_tz_offset_hours*60 + set_tz_offset_minutes;
    
        _latitude.positive = set_latitude.positive;
        _latitude.deg = set_latitude.deg;
        _latitude.minutes = set_latitude.minutes;
        _latitude.seconds = set_latitude.seconds;
    
        _longitude.positive = set_longitude.positive;
        _longitude.deg = set_longitude.deg;
        _longitude.minutes = set_longitude.minutes;
        _longitude.seconds = set_longitude.seconds;
      
        // Return
        _configMode = false;
        _state = SUN_ENTER;
        return;
      }

      bool modeAction = _core->buttons[BUTTON_MODE].wasReleased();
      bool upAction   = _core->buttons[BUTTON_UP].wasReleased() || _core->buttons[BUTTON_UP].wasLongPressed() || _core->buttons[BUTTON_UP].isRepeating();
      bool downAction = _core->buttons[BUTTON_DOWN].wasReleased() || _core->buttons[BUTTON_DOWN].wasLongPressed() || _core->buttons[BUTTON_DOWN].isRepeating();

      if (_state != SET_LOCALE && (modeAction || upAction || downAction)) {
        _nextTick = _core->currentMillis + BLINK_INTERVAL;
        _tickState = false;
        _updateDisplay = true;      
      }
    
      // Flow control
      switch (_state) {
        case SET_LOCALE:
          if (_nextTick < _core->currentMillis) {
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
            _updateDisplay = true;
            _state = SET_TZ_OFFSET_HOUR;
          }
          break;
        case SET_TZ_OFFSET_HOUR:
          if (modeAction) {
            _state = SET_TZ_OFFSET_MINUTE;
            break;
          }
          if (upAction) {
            set_tz_offset_hours = set_tz_offset_hours + 1;
          }
          if (downAction) {
            set_tz_offset_hours = set_tz_offset_hours - 1;
          }
    
          if (set_tz_offset_hours < -24) {
            set_tz_offset_hours = -24;
          }
          if (set_tz_offset_hours > 24) {
            set_tz_offset_hours = 24;
          }
          break;
        case SET_TZ_OFFSET_MINUTE:
          if (modeAction) {
            _state = SET_LAT_DEG;
            break;
          }
          if (upAction) {
            set_tz_offset_minutes = (set_tz_offset_minutes + 1) % 60;
          }
          if (downAction) {
            set_tz_offset_minutes = (set_tz_offset_minutes + 59) % 60;
          }
          break;    
        case SET_LAT_DEG:
          if (modeAction) {
            _state = SET_LAT_MIN;
            break;
          }
          if (upAction) {
            set_latitude.deg = set_latitude.deg + (set_latitude.positive ? 1 : -1);
          }
          if (downAction) {
            set_latitude.deg = set_latitude.deg + (set_latitude.positive ? -1 : 1);
          }
          if (set_latitude.deg < 0) {
            set_latitude.deg = -set_latitude.deg;
            set_latitude.positive = !set_latitude.positive;
          } else if (set_latitude.deg > 180) {
            set_latitude.deg = 360-set_latitude.deg;
            set_latitude.positive = !set_latitude.positive;        
          }
          break;    
        case SET_LAT_MIN:
          if (modeAction) {
            _state = SET_LAT_SEC;
            break;
          }
          if (upAction) {
            set_latitude.minutes = (set_latitude.minutes + 1) % 60;
          }
          if (downAction) {
            set_latitude.minutes = (set_latitude.minutes + 59) % 60;
          }
          break;
        case SET_LAT_SEC:
          if (modeAction) {
            _state = SET_LONG_DEG;
            break;
          }
          if (upAction) {
            set_latitude.seconds = (set_latitude.seconds+1) % 60;
          }
          if (downAction) {
            set_latitude.seconds = (set_latitude.seconds + 59) % 60;
          }
          break;
        case SET_LONG_DEG:
          if (modeAction) {
            _state = SET_LONG_MIN;
            break;
          }
          if (upAction) {
            set_longitude.deg = set_longitude.deg + (set_longitude.positive ? 1 : -1);
          }
          if (downAction) {
            set_longitude.deg = set_longitude.deg + (set_longitude.positive ? -1 : 1);
          }
          if (set_longitude.deg < 0) {
            set_longitude.deg = -set_longitude.deg;
            set_longitude.positive = !set_longitude.positive;
          } else if (set_longitude.deg > 180) {
            set_longitude.deg = 360-set_longitude.deg;
            set_longitude.positive = !set_longitude.positive;        
          }
          break;    
        case SET_LONG_MIN:
          if (modeAction) {
            _state = SET_LONG_SEC;
            break;
          }
          if (upAction) {
            set_longitude.minutes = (set_longitude.minutes + 1) % 60;
          }
          if (downAction) {
            set_longitude.minutes = (set_longitude.minutes + 59) % 60;
          }
          break;    
        case SET_LONG_SEC:
          if (modeAction) {
            _state = SET_TZ_OFFSET_HOUR;
            break;
          }
          if (upAction) {
            set_longitude.seconds = (set_longitude.seconds+1) % 60;
          }
          if (downAction) {
            set_longitude.seconds = (set_longitude.seconds + 59) % 60;
          }
          break;
        default:
          // Something very wrong happenned for this to run
          _configMode = false;
          _state = SUN_ENTER;
          _nextTick = _core->currentMillis + SHOW_SET_TIME;
          _tickState = false;
          return;
      }

      if (_core->currentMillis > _nextTick) {
        _nextTick = _core->currentMillis + BLINK_INTERVAL;
        _tickState = !_tickState;
        _updateDisplay = true;
      }

      if (_updateDisplay) {
        _updateDisplay = false;
        // Display
        switch (_state) {
          case SET_LOCALE:
            _core->display->digitValues[0] = 0x6D; // S
            _core->display->digitValues[1] = 0x79; // E
            _core->display->digitValues[2] = 0x78; // t
            _core->display->digitValues[3] = 0x00;
            break;
          case SET_TZ_OFFSET_HOUR:
            if (set_tz_offset_hours < 0) {
              _core->display->digitValues[0] = 0x40;
              _core->display->digitValues[1] = digitsSegments[(int)(-set_tz_offset_hours / 10) % 10];
              _core->display->digitValues[2] = digitsSegments[(int)(-set_tz_offset_hours) % 10];
            } else {
              _core->display->digitValues[0] = 0;
              _core->display->digitValues[1] = digitsSegments[(int)(set_tz_offset_hours / 10) % 10];
              _core->display->digitValues[2] = digitsSegments[(int)(set_tz_offset_hours) % 10];
            }
            _core->display->digitValues[3] = 0x74; // h
            if (_tickState) {
               _core->display->digitValues[0] = 0;
               _core->display->digitValues[1] = 0;
               _core->display->digitValues[2] = 0;
            }
            break;
          case SET_TZ_OFFSET_MINUTE:
            _core->display->digitValues[0] = 0;
            _core->display->digitValues[1] = 0;
            _core->display->digitValues[2] = digitsSegments[(int)(set_tz_offset_minutes / 10) % 10];
            _core->display->digitValues[3] = digitsSegments[(int)(set_tz_offset_minutes) % 10];
            if (_tickState) {
               _core->display->digitValues[2] = 0;
               _core->display->digitValues[3] = 0;
            }
            break;
          case SET_LAT_DEG:
            _core->display->digitValues[0] = digitsSegments[(int)(set_latitude.deg / 100) % 10];
            _core->display->digitValues[1] = digitsSegments[(int)(set_latitude.deg / 10) % 10];
            _core->display->digitValues[2] = digitsSegments[(int)(set_latitude.deg) % 10];
      
            if (set_latitude.positive) {
              _core->display->digitValues[3] = 0b00110111; // N
            } else {
              _core->display->digitValues[3] = 0b01101101; // S
            }
      
            if (_tickState) {
               _core->display->digitValues[0] = 0;
               _core->display->digitValues[1] = 0;
               _core->display->digitValues[2] = 0;
            }
            break;
          case SET_LAT_MIN:
            _core->display->digitValues[0] = 0;
            _core->display->digitValues[1] = digitsSegments[(int)(set_latitude.minutes / 10) % 10];
            _core->display->digitValues[2] = digitsSegments[(int)(set_latitude.minutes) % 10];
            _core->display->digitValues[3] = 0b00100000; // '
            if (_tickState) {
               _core->display->digitValues[1] = 0;
               _core->display->digitValues[2] = 0;
            }
            break;
          case SET_LAT_SEC:
            _core->display->digitValues[0] = 0;
            _core->display->digitValues[1] = digitsSegments[(int)(set_latitude.seconds / 10) % 10];
            _core->display->digitValues[2] = digitsSegments[(int)(set_latitude.seconds) % 10];
            _core->display->digitValues[3] = 0b00100010; // "
            if (_tickState) {
               _core->display->digitValues[1] = 0;
               _core->display->digitValues[2] = 0;
            }
            break;
          case SET_LONG_DEG:
            _core->display->digitValues[0] = digitsSegments[(int)(set_longitude.deg / 100) % 10];
            _core->display->digitValues[1] = digitsSegments[(int)(set_longitude.deg / 10) % 10];
            _core->display->digitValues[2] = digitsSegments[(int)(set_longitude.deg) % 10];
            if (set_longitude.positive) {
              _core->display->digitValues[3] = 0b01111001; // E
            } else {
              _core->display->digitValues[3] = 0b00111110; // W
            }
            if (_tickState) {
               _core->display->digitValues[0] = 0;
               _core->display->digitValues[1] = 0;
               _core->display->digitValues[2] = 0;
            }
            break;
          case SET_LONG_MIN:
            _core->display->digitValues[0] = 0;
            _core->display->digitValues[1] = digitsSegments[(int)(set_longitude.minutes / 10) % 10];
            _core->display->digitValues[2] = digitsSegments[(int)(set_longitude.minutes) % 10];
            _core->display->digitValues[3] = 0b00100000; // '
            if (_tickState) {
               _core->display->digitValues[1] = 0;
               _core->display->digitValues[2] = 0;
            }
            break;
          case SET_LONG_SEC:
            _core->display->digitValues[0] = 0;
            _core->display->digitValues[1] = digitsSegments[(int)(set_longitude.seconds / 10) % 10];
            _core->display->digitValues[2] = digitsSegments[(int)(set_longitude.seconds) % 10];
            _core->display->digitValues[3] = 0b00100010; // "
            if (_tickState) {
               _core->display->digitValues[1] = 0;
               _core->display->digitValues[2] = 0;
            }
            break;
          default:
            // Something very wrong happenned for this to run
            _configMode = false;
            _state = SUN_ENTER;
            _nextTick = _core->currentMillis + SHOW_SET_TIME;
            _tickState = false;
            return;
        }
      }    
    }

};

#endif
