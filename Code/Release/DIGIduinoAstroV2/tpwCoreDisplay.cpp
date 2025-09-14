#include "tpwCoreDisplay.h"

namespace TPWCore {

void setNumberSegs(uint8_t *segs, long number, int decimal, bool padZeros) {
  long truncated;
  long digit;

  bool last = false;

  truncated = number;
  for (int i=0; i<NUM_DIGITS; i++) {
    if (last && !padZeros && (decimal == -1 || decimal < i)) {
      segs[NUM_DIGITS-i-1] = 0;
    } else {
      digit = truncated % 10;
      truncated = (long)(truncated / 10);
      segs[NUM_DIGITS-i-1] = digitsSegments[digit];
      if (truncated == 0) {
        last = true;
      }
    }
    if (decimal == i) {
      segs[NUM_DIGITS-i-1] = segs[NUM_DIGITS-i-1] | 0x80;
    }
  }
}

Display::Display(
  uint8_t hardwareConfig,
  uint8_t numDigits,
  const uint8_t *digitPins,
  const uint8_t *segmentPins,
  uint8_t *digitValuesIn,
  int32_t *idxDurations,
  bool bySegments
) {
  _Idx=0;
  _prevTime=0;

  _brightness = 1;
  
  _offState = true;
  _onSum = 0;
  _offSum = 0;

  switch (hardwareConfig) {

    case 0: // Common cathode
      _digitOnVal = LOW;
      _segmentOnVal = HIGH;
      break;

    case 1: // Common anode
      _digitOnVal = HIGH;
      _segmentOnVal = LOW;
      break;

    case 2: // With active-high, low-side switches (most commonly N-type FETs)
      _digitOnVal = HIGH;
      _segmentOnVal = HIGH;
      break;

    case 3: // With active low, high side switches (most commonly P-type FETs)
      _digitOnVal = LOW;
      _segmentOnVal = LOW;
      break;
  }

  // define the Off-Values depending on the On-Values
  if (_digitOnVal == HIGH){
    _digitOffVal = LOW;
  } else {
    _digitOffVal = HIGH;
  }
  // define the Off-Values depending on the On-Values
  if (_segmentOnVal == HIGH){
    _segmentOffVal = LOW;
  } else {
    _segmentOffVal = HIGH;
  }

  _bySegments = bySegments;
  _digitPins = digitPins;
  _segmentPins = segmentPins;
  _numDigits = numDigits;
  _idxDurations = idxDurations;
  digitValues = digitValuesIn;
  
}

void Display::init() {
  // Set the pins as outputs, and turn them off
  for (uint8_t digit = 0 ; digit < _numDigits ; digit++) {
    pinMode(_digitPins[digit], OUTPUT);
    digitalWrite(_digitPins[digit], _digitOffVal);
  }

  for (uint8_t segmentNum = 0 ; segmentNum < NUM_SEGMENTS; segmentNum++) {
    pinMode(_segmentPins[segmentNum], OUTPUT);
    digitalWrite(_segmentPins[segmentNum], _segmentOffVal);
  }
}

void Display::setBrightness(float brightness) {
  _brightness = constrain(brightness, 0.0, 100.0) / 100;
}


void Display::_modulateBrightness() {
  uint32_t totals;
  int32_t error;

  totals = _offSum + _onSum;

  error = _brightness * totals - _onSum;

  if (error > 0) {
      _offState = false;
  } else if (error < 0){
      _offState = true;
  }

  // Prevent overflows at the cost of precision
  if (totals > 100000 ) {
      _onSum = _onSum / (totals >> 3);
      _offSum = _offSum / (totals >> 3);
  }
}

void Display::refresh() {

  uint32_t us = micros();
  uint32_t duration = (us - _prevTime);
  _prevTime = us;

  if (_offState) {
      _offSum = duration + _offSum;
  } else {
      _onSum = duration + _onSum;
  }

  _idxDurations[_Idx] = _idxDurations[_Idx] + duration;
  if (_idxDurations[_Idx] >= ITERATION_LENGTH) {
    // Iterate to a new digit/segment
    _idxDurations[_Idx] = _idxDurations[_Idx] - ITERATION_LENGTH;
    
    if (_bySegments) {
      // Turn current segment off
      _segmentsOff(_Idx);
      
      // Iterate over the segments
      _Idx = (_Idx + 1) % NUM_SEGMENTS;

      // We are starting a new display frame
      if (_Idx == 0) {
        _modulateBrightness();
      }

      if (!_offState) {
        _segmentsOn(_Idx);
      }
    } else {
      // Turn current digit off
      _digitsOff(_Idx);

      // Iterate over the digits
      _Idx = (_Idx + 1) % _numDigits;

      // We are starting a new display frame
      if (_Idx == 0) {
        _modulateBrightness();
      }

      if (!_offState) {
        _digitsOn(_Idx);
      }
    }
  }
}

void Display::blank() {
  for (uint8_t digitNum = 0 ; digitNum < _numDigits ; digitNum++) {
    digitValues[digitNum] = 0;
    digitalWrite(_digitPins[digitNum], _digitOffVal);
  }
  for (uint8_t segmentNum = 0 ; segmentNum < NUM_SEGMENTS ; segmentNum++) {
    _idxDurations[segmentNum] = 0;
    digitalWrite(_segmentPins[segmentNum], _segmentOffVal);
  }
}

// Turns the segments off accross all digits
void Display::_segmentsOff(uint8_t segmentNum) {
  for (uint8_t digitNum = 0 ; digitNum < _numDigits ; digitNum++) {
    digitalWrite(_digitPins[digitNum], _digitOffVal);
  }
  digitalWrite(_segmentPins[segmentNum], _segmentOffVal);
}

// Turns the segments on accross all digits, according to the digitValues
void Display::_segmentsOn(uint8_t segmentNum) {
  digitalWrite(_segmentPins[segmentNum], _segmentOnVal);
  for (uint8_t digitNum = 0 ; digitNum < _numDigits ; digitNum++) {
    if (digitValues[digitNum] & (1 << segmentNum)) { // Check a single bit
      digitalWrite(_digitPins[digitNum], _digitOnVal);
    }
  }
}

void Display::_digitsOff(uint8_t digitNum) {
  for (uint8_t segmentNum = 0 ; segmentNum < NUM_SEGMENTS ; segmentNum++) {
    digitalWrite(_segmentPins[segmentNum], _segmentOffVal);
  }
  digitalWrite(_digitPins[digitNum], _digitOffVal);
}

void Display::_digitsOn(uint8_t digitNum) {
  digitalWrite(_digitPins[digitNum], _digitOnVal);
  for (uint8_t segmentNum = 0 ; segmentNum < NUM_SEGMENTS ; segmentNum++) {
    if (digitValues[digitNum] & (1 << segmentNum)) {
      digitalWrite(_segmentPins[segmentNum], _segmentOnVal);
    }
  }
}

}
