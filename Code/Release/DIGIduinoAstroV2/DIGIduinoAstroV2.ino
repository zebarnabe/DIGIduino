/* The Printable Watch 2025 - Example
  theprintablewatch.com

  By José Sousa
  Based on:
  https://github.com/theprintablewatch/DIGIduino/
  https://github.com/DeanIsMe/SevSeg/
 */

#include "RtcDS1302.h"
#include "tpwCore.h"
#include "tpwCoreDisplay.h"
#include "tpwTimeMode.h"
#include "tpwChronoMode.h"
#include "tpwMoonMode.h"
#include "tpwSunMode.h"

using namespace TPWCore;

// 2  Wake-up button (external interrupt), Up button - top right
// 19 Mode button - bottom left
// 18 Down button - bottom right
// 0  Set button (held 2 seconds) - top left

#define BUTTON_UP_PIN 2
#define BUTTON_MODE_PIN 19
#define BUTTON_DOWN_PIN 18
#define BUTTON_SET_PIN 0

#define RTC_DATA 14
#define RTC_SCLK 15
#define RTC_CE 16
#define RTC_WAKE 17

#define NUM_DIGITS 4

#define DIGIT_1 1
#define DIGIT_2 3
#define DIGIT_3 4
#define DIGIT_4 5

#define SEG_A 6
#define SEG_B 7
#define SEG_C 8
#define SEG_D 9
#define SEG_E 10
#define SEG_F 11
#define SEG_G 12
#define SEG_DP 13


// Change the RtcDS1302 definition in the read Arduino...
// ------------------ RTC Setup ------------------
ThreeWire myWire(RTC_DATA, RTC_SCLK, RTC_CE);  // IO (DATA), SCLK (CLK), CE (RST)
RtcDS1302<ThreeWire> Rtc(myWire);

// ------------------ Seven-Segment Display Setup ------------------

const static byte digitPins[] = { DIGIT_1, DIGIT_2, DIGIT_3, DIGIT_4 };
const static byte segmentPins[] = { SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F, SEG_G, SEG_DP };

byte digValues[4] = {0, 0, 0, 0};
long idxDurations[8] = {0, 0, 0, 0, 0, 0, 0, 0};

Display segDisplay = Display(
    COMMON_CATHODE,  // Hardware config (COMMON_CATHODE - digits pins are LOW and segments HIGH for segments to light up)
    NUM_DIGITS,       // Number of digits
    digitPins,       // Digit pins
    segmentPins,     // Segment pins
    digValues,       // values for each digit, mapped as A-B-C-D-E-F-G-DP
    idxDurations,    // jitter compensation tracking
    true             // Iterate over the segments when using the display: true → Resistors on digit pins - makes brightness more uniform
);

// ------------------ Buttons ------------------
Button buttons[4] = {
  {BUTTON_SET_PIN},
  {BUTTON_MODE_PIN},
  {BUTTON_DOWN_PIN},
  {BUTTON_UP_PIN}
};

// //////////////////////////////////////////////////////////////

Core core = Core(buttons, &segDisplay, &Rtc, RTC_WAKE, BUTTON_UP_PIN);
TPWTime timeMode = TPWTime(&core);
TPWChrono chronoMode = TPWChrono(&core);
TPWMoon moonMode = TPWMoon(&core);
TPWSun sunMode = TPWSun(&core);

// //////////////////////////////////////////////////////////////

void setup() {
  // Chain TPW modes
  core.setInitialMode(&timeMode);
  timeMode.setNextMode(&chronoMode);
  chronoMode.setNextMode(&moonMode);
  moonMode.setNextMode(&sunMode);

  // Core init - Set initialize buttons, display and set RTC wake pins
  core.init();

  // ------------------ RTC Setup ------------------
  
  // Set the RTC to compile time ONCE at startup (remove if not desired):
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  Rtc.SetDateTime(compiled);

  // ------------------ RTC Setup ------------------
  // Disable any unused pins
  pinMode(20, OUTPUT);
  pinMode(21, OUTPUT);
  pinMode(22, OUTPUT);
}

void loop() {
  core.handleLoop();
}
