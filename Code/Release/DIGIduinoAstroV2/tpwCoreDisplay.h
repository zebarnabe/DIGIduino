#ifndef SEGDISPLAY_H_INCLUDED
#define SEGDISPLAY_H_INCLUDED

#include <Arduino.h>

#ifndef NUM_DIGITS
#define NUM_DIGITS 4
#endif

// Hardcoded to 8 segments
#define NUM_SEGMENTS 8

#define COMMON_CATHODE 0
#define COMMON_ANODE 1
#define N_TRANSISTORS 2
#define P_TRANSISTORS 3
#define NP_COMMON_CATHODE 1
#define NP_COMMON_ANODE 0

// microseconds, full display with 8ms period is ~120Hz (note that the updates run with each refresh() call)
// If there are inconsistencies in the brightness, aside from bySegments parameter, you can try to increase this value.
// Please note that with big values can cause flickering, specially when brightness is set to a low value.
#ifndef ITERATION_LENGTH
#define ITERATION_LENGTH 1
#endif

#define SHOW_SET_TIME 2000
#define BLINK_INTERVAL 300

namespace TPWCore {

  // LUTs
const byte digitsSegments[10] = {
  // GFEDCBA  Segments      7-segment map:
  0b00111111, // 0   "0"          AAA
  0b00000110, // 1   "1"         F   B
  0b01011011, // 2   "2"         F   B
  0b01001111, // 3   "3"          GGG
  0b01100110, // 4   "4"         E   C
  0b01101101, // 5   "5"         E   C
  0b01111101, // 6   "6"          DDD
  0b00000111, // 7   "7"
  0b01111111, // 8   "8"
  0b01101111, // 9   "9"
};

void setNumberSegs(uint8_t *segs, long number, int decimal, bool padZeros);

// ///////////////////////////////////////////////////////////////////////////////

class Display {

  public:
      uint8_t *digitValues;

      /**
       * Display class
       * 
       * Controls seven segment display output.
       * 
       * Arguments:
       * hardwareConfig:   one of COMMON_CATHODE, COMMON_ANODE, N_TRANSISTORS, P_TRANSISTORS, NP_COMMON_CATHODE, NP_COMMON_ANODE
       * numDigits:        Number of digits in the display
       * digitPins:        Arduino pins connected to the digits pins of the segment display
       * segmentPins:      Arduino pins connected to the segments pins of the segment display. Note that 8 pins are expected.
       * digitValue:       Pointer for an byte array with numDigits elements with the digit information to display.
       * idxDurations:     Pointer for a long array with 8/4 elements with the segment/digit timing information
       * bySegments:       Boolean with iteration strategy, true means by segment, usually associated with resitors in digit pins.
       */
      Display(
        uint8_t hardwareConfig,
        uint8_t numDigits,
        const uint8_t *digitPins,
        const uint8_t *segmentPins,
        uint8_t *digitValuesIn,
        int32_t *idxDurations,
        bool bySegments  
      );

      /**
       * Initialization assigns and sets pins
       * 
       * Should be called in setup() call
       */
      void init();

      // Should be called at every main loop call
      void refresh();

      /**
       * Set the brightness of the whole display:
       * 
       * Note that at low values it might cause some flickering.
       * 
       * Arguments:
       * brightness:   float with values from 0.0 to 100.0
       */
      void setBrightness(float brightness);

      /**
       * Blanks the screen
       * 
       * Sets all associated pins to off.
       * Sets all the associated values to off.
       */
      void blank();

  private:
      void _segmentsOff(uint8_t segmentNum);
      void _segmentsOn(uint8_t segmentNum);
      void _digitsOff(uint8_t digitNum);
      void _digitsOn(uint8_t digitNum);
      void _modulateBrightness();

      // Configuration
      uint8_t _numDigits;
      uint8_t _digitOnVal, _digitOffVal, _segmentOnVal, _segmentOffVal;

      float _brightness; // total duty cycle

      // iterate over the segments if true, over the digits if false.
      // If current limiting resistors are on the digit pins set to true to avoid brightness variations.
      // If true the number of iterations is defined by the number of segments.
      // If false the number of iterations is defined by the number of digits.
      bool _bySegments;

      // External pointers
      const uint8_t *_digitPins;
      const uint8_t *_segmentPins; // Always 8 segments in the current implementation
      int32_t *_idxDurations;  // Iteration jitter compensation
      
      // State variables
      uint8_t _Idx;           // Index (digit/segment) for the display update
      uint32_t _prevTime;     // Last update time

      // Brightness modulation
      bool _offState;   // Internal control for when to keep the leds on or off
      uint32_t _onSum;  // Duration cumulative sum for on time
      uint32_t _offSum; // Duration cumulative sum for off time
      
};

}

#endif // SEGDISPLAY_H_INCLUDED
