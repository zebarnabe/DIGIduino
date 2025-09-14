#ifndef TPWMOONMODE_H_INCLUDED
#define TPWMOONMODE_H_INCLUDED

#include "tpwCore.h"
#include "tpwTimeCommons.h"

#define SPLIT_BLINK_INTERVAL 2000

using namespace TPWCore;

// ///////////////////// Chronograph mode //////////////////////

// ------------------- Moon Phases ------------------
const byte moonPhases[8][4] = {
  { 0x80, 0x80, 0x80, 0x80 },  // 0: New Moon
  { 0x00, 0x00, 0x00, 0x0F },  // 1: Waxing Crescent
  { 0x00, 0x00, 0x39, 0x0F },  // 2: First Quarter
  { 0x00, 0x39, 0x09, 0x0F },  // 3: Waxing Gibbous
  { 0x39, 0x09, 0x09, 0x0F },  // 4: Full Moon
  { 0x39, 0x09, 0x0F, 0x00 },  // 5: Waning Gibbous
  { 0x39, 0x0F, 0x00, 0x00 },  // 6: Last Quarter
  { 0x39, 0x00, 0x00, 0x00 }   // 7: Waning Crescent
};

enum MoonState {
  MOON_PHASE = 0,
  MOON_FULL = 1,
  MOON_NEW = 2
};

byte getMoonPhase(int year, byte month, byte day) {
  long jd = getJulianDay(year, month, day);

  // Moon age in days (modulo synodic month)
  float synodicMonth = 29.53058867;
  // Days since known new moon on 2000 Jan 6 at 18:14 UTC (JD = 2451550.1)
  float daysSinceNew = jd - 2451550.1;

  float age = fmod(daysSinceNew, synodicMonth);
  if (age < 0) {
    age += synodicMonth;
  }

  // Convert age to phase index 0–7
  byte index = (byte)((age / synodicMonth) * 8 + 0.5);  // round to nearest
  return index & 7;                                     // wrap to 0–7
}

void getNextMoonPhase(byte phase, int year, byte month, byte day, int* new_year, byte* new_month, byte* new_day) {
  long jd = getJulianDay(year, month, day);

  // Moon age in days (modulo synodic month)
  float synodicMonth = 29.53058867;
  // Days since known new moon on 2000 Jan 6 at 18:14 UTC (JD = 2451550.1)
  float daysSinceLast = jd - 2451550.1 + synodicMonth * phase / 8 ;

  float age = fmod(daysSinceLast, synodicMonth);
  if (age < 0) {
    age += synodicMonth;
  }
  
  if ((synodicMonth - age) < 2) {
    // new moon is basically happenning now, predict the next one...
    getGregorianDate(jd + synodicMonth - age + synodicMonth, new_year, new_month, new_day);
  } else {
    getGregorianDate(jd + synodicMonth - age, new_year, new_month, new_day);
  }
}

void getNewMoon(int year, byte month, byte day, int* new_year, byte* new_month, byte* new_day) {
  getNextMoonPhase(0, year, month, day, new_year, new_month, new_day);
}

void getFullMoon(int year, byte month, byte day, int* new_year, byte* new_month, byte* new_day) {
  getNextMoonPhase(4, year, month, day, new_year, new_month, new_day);
}


class TPWMoon : public Mode {
  public:
    TPWMoon(Core *core) : Mode(core) {
      _state = MOON_PHASE;
      _moonPhase = 0;
      _new_year = 2000;
      _new_month = 1;
      _new_day = 1;
      _full_year = 2000;
      _full_month = 1;
      _full_day = 1;
      // State variables
      _nextTick = 0;
      _tickState = false;
    }

    void handleLoop() {
      handleLoop(LOOP);
    }
  
    void handleLoop(ModeEvent event) {
      if (event == ENTERED) {
        // Compute values when entering the mode
        RtcDateTime now = _core->Rtc->GetDateTime();
        int day = now.Day();
        int month = now.Month();
        int year = now.Year();
        _moonPhase = getMoonPhase(year, month, day);
        
        getNewMoon(year, month, day, &_new_year, &_new_month, &_new_day);
        getFullMoon(year, month, day, &_full_year, &_full_month, &_full_day);

        // Initialize mode
        _nextTick = _core->currentMillis + SHOW_SET_TIME;
        _tickState = true;
        _updateDisplay = true;
        _state = MOON_PHASE;
      }
      _handleMoonMode();
    }

  private:
    MoonState _state;
   
    byte _moonPhase;
    int _new_year;
    byte _new_month;
    byte _new_day;
    int _full_year;
    byte _full_month;
    byte _full_day;
    // State variables
    unsigned long _nextTick;
    bool _tickState;
    bool _updateDisplay;

    void _handleMoonMode() {
      int dateDisplay;
    
      if (_core->buttons[BUTTON_MODE].wasLongPressed()) {
        _core->goToBase();
        return;
      }
    
      if (_core->buttons[BUTTON_MODE].wasReleased()) {
        _core->goToNext();
        return;
      }

      // Cycle through the views when UP or DOWN (or SET) are pressed
      if (_core->buttons[BUTTON_UP].wasReleased()) {
        _state = (MoonState)((_state + 2) % 3);
        _nextTick = _core->currentMillis + SHOW_SET_TIME;
        _tickState = true;
        _updateDisplay = true;
      }
      else if (_core->buttons[BUTTON_DOWN].wasReleased() || _core->buttons[BUTTON_SET].wasReleased()) {
        _state = (MoonState)((_state + 1) % 3);
        _nextTick = _core->currentMillis + SHOW_SET_TIME;
        _tickState = true;
        _updateDisplay = true;
      }
      
      if (_nextTick < _core->currentMillis) {
        _nextTick = _core->currentMillis + SHOW_SET_TIME;
        _tickState = !_tickState;
        _updateDisplay = true;
      }

      // Display logic
      if (_updateDisplay) {
        _updateDisplay = false;
        switch (_state) {
          case MOON_PHASE:
            if (_tickState) {
              _core->display->digitValues[0] = 0x37; // M
              _core->display->digitValues[1] = 0x5C; // o
              _core->display->digitValues[2] = 0x5C; // o
              _core->display->digitValues[3] = 0x54; // n
            } else {
              _core->display->digitValues[0] = moonPhases[_moonPhase][0];
              _core->display->digitValues[1] = moonPhases[_moonPhase][1];
              _core->display->digitValues[2] = moonPhases[_moonPhase][2];
              _core->display->digitValues[3] = moonPhases[_moonPhase][3];
            }
            break;
          case MOON_FULL:
            if (_tickState) {
              _core->display->digitValues[0] = 0x71; // F
              _core->display->digitValues[1] = 0x1C; // u
              _core->display->digitValues[2] = 0x30; // l
              _core->display->digitValues[3] = 0x30; // l
            } else {
              if (_core->dateFormat == true) {
                dateDisplay = (_full_day * 100) + _full_month;
              } else {
                dateDisplay = (_full_month * 100) + _full_day;
              }
              setNumberSegs(_core->display->digitValues, dateDisplay, 2, true);
            }
            break;
          case MOON_NEW:
            if (_tickState) {
              _core->display->digitValues[0] = 0x37; // N
              _core->display->digitValues[1] = 0x79; // E
              _core->display->digitValues[2] = 0x3E; // W
              _core->display->digitValues[3] = 0; // 
            } else {
              if (_core->dateFormat == true) {
                dateDisplay = (_new_day * 100) + _new_month;
              } else {
                dateDisplay = (_new_month * 100) + _new_day;
              }
              setNumberSegs(_core->display->digitValues, dateDisplay, 2, true);
            }
            break;
        }
      }
    }

};

#endif
