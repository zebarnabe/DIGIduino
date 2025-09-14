#include <Arduino.h>
#include "tpwCoreButtons.h"

// ----------------------------------------------------------
//                Button management
// ----------------------------------------------------------

namespace TPWCore {

Button::Button(
    const int pin_
) {
  pin = pin_;
}

void Button::init() {
  pinMode(pin, INPUT);
  lastReadChange = 0;
  previouslyPressed = false;
  pressed = false;
  pressedEvent = false;
  pressedEventTime = 0;
  longPressed = false;
  longPressedEvent = false;
  repeatedEvent = false;
  repeatedEventNextTime = 0;
  repeats = 0;
}

void Button::updateState(unsigned long currentMillis) {
  bool readState;
  // reset event flags.
  pressedEvent = false;
  longPressedEvent = false;
  repeatedEvent = false;

  // We assume pin is active HIGH; adjust if reversed
  readState = (digitalRead(pin) == HIGH);
  // Debounce input
  if (pressed == readState && currentMillis >= lastReadChange + DEBOUNCE_TIME) {
    // button.pressed is stable
    if (!pressed) {
      // pin released, reset flags
      if (previouslyPressed) {
        if (!longPressed) {
          pressedEvent = true;
        }
        previouslyPressed = false;
        pressedEventTime = 0;
        longPressed = false;
        longPressedEvent = false;
        repeatedEvent = false;
        repeatedEventNextTime = 0;
        repeats = 0;
      }

    } else {
      // Button is pressed!
      if (!previouslyPressed) {
        // was just pressed.
        previouslyPressed = true;
        pressedEvent = true;
        pressedEventTime = currentMillis;

      } else if (!longPressed && currentMillis >= pressedEventTime + LONG_PRESS_DELAY) {
        longPressed = true;
        longPressedEvent = true;
        repeatedEventNextTime = currentMillis + REPEATED_EVENT_INTERVAL;
      
      } else if (longPressed && currentMillis >= repeatedEventNextTime) {
        repeatedEvent = true;
        if (repeats < REPEATED_EVENT_FAST_COUNT) {
          repeatedEventNextTime = currentMillis + REPEATED_EVENT_INTERVAL;
          repeats++;
        } else {
          repeatedEventNextTime = currentMillis + REPEATED_EVENT_FAST_INTERVAL;
        }
      }
    }
  } else {
    // readState changed
    if (pressed != readState) {
      pressed = readState;
      lastReadChange = currentMillis;
    }
  }
}

bool Button::wasReleased() {
  return pressedEvent && !pressed;
}

bool Button::wasPressed() {
  return pressedEvent && pressed;
}

bool Button::isRepeating() {
  return repeatedEvent;  
}

bool Button::wasLongPressed() {
  return longPressedEvent;
}

bool Button::isPressed() {
  return pressed;
}

}
