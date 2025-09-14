#ifndef TPWCHRONOMODE_H_INCLUDED
#define TPWCHRONOMODE_H_INCLUDED

#include "tpwCore.h"

#define SPLIT_BLINK_INTERVAL 2000

using namespace TPWCore;

// ///////////////////// Chronograph mode //////////////////////

enum ChronoState {
  CHRONO_ENTER,
  CHRONO_STOP,
  CHRONO_RUN,
  CHRONO_SPLIT,
};

class TPWChrono : public Mode {
  public:  
    TPWChrono(Core *core) : Mode(core) {
      _state = CHRONO_ENTER;
      _chronoStart = -1;
      _chronoSplit = 0;
      _chronoTotal = 0; 
    }

    void handleLoop() {
      handleLoop(LOOP);
    }
  
    void handleLoop(ModeEvent event) {
      if (event == ENTERED) {
        // Initialize mode
        // Initialize mode
        _nextTick = _core->currentMillis + 1000;
        _tickState = false;

        _state = CHRONO_ENTER;
        _viewMinutes = false;
      }
      _handleChronoMode();
    }

  private:
    ChronoState _state;
    long _chronoStart;
    long _chronoSplit;
    long _chronoTotal;
    bool _viewMinutes;
    // State variables
    unsigned long _nextTick;
    bool _tickState;


    void _handleChronoMode() {
      long cents;
      long minutes;
          
      if (_core->buttons[BUTTON_MODE].wasLongPressed()) {
        _core->goToBase();
        return;
      }
      if (_core->buttons[BUTTON_MODE].wasReleased()) {
        _core->goToNext();
        return;
      }
    
      if (_core->buttons[BUTTON_UP].wasReleased()) {
        _viewMinutes = !_viewMinutes;
      }
    
      // Input handling
      switch (_state) {
        case CHRONO_ENTER:
          if (_nextTick < _core->currentMillis) {
            _tickState = false;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _viewMinutes=false;
            _state = CHRONO_STOP;
          }
          break;
        case CHRONO_STOP:
          if (_core->buttons[BUTTON_SET].wasPressed()) {
            // Reset
            _chronoStart = -1;
            _chronoSplit = 0;
            _chronoTotal = 0;
          } else if (_core->buttons[BUTTON_DOWN].wasPressed()) {
            // Run
            _chronoStart = _core->currentMillis;
            _state = CHRONO_RUN;
          }
          break;
        case CHRONO_RUN:
          if (_core->buttons[BUTTON_SET].wasPressed()) {
            // Split
            _chronoSplit = _core->currentMillis - _chronoStart + _chronoTotal;
            _nextTick = _core->currentMillis + BLINK_INTERVAL;
            _tickState = false;
            _state = CHRONO_SPLIT;
          } else if (_core->buttons[BUTTON_DOWN].wasPressed()) {
            // Stop
            _chronoTotal = _core->currentMillis - _chronoStart + _chronoTotal;
            _state = CHRONO_STOP;
          }
          break;
        case CHRONO_SPLIT:
          if (_core->buttons[BUTTON_SET].wasPressed()) {
            // Run (continue)
            _state = CHRONO_RUN;
          } else if (_core->buttons[BUTTON_DOWN].wasPressed()) {
            // Stop
            _chronoTotal = _core->currentMillis - _chronoStart + _chronoTotal;
            _chronoSplit = 0;
            _state = CHRONO_STOP;
          }
          break;
      }
    
      // Display handling
      switch (_state) {
        case CHRONO_ENTER:
          // Chrn
          _core->display->digitValues[0] = 0x39;
          _core->display->digitValues[1] = 0x74;
          _core->display->digitValues[2] = 0x50;
          _core->display->digitValues[3] = 0x54;
          break;
        case CHRONO_STOP:
          cents = (long)(_chronoTotal / 10);
          minutes = (long)(cents / 6000);
          cents = cents - minutes * 6000;
    
          if (_viewMinutes) {
            setNumberSegs(_core->display->digitValues, minutes, -1, false);
          } else {
            setNumberSegs(_core->display->digitValues, cents, 2, false);
          }
          break;
        case CHRONO_RUN:
          cents = (long)((_core->currentMillis - _chronoStart + _chronoTotal) / 10);     
          minutes = (long)(cents / 6000);
          cents = cents - minutes * 6000;
          
          if (_viewMinutes) {
            setNumberSegs(_core->display->digitValues, minutes, -1, false);
          } else {
            setNumberSegs(_core->display->digitValues, cents, 2, minutes > 0);
          }
          break;
        case CHRONO_SPLIT:
          if (_core->currentMillis > _nextTick) {
            if (_tickState) {
              _nextTick = _core->currentMillis + SPLIT_BLINK_INTERVAL;
            } else {
              _nextTick = _core->currentMillis + BLINK_INTERVAL;
            }
            _tickState = !_tickState;
          }
    
          cents = (long)(_chronoSplit / 10);
          minutes = (long)(cents / 6000);
          cents = cents - minutes * 6000;
    
          if (_tickState) {
            //SPLt
            _core->display->digitValues[0] = 0x6D;
            _core->display->digitValues[1] = 0x73;
            _core->display->digitValues[2] = 0x38;
            _core->display->digitValues[3] = 0x78;
          } else {
            if (_viewMinutes) {
              setNumberSegs(_core->display->digitValues, minutes, -1, false);
            } else {
              setNumberSegs(_core->display->digitValues, cents, 2, minutes > 0);
            }
          }
          break;
      }
    }

};

#endif
