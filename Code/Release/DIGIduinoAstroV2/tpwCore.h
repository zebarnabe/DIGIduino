#ifndef TPWCORE_H_INCLUDED
#define TPWCORE_H_INCLUDED

#include <Arduino.h>
#include "RtcDS1302.h"
#include "tpwCoreButtons.h"
#include "tpwCoreDisplay.h"

/**
 * TPWCore
 *   Buttons
 *   Display
 *   Mode
 */

#ifndef NUM_BUTTONS
// How many buttons are supported
#define NUM_BUTTONS 4
// Default button indices
#define BUTTON_SET 0
#define BUTTON_MODE 1
#define BUTTON_DOWN 2
#define BUTTON_UP 3
#endif

#ifndef MAX_NUM_MODES
#define MAX_NUM_MODES 16
#endif

// In percentage
#ifndef DEFAULT_BRIGHTNESS
#define DEFAULT_BRIGHTNESS 50.0
#endif

// In seconds
#ifndef DEFAULT_AWAKE_INTERVAL
#define DEFAULT_AWAKE_INTERVAL 8
#endif

namespace TPWCore {

enum ModeEvent {
  ENTERED,
  LOOP
};

// Forward declaration
class Core;

/**
 * TPW Mode controls
 * 
 * Constructor:
 * TPWMode(TPWCore core)
 * 
 *   TPWCore core is the core that manages the hardware facing interface (display and buttons)
 * 
 * Methods:
 *   handleLoop();
 *   setPrevMode(TPWMode mode);
 *   setNextMode(TPWMode mode);
 *   setBaseMode(TPWMode mode);
 */

class Mode {
  public:
    Mode *next;

    Mode(Core *core) {
      _core = core;
      next = 0;
    };
    Mode(Core *core, Mode *nextIn) {
      _core = core;
      next = nextIn;
    };
    void setNextMode(Mode *nextIn) {
      next = nextIn;
    };

    void handleLoop() {
      handleLoop(LOOP);
    };
    virtual void handleLoop(ModeEvent event);

  protected:
    Core *_core;
};

static byte _rtcWakePin;     // RtcDS1302 wake pin
static byte _buttonWakePin;  // Arduino input wake pin

static void _isrWake() {
  // This is triggered by BUTTON_UP_PIN falling
  // lastInteraction = millis();
  digitalWrite(_rtcWakePin, HIGH);
}

class Core {
  public:
    // Public variables
    unsigned long currentMillis;
    Button *buttons;
    Display *display;
    RtcDS1302<ThreeWire>* Rtc;
    bool dateFormat;

    byte awakeInterval;

    // Public methods
    Core(Button *buttonsIn, Display *displayIn, RtcDS1302<ThreeWire>* RtcIn, byte rtcWakePin, byte buttonWakePin) {
      buttons = buttonsIn;
      display = displayIn;
      Rtc = RtcIn;
      dateFormat = true;   // Note: Initialization hardcoded to use dd/mm
      awakeInterval = DEFAULT_AWAKE_INTERVAL;
      _brightness = DEFAULT_BRIGHTNESS;
      _rtcWakePin = rtcWakePin;
      _buttonWakePin = buttonWakePin;
    }

    void setInitialMode(Mode *initialMode) {
      _base = initialMode;
      _mode = 0;
      _targetMode = initialMode;
    }

    void init() {
      // Initialize button pins and state
      for (byte i=0; i<NUM_BUTTONS; i++) {
        buttons[i].init();
      }

      display->init();
      // Set brightness
      display->setBrightness(DEFAULT_BRIGHTNESS);

      // ------------------ Pin Modes ------------------
      pinMode(_rtcWakePin, OUTPUT);

      // Attach interrupt for wake button
      attachInterrupt(digitalPinToInterrupt(_buttonWakePin), TPWCore::_isrWake, FALLING);
    }

    void handleLoop() {
      currentMillis = millis();
      display->refresh();

      for (byte i=0; i<NUM_BUTTONS; i++) {
        buttons[i].updateState(currentMillis);
      }

      if (_mode != _targetMode) {
        _mode = _targetMode;
        _mode->handleLoop(ENTERED);
        
      } else if (_mode != 0) {
        _mode->handleLoop(LOOP);
      } else {
        display->digitValues[0] = 0b01111001;
        display->digitValues[1] = 0b01010000;
        display->digitValues[2] = 0b01010000;
        display->digitValues[3] = 0;
      }
    }

    void goToBase() {
      _targetMode = _base;
    }

    void goToNext() {
      if (_mode->next != 0) {
        _targetMode = _mode->next;
      } else {
        _targetMode = _base;
      }
    }

    void goToSleep() {
      // Reset mode - Akin to go into a undefined mode, when it awakes it goes back to where it was.
      _targetMode = _mode;
      _mode = 0;
      
      // Blank display
      display->blank();

      // Tell teh RTC to go to sleep, keep just track of time...
      digitalWrite(_rtcWakePin, LOW);
    
      // Example low-level sleep (on AVR):
      ADCSRA &= ~(1 << 7);  // Disable ADC
      SMCR |= (1 << 2);     // Power-down mode bit
      SMCR |= 1;            // Enable sleep
      MCUCR |= (3 << 5);    // BOD disable (bits 5 & 6)
      MCUCR = (MCUCR & ~(1 << 5)) | (1 << 6);
      __asm__ __volatile__("sleep");

    }

    float getBrightness() {
      return _brightness;
    }

    void setBrightness(float brightness) {
      _brightness = brightness;

      if (_brightness < 10) {
        _brightness = 10.0;
      }
      if (_brightness > 100) {
        _brightness = 100.0; 
      }
      display->setBrightness(_brightness);
    }

  private:
    float _brightness;

    Mode *_base;        // Base, initial mode (Time mode)
    Mode *_mode;        // Current mode
    Mode *_targetMode;  // Target mode to go to in next loop

};

}

#endif
