#ifndef TPWCOREBUTTONS_H_INCLUDED
#define TPWCOREBUTTONS_H_INCLUDED

#include <Arduino.h>

// ----------------------------------------------------------
//                Button management
// ----------------------------------------------------------

#define DEBOUNCE_TIME                 50
#define REPEATED_EVENT_INTERVAL       300
#define REPEATED_EVENT_FAST_INTERVAL  100
#define REPEATED_EVENT_FAST_COUNT     3
#define LONG_PRESS_DELAY              2000

namespace TPWCore {

class Button {
  public:

    Button(
        const int pin_
    );
    
    void init();

    void updateState(unsigned long currentMillis);

    bool wasReleased();
    bool wasPressed();
    bool isRepeating();
    bool wasLongPressed();
    bool isPressed();
    
  private:
    int pin;
    // Debounce
    unsigned long lastReadChange;
  
    bool previouslyPressed;
    // pressed:
    bool pressed;
    bool pressedEvent; // true only for a single loop when pressed.
    unsigned long pressedEventTime;
    // long pressed:
    bool longPressed; // true only for a single loop after LONG_PRESS_DELAY milliseconds
    bool longPressedEvent;
    // repeated event fired after a long press
    bool repeatedEvent; // true only for a single loop, every REPEATED_EVENT_INTERVAL millisecond
    unsigned long repeatedEventNextTime;
    byte repeats;
};

}

#endif
