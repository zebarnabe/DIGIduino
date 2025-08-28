/* The Printable Watch 2025 - Example
  theprintablewatch.com

  By José Sousa
  Based on:
  https://github.com/theprintablewatch/DIGIduino/
  https://github.com/DeanIsMe/SevSeg/
 */

#define MAXNUMDIGITS 4
#include "SevSeg.h"
#include "RtcDS1302.h"

// ------------------ RTC Setup ------------------
ThreeWire myWire(14, 15, 16);  // IO (DATA), SCLK (CLK), CE (RST)
RtcDS1302<ThreeWire> Rtc(myWire);

// ------------------ SevSeg Setup ------------------
SevSeg sevseg;

// ------------------ Defaults ------------------
#define DEFAULT_BRIGHTNESS 50
#define DEFAULT_AWAKE_INTERVAL 8000
#define DEBOUNCE_TIME 50

// ------------------ Times & Delays ------------------
unsigned long wakeInterval                  = DEFAULT_AWAKE_INTERVAL; // Display active for 8s if no further interaction

const unsigned long COMMIT_TIME             = 2000;  // Commit changes after 2s if no interaction
const unsigned long LONG_PRESS_DELAY        = 2000;  // 2s hold to enter time-set mode
const unsigned long REPEATED_EVENT_INTERVAL = 300;
const unsigned long BLINK_INTERVAL          = 300;
const unsigned long SPLIT_BLINK_INTERVAL    = 2000;
const unsigned long SHOW_SET_TIME           = 2000;  // 2s to display "SET" before entering time-set

unsigned long currentMillis;
unsigned long lastInteraction = 0;     // Tracks when a button was last pressed
unsigned long nextBlinkChange = 0;     // Tracks when a blink state should change

// ------------------ Buttons Pins ------------------

const byte BUTTON_UP_PIN   = 2;  // Wake-up button (external interrupt), Up button - top right
const byte BUTTON_MODE_PIN = 19; // Mode button - bottom left
const byte BUTTON_DOWN_PIN = 18; // Down button - bottom right
const byte BUTTON_SET_PIN  = 0;  // Set button (held 2 seconds) - top left

struct Button {
  const int pin;
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
};
Button upButton   = {BUTTON_UP_PIN,   0, false, false, false, 0, false, false, false, 0};
Button modeButton = {BUTTON_MODE_PIN, 0, false, false, false, 0, false, false, false, 0};
Button downButton = {BUTTON_DOWN_PIN, 0, false, false, false, 0, false, false, false, 0};
Button setButton  = {BUTTON_SET_PIN,  0, false, false, false, 0, false, false, false, 0};

// ------------------ State Machine ------------------
enum WatchState {
  SHOW_TIME,
  SET_TIME,
  CHRONO,
  MOON,
  SUN,
  SET_LOCALE,
  SLEEPING,
};

enum ShowTimeState {
  SHOW_HOUR,
  SHOW_DATE,
  SHOW_WEEKDAY,
  SHOW_YEAR,
};

enum SetTimeState {
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

enum SetLocaleState {
  SET_TZ_OFFSET_HOUR,
  SET_TZ_OFFSET_MINUTE,
  SET_LAT_DEG,
  SET_LAT_MIN,
  SET_LAT_SEC,
  SET_LONG_DEG,
  SET_LONG_MIN,
  SET_LONG_SEC    
};

enum ChronoState {
  CHRONO_ENTER,
  CHRONO_STOP,
  CHRONO_RUN,
  CHRONO_SPLIT,
};

enum MoonState {
  MOON_ENTER,
  MOON_PHASE,
  MOON_FULL,
  MOON_NEW
};

enum SunState {
  SUN_ENTER,
  SUN_NOON,
  SUN_SET,
  SUN_RISE
};

// State initialization
WatchState watchState = SHOW_TIME;
WatchState previousWatchState = SLEEPING;
ShowTimeState showTimeState = SHOW_HOUR;
SetTimeState setTimeState = SET_ENTER;
ChronoState chronoState = CHRONO_ENTER;
MoonState moonState = MOON_ENTER;
SunState sunState = SUN_ENTER;
SetLocaleState setLocaleState = SET_TZ_OFFSET_HOUR;

// ------------------ Display Options -------------------
int brightness = DEFAULT_BRIGHTNESS;

// ------------------ Global Time Variables -------------------
RtcDateTime now;
int hour = 0;
int minute = 0;
int day = 1;
int month = 1;
int year = 1066;
// 0 = Sunday, 1 = Monday, ... 6 = Saturday
uint8_t dayOfWeek = 1;

bool dateFormat = true;  // UK Date format DDMM = True, US Date Format MMDD = False

// ------------------ Set Time Variables -------------------
bool setBlink = false;

// ------------------ Set Locale Variables -------------------
struct Coord {
  bool positive;
  int deg;
  byte minutes;
  byte seconds;
};
int tz_offset_minutes = 0;  // UTC Offset in minutes for the current time
Coord latitude = {true, 51, 28, 40};
Coord longitude = {false, 0, 0, 5};

// ------------------ Chrono Time Variables -------------------

unsigned long chronoStartTime = 0;
unsigned long chronoSplitTime = 0;

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

// ------------------- Moon Phases ------------------
const byte moonPhases[8][4] = {
  { 0x80, 0x80, 0x80, 0x80 },  // 0: New Moon
  { 0x00, 0x00, 0x00, 0x0F },  // 1: Waxing Crescent
  { 0x00, 0x00, 0x39, 0x0F },  // 2: First Quarter
  { 0x00, 0x39, 0x09, 0x0F },  // 3: Waxing Gibbous
  { 0x39, 0x09, 0x09, 0x0F },  // 4: Full Moon
  { 0x39, 0x09, 0x0F, 0x00 },  // 5: Waning Gibbous (fixed your typo)
  { 0x39, 0x0F, 0x00, 0x00 },  // 6: Last Quarter
  { 0x39, 0x00, 0x00, 0x00 }   // 7: Waning Crescent
};


// ------------------- Week days ------------------
const byte weekDays[7][4] = {
  { 0x2D, 0x1C, 0x54, 0x00 },  // 0: SundAY   = 0x2D 0x1C 0x54 0x5E 0x77 0x6E
  { 0x37, 0x5C, 0x54, 0x00 },  // 1: MondAY   = 0x37 0x5C 0x54 0x5E 0x77 0x6E
  { 0x78, 0x1C, 0x79, 0x00 },  // 2: tuESdaY  = 0x78 0x1C 0x79 0x2D 0x5E 0x77 0x6E
  { 0x3E, 0x79, 0x5E, 0x00 },  // 3: WednSdAY = 0x3E 0x17 0x5E 0x54 0x2D 0x5E 0x77 0x6E
  { 0x78, 0x74, 0x1C, 0x00 },  // 4: thurSdAY = 0x78 0x74 0x1C 0x50 0x2D 0x5E 0x77 0x6E
  { 0x71, 0x50, 0x10, 0x00 },  // 5: FridAY   = 0x71 0x50 0x10 0x5E 0x77 0x6E
  { 0x2D, 0x77, 0x78, 0x00 }   // 6: SAturdAY = 0x2D 0x77 0x78 0x1C 0x50 0x5E 0x77 0x6E
};

float getDecimalCoord(int degs, int minutes, int seconds) {
  float decimals = degs + minutes / 60 + seconds / 3600;
  if (decimals < 0) {
    return -decimals;
  }
  return decimals;
}

float toDegrees(float radians) {
  return radians * 180 / PI;
} 

float toRadians(float radians) {
  return radians * PI / 180.0;
} 

long getJulianDay(int year, byte month, byte day) {
  // Adjust months for algorithm (Jan & Feb are 13 & 14 of previous year)
  if (month < 3) {
    year--;
    month += 12;
  }

  // Calculate Julian Day Number (JDN)
  long a = year / 100;
  long b = 2 - a + a / 4;
  long jd = (long)(365.25 * (year + 4716)) + (int)(30.6001 * (month + 1)) + day + b - 1524.5;
  return jd;
}

void getGregorianDate(long jd, int* year, byte* month, byte* day) {
  long y=4716;
  long j=1401;
  long m=2;
  long n=12;
  long r=4;
  long p=1461;
  long v=3;
  long u=5;
  long s=153;
  long w=2;
  long B=274277;
  long C=-38;

  long f = jd + j + (long)(((long)((4 * jd + B) / 146097) * 3) / 4) + C;
  long e = r * f + v;
  long g = (long)((e % p) / r);
  long h = u * g + w;
  long D = (long)((h % s) / u) + 1;
  long M = ((long)(h / s) + m) % n + 1;
  long Y = (long)(e / p) - y + (long)((n + m - M) / n);

  *year = (int)Y;
  *month = (byte)M;
  *day = (byte)D;
}

bool isLeapYear(int year) {
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
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

int getMonthDays(int month, int year) {
  if (month < 8) {
    if (month % 2 != 0) return 31;
    if (month == 2) {
      if (isLeapYear(year)) {
        return 29;
      } else {
        return 28;
      }
    }
    if (month % 2 == 0) {
      return 30;
    }
  } else {
    if (month % 2 == 0) {
      return 31;
    } else {
      return 30;
    }
  }
  // This should never happen
  return 31;
}

byte getMoonPhase(int year, byte month, byte day) {
  long jd = getJulianDay(year, month, day);

  // Days since known new moon on 2000 Jan 6 at 18:14 UTC (JD = 2451550.1)
  float daysSinceNew = jd - 2451550.1;

  // Moon age in days (modulo synodic month)
  float synodicMonth = 29.53058867;
  float age = fmod(daysSinceNew, synodicMonth);
  if (age < 0) {
    age += synodicMonth;
  }

  // Convert age to phase index 0–7
  byte index = (byte)((age / synodicMonth) * 8 + 0.5);  // round to nearest
  return index & 7;                                     // wrap to 0–7
}

void getNewMoon(int year, byte month, byte day, int* new_year, byte* new_month, byte* new_day) {
  long jd = getJulianDay(year, month, day);

  // Moon age in days (modulo synodic month)
  float synodicMonth = 29.53058867;
  // Days since known new moon on 2000 Jan 6 at 18:14 UTC (JD = 2451550.1)
  float daysSinceNew = jd - 2451550.1;

  float age = fmod(daysSinceNew, synodicMonth);
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

void getFullMoon(int year, byte month, byte day, int* new_year, byte* new_month, byte* new_day) {
  long jd = getJulianDay(year, month, day);

  // Moon age in days (modulo synodic month)
  float synodicMonth = 29.53058867;
  // Days since known new moon on 2000 Jan 6 at 18:14 UTC (JD = 2451550.1)
  float daysSinceFull = jd - 2451550.1 + synodicMonth / 2;

  float age = fmod(daysSinceFull, synodicMonth);
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

void setNumberSegs(uint8_t *segs, long number, int decimal, bool padZeros) {
  long truncated;
  long digit;

  bool last = false;

  truncated = number;
  for (int i=0; i<MAXNUMDIGITS; i++) {
    if (last && !padZeros && (decimal == -1 || decimal < i)) {
      segs[MAXNUMDIGITS-i-1] = 0;
    } else {
      digit = truncated % 10;
      truncated = (long)(truncated / 10);
      segs[MAXNUMDIGITS-i-1] = digitsSegments[digit];
      if (truncated == 0) {
        last = true;
      }
    }
    if (decimal == i) {
      segs[MAXNUMDIGITS-i-1] = segs[MAXNUMDIGITS-i-1] | 0x80;
    }
  }
}

void setup() {
  // ------------------ Pin Modes ------------------
  buttonInit(&upButton);
  buttonInit(&modeButton);
  buttonInit(&downButton);
  buttonInit(&setButton);
  pinMode(17, OUTPUT);

  // Attach interrupt for wake button
  attachInterrupt(digitalPinToInterrupt(BUTTON_UP_PIN), isrWake, FALLING);

  // ------------------ Seven-Segment Setup ------------------
  const byte numDigits = MAXNUMDIGITS;
  const byte digitPins[] = { 1, 3, 4, 5 };
  const byte segmentPins[] = { 6, 7, 8, 9, 10, 11, 12, 13 };
  const bool resistorsOnSegments = false;      // 'false' → resistors on digit pins
  const byte hardwareConfig = COMMON_CATHODE;  // On your PCB
  const bool updateWithDelays = false;         // Recommended = false
  const bool leadingZeros = true;              // Display leading zeros
  const bool disableDecPoint = false;          // No decimal point used

  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins,
               resistorsOnSegments, updateWithDelays, leadingZeros,
               disableDecPoint);

  // Set brightness
  sevseg.setBrightness(brightness);

  // ------------------ RTC Setup ------------------
  
  // Rtc.Begin(); MOCK EXAMPLE

  // Set the RTC to compile time ONCE at startup (remove if not desired):
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  Rtc.SetDateTime(compiled);

  // ------------------ RTC Setup ------------------
  // Disable any unused pins
  pinMode(20, OUTPUT);
  pinMode(21, OUTPUT);
  pinMode(22, OUTPUT);
}

void testLoop() {
  static int dotSecond = 0;
  static unsigned int lastDot = 0;
  uint8_t segs[MAXNUMDIGITS];
  segs[0] = 0;
  segs[1] = 0;
  segs[2] = 0;
  segs[3] = 0;

  static int setButtonCounter = 0;
  static int modeButtonCounter = 0;
  static int downButtonCounter = 0;
  static int upButtonCounter = 0;

  if (setButton.pressed) {
    segs[0] = segs[0] | 0b00110000;
  }
  if (setButton.repeatedEvent) {
    segs[0] = segs[0] | 0b01000000;
  }
  if (setButton.longPressed) {
    segs[0] = segs[0] | 0b00001001;
  }
  if (modeButton.pressed) {
    segs[1] = segs[1] | 0b00110000;
  }
  if (modeButton.repeatedEvent) {
    segs[1] = segs[1] | 0b01000000;
  }
  if (modeButton.longPressed) {
    segs[1] = segs[1] | 0b00001001;
  }
  if (downButton.pressed) {
    segs[2] = segs[2] | 0b00110000;
  }
  if (downButton.repeatedEvent) {
    segs[2] = segs[2] | 0b01000000;
  }
  if (downButton.longPressed) {
    segs[2] = segs[2] | 0b00001001;
  }
  if (upButton.pressed) {
    segs[3] = segs[3] | 0b00110000;
  }
  if (upButton.repeatedEvent) {
    segs[3] = segs[3] | 0b01000000;
  }
  if (upButton.longPressed) {
    segs[3] = segs[3] | 0b00001001;
  }

  if (setButton.pressedEvent) { // || setButton.repeatedEvent) {
    setButtonCounter = (setButtonCounter + 1) % 10;
  }
  if (modeButton.pressedEvent) { // || modeButton.repeatedEvent) {
    modeButtonCounter = (modeButtonCounter + 1) % 10;
  }
  if (downButton.pressedEvent) { // || downButton.repeatedEvent) {
    downButtonCounter = (downButtonCounter + 1) % 10;
  }
  if (upButton.pressedEvent) { // || upButton.repeatedEvent) {
    upButtonCounter = (upButtonCounter + 1) % 10;
  }

  if ((segs[0] & 0x7f) != 0) {
    segs[3] = digitsSegments[setButtonCounter];
  } else if (((segs[0] & 0x7f) | (segs[1] & 0x7f) | (segs[2] & 0x7f) | (segs[3] & 0x7f)) == 0) {
    segs[0] = digitsSegments[setButtonCounter];
    segs[1] = digitsSegments[modeButtonCounter];
    segs[2] = digitsSegments[downButtonCounter];
    segs[3] = digitsSegments[upButtonCounter];
  }

  if (lastDot + 1000 < currentMillis) {
    dotSecond = (dotSecond + 1) % 4;
    lastDot = lastDot + 1000;
  }
  segs[dotSecond] = segs[dotSecond] | 0x80;

  sevseg.setSegments(segs);
}

void testChrono() {
  uint8_t segs[MAXNUMDIGITS];
  long cents;

  cents = (long)(currentMillis / 10);
  setNumberSegs(segs, cents, 2, false);
  sevseg.setSegments(segs);
}

void loop() {

  currentMillis = millis();

  // Refresh the display often
  sevseg.refreshDisplay();

  // Refresh the buttons state
  buttonUpdateState(&upButton);
  buttonUpdateState(&modeButton);
  buttonUpdateState(&downButton);
  buttonUpdateState(&setButton);
  if (upButton.pressed || modeButton.pressed || downButton.pressed || setButton.pressed) {
    lastInteraction = currentMillis;
  }

  //testLoop();
  //testChrono();
  
  /**/
  // State machine
  switch (watchState) {
    case SHOW_TIME:
      handleShowTimeMode();
      previousWatchState = SHOW_TIME;  
      break;
    case SET_TIME:
      handleSetTimeMode();
      previousWatchState = SET_TIME;  
      break;
    case CHRONO:
      handleChronoMode();
      previousWatchState = CHRONO;  
      break;
    case MOON:
      handleMoonMode();
      previousWatchState = MOON;  
      break;
    case SUN:
      handleSunMode();
      previousWatchState = SUN;  
      break;
    case SET_LOCALE:
      handleSetLocaleMode();
      previousWatchState = SET_LOCALE;  
      break;
    case SLEEPING:
      // Code only returns here after interrupt sets watchState to NORMAL.
      // So there's nothing special to do in the loop if watchState == SLEEPING.
      previousWatchState = SLEEPING;
      break;
  }
  /**/
}

// ----------------------------------------------------------
//                  SHOW TIME MODE
// ----------------------------------------------------------
void handleShowTimeMode() {
  int dateDisplay;
  uint8_t segs[MAXNUMDIGITS];
  static unsigned long nextTick;
  static bool showDot = false;
  
  if (previousWatchState != SHOW_TIME) {
    readDateVars();
    nextTick = currentMillis + 1000;
    showDot = false;
    showTimeState = SHOW_HOUR;
  }
  
  //check if min button is pressed, display date if it is
  if (setButton.longPressedEvent) {
    watchState = SET_TIME;
  } else if (setButton.pressedEvent) {
    switch (showTimeState) {
      case SHOW_HOUR:
        showTimeState = SHOW_DATE;
        break; 
      case SHOW_DATE:
        showTimeState = SHOW_WEEKDAY;
        break; 
      case SHOW_WEEKDAY:
        showTimeState = SHOW_YEAR;
        break; 
      case SHOW_YEAR:
        showTimeState = SHOW_HOUR;
        break; 
    }
  } else if (modeButton.pressedEvent) {
    if (showTimeState == SHOW_HOUR) {
      watchState = CHRONO;
      return;
    } else {
      showTimeState = SHOW_HOUR;
    }
  }

  // Display time components
  switch (showTimeState) {
    case SHOW_HOUR:
      //Time is read once globally before displaying time, reduces flickering
      if (nextTick < currentMillis) {
        nextTick = nextTick + 1000;
        showDot = !showDot;
      }
      if (showDot) {
        setNumberSegs(segs, hour*100 + minute, 2, true);
      } else {
        setNumberSegs(segs, hour*100 + minute, -1, true);
      }
      sevseg.setSegments(segs);
      break; 
    case SHOW_DATE:
      // Date
      if (dateFormat == true) {
        dateDisplay = (now.Day() * 100) + now.Month();
      } else {
        dateDisplay = (now.Month() * 100) + now.Day();
      }
      sevseg.setNumber(dateDisplay, 2);  // 1 = leading zeros
      break; 
    case SHOW_WEEKDAY:
      // Weekday
      sevseg.setSegments(weekDays[dayOfWeek]);
      break; 
    case SHOW_YEAR:
      //Year
      sevseg.setNumber(now.Year());  // 1 = leading zeros
      break; 
  }

  // Check for inactivity → sleep
  if (currentMillis - lastInteraction > wakeInterval) {
    goToSleep();
  }
}


void handleSetTimeMode() {
  static int setHour;
  static int setMinute;
  static int setDay;
  static int setMonth;
  static int setYear;
  static bool setDateFormat;

  int maxDays;

  uint8_t segs[MAXNUMDIGITS];

  if (previousWatchState != SET_TIME) {
    setTimeState = SET_ENTER;
  }

  if (modeButton.longPressed) {
    watchState = SHOW_TIME;
    return;
  }

  if (setTimeState != SET_ENTER) {
    if (setButton.pressedEvent) {
      maxDays = getMonthDays(setMonth, setYear);
      if (setDay > maxDays) {
        setDay = maxDays;
      }
      Rtc.SetDateTime(RtcDateTime(setYear, setMonth, setDay, setHour, setMinute, now.Second()));
      readDateVars();
      dateFormat = setDateFormat;
      watchState = SHOW_TIME;
    }
  }

  // Handle inputs
  switch (setTimeState) {
    case SET_ENTER:
      if (lastInteraction + SHOW_SET_TIME < currentMillis) {
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = false;
        setTimeState = SET_HOUR;
        readDateVars();
        setHour = hour;
        setMinute = minute;
        setDay = day;
        setMonth = month;
        setYear = year; 
        setDateFormat = dateFormat;
      }
      break;
    case SET_HOUR:
      if (modeButton.pressedEvent) {
        setTimeState = SET_MINUTES;
        break;
      }    
      if (upButton.pressedEvent || upButton.repeatedEvent) {
        setHour = (setHour + 1) % 24;
      }
      if (downButton.pressedEvent || downButton.repeatedEvent) {
        setHour = (setHour + 23) % 24;
      }
      break;
    case SET_MINUTES:
      if (modeButton.pressedEvent) {
        setTimeState = SET_DAY;
        break;
      }    
      if (upButton.pressedEvent || upButton.repeatedEvent) {
        setMinute = (setMinute + 1) % 60;
      }
      if (downButton.pressedEvent || downButton.repeatedEvent) {
        setMinute = (setMinute + 59) % 60;
      }    
      break;
    case SET_DAY:
      if (modeButton.pressedEvent) {
        setTimeState = SET_MONTH;
        break;
      }
      // Get max days in setMonth for setYear
      maxDays = getMonthDays(setMonth, setYear);
      if (upButton.pressedEvent || upButton.repeatedEvent) {
        setDay = (setDay + 1) % maxDays;
      }
      if (downButton.pressedEvent || downButton.repeatedEvent) {
        setDay = (setDay + maxDays - 1) % maxDays;
      }
      break;
    case SET_MONTH:
      if (modeButton.pressedEvent) {
        setTimeState = SET_YEAR;
        break;
      }
      if (upButton.pressedEvent || upButton.repeatedEvent) {
        setMonth = (setMonth + 1) % 12;
      }
      if (downButton.pressedEvent || downButton.repeatedEvent) {
        setMonth = (setMonth + 11) % 12;
      }    
      break;
    case SET_YEAR:
      if (modeButton.pressedEvent) {
        setTimeState = SET_FORMAT;
        break;
      }    
      if (upButton.pressedEvent || upButton.repeatedEvent) {
        setYear = setYear + 1;
      }
      if (downButton.pressedEvent || downButton.repeatedEvent) {
        setYear = setYear - 1;
      }    
      break;
    case SET_FORMAT:
      if (modeButton.pressedEvent) {
        setTimeState = SET_DISPLAY;
        break;
      }    
      // Set day/month order format
      if (upButton.pressedEvent || downButton.pressedEvent) {
        setDateFormat = !setDateFormat;
      }
      break;
    case SET_DISPLAY:
      if (modeButton.pressedEvent) {
        setTimeState = SET_BRIGHTNESS;
        break;
      }    
      if (upButton.pressedEvent || upButton.repeatedEvent) {
        wakeInterval = wakeInterval + 1000;
      }
      if (downButton.pressedEvent || downButton.repeatedEvent) {
        wakeInterval = wakeInterval - 1000;
      }
      if (wakeInterval < 3000) {
        wakeInterval = 3000;
      }
      if (wakeInterval > 9000) {
        wakeInterval = 9000;
      }
      break;
    case SET_BRIGHTNESS:
      if (modeButton.pressedEvent) {
        setTimeState = SET_HOUR;
        break;
      }    
      // Set display brightness
      if (upButton.pressedEvent || upButton.repeatedEvent) {
        brightness = brightness + 5;
      }
      if (downButton.pressedEvent || downButton.repeatedEvent) {
        brightness = brightness - 5;
      }      
      if (brightness < 25) {
        brightness = 25;
      }
      if (brightness > 100) {
        brightness = 100;
      }
      sevseg.setBrightness(brightness);
      break;
  }

  // Handle display
  switch (setTimeState) {
    case SET_ENTER:
      sevseg.setChars("SET ");
      break;
    case SET_HOUR:
      if (currentMillis > nextBlinkChange) {
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = !setBlink;
      }
      sevseg.setNumber((setHour * 100) + setMinute, 2, false);
      sevseg.getSegments(segs);
      if (setBlink) {
         segs[0] = 0;
         segs[1] = 0;
         sevseg.setSegments(segs);
      }
      break;
    case SET_MINUTES:
      if (currentMillis > nextBlinkChange) {
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = !setBlink;
      }
      sevseg.setNumber((setHour * 100) + setMinute, 2, false);
      sevseg.getSegments(segs);
      if (setBlink) {
         segs[2] = 0;
         segs[3] = 0;
         sevseg.setSegments(segs);
      }
      break;
    case SET_DAY:
      if (currentMillis > nextBlinkChange) {
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = !setBlink;
      }      
      segs[0] = 0x5E; // 'd'
      segs[1] = 0x6E; // 'y'
      segs[2] = digitsSegments[(int)(setDay / 10)];
      segs[3] = digitsSegments[setDay % 10];
      if (setBlink) {
         segs[2] = 0;
         segs[3] = 0;
      }
      sevseg.setSegments(segs);      
      break;
    case SET_MONTH:
      if (currentMillis > nextBlinkChange) {
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = !setBlink;
      }
      segs[0] = 0x37; // 'M'
      segs[1] = 0x5C; // 'o'
      segs[2] = digitsSegments[(int)(setMonth / 10)];
      segs[3] = digitsSegments[setMonth % 10];
      if (setBlink) {
         segs[2] = 0;
         segs[3] = 0;
      }
      sevseg.setSegments(segs);
      break;
    case SET_YEAR:
      if (currentMillis > nextBlinkChange) {
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = !setBlink;
      }

      setNumberSegs(segs, setYear, -1, true);
      if (setBlink) {
        segs[0] = 0;
        segs[1] = 0;
        segs[2] = 0;
        segs[3] = 0;
        sevseg.setSegments(segs);
      } else {
        sevseg.setSegments(segs);
      }
      break;
    case SET_FORMAT:
      if (setDateFormat) {
        // dY.Mo
        segs[0] = 0x5E;
        segs[1] = 0xEE;
        segs[2] = 0x37;
        segs[3] = 0x5C;
        sevseg.setSegments(segs);
      } else {
        // Mo.dY
        segs[0] = 0x37;
        segs[1] = 0xDC;
        segs[2] = 0x5E;
        segs[3] = 0x6E;
        sevseg.setSegments(segs);
      }
      break;
    case SET_DISPLAY:
      // Set display time
      segs[0] = 0x6D; // 'S'
      segs[1] = 0x38; // 'L'
      segs[2] = 0x73; // 'P'
      segs[3] = digitsSegments[(int)(wakeInterval / 1000)];
      sevseg.setSegments(segs);
      break;
    case SET_BRIGHTNESS:
      // Set display segments
      segs[0] = 0;
      segs[1] = digitsSegments[(int)(brightness / 100)];
      segs[2] = digitsSegments[(int)(brightness / 10) % 10];
      segs[3] = digitsSegments[(int)(brightness % 10)];
      sevseg.setSegments(segs);
      sevseg.setBrightness(brightness);
      break;
  }
}

void handleChronoMode() {
  static long chronoStart = -1;
  static long chronoSplit = 0;
  static long chronoTotal = 0;
  static bool viewMinutes = false;
  long cents;
  long minutes;
  uint8_t segs[MAXNUMDIGITS];
  
  if (previousWatchState != CHRONO) {
    chronoState = CHRONO_ENTER;
  }

  if (modeButton.longPressed || modeButton.pressedEvent) {
    watchState = MOON;
    return;
  }

  if (upButton.pressedEvent) {
    viewMinutes = !viewMinutes;
  }

  // Input handling
  switch (chronoState) {
    case CHRONO_ENTER:
      if (lastInteraction + SHOW_SET_TIME < currentMillis) {
        setBlink = false;
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        viewMinutes=false;
        chronoState = CHRONO_STOP;
      }
      break;
    case CHRONO_STOP:
      if (setButton.pressedEvent) {
        // Reset
        chronoStart = -1;
        chronoSplit = 0;
        chronoTotal = 0;
      } else if (downButton.pressedEvent) {
        // Run
        chronoStart = currentMillis;
        chronoState = CHRONO_RUN;
      }
      break;
    case CHRONO_RUN:
      if (setButton.pressedEvent) {
        // Split
        chronoSplit = currentMillis - chronoStart + chronoTotal;
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = false;
        chronoState = CHRONO_SPLIT;
      } else if (downButton.pressedEvent) {
        // Stop
        chronoTotal = currentMillis - chronoStart + chronoTotal;
        chronoState = CHRONO_STOP;
      }
      break;
    case CHRONO_SPLIT:
      if (setButton.pressedEvent) {
        // Run (continue)
        chronoState = CHRONO_RUN;
      } else if (downButton.pressedEvent) {
        // Stop
        chronoTotal = currentMillis - chronoStart + chronoTotal;
        chronoSplit = 0;
        chronoState = CHRONO_STOP;
      }
      break;
  }

  // Display handling
  switch (chronoState) {
    case CHRONO_ENTER:
      // Chrn
      segs[0] = 0x39; 
      segs[1] = 0x74;
      segs[2] = 0x50;
      segs[3] = 0x54;
      sevseg.setSegments(segs);
      break;
    case CHRONO_STOP:
      cents = (long)(chronoTotal / 10);
      minutes = (long)(cents / 6000);
      cents = cents - minutes * 6000;

      if (viewMinutes) {
        setNumberSegs(segs, minutes, -1, false);
        sevseg.setSegments(segs);
      } else {
        setNumberSegs(segs, cents, 2, false);
        sevseg.setSegments(segs);
      }
      break;
    case CHRONO_RUN:
      cents = (long)((currentMillis - chronoStart + chronoTotal) / 10);     
      minutes = (long)(cents / 6000);
      cents = cents - minutes * 6000;
      
      if (viewMinutes) {
        setNumberSegs(segs, minutes, -1, false);
        sevseg.setSegments(segs);
      } else {
        setNumberSegs(segs, cents, 2, minutes > 0);
        sevseg.setSegments(segs);
      }
      break;
    case CHRONO_SPLIT:
      if (currentMillis > nextBlinkChange) {
        if (setBlink) {
          nextBlinkChange = currentMillis + SPLIT_BLINK_INTERVAL;
        } else {
          nextBlinkChange = currentMillis + BLINK_INTERVAL;
        }
        setBlink = !setBlink;
      }

      cents = (long)(chronoSplit / 10);
      minutes = (long)(cents / 6000);
      cents = cents - minutes * 6000;

      if (setBlink) {
        //SPLt
        segs[0] = 0x6D; 
        segs[1] = 0x73; 
        segs[2] = 0x38; 
        segs[3] = 0x78; 
        sevseg.setSegments(segs);
      } else {
        if (viewMinutes) {
          setNumberSegs(segs, minutes, -1, false);
          sevseg.setSegments(segs);
        } else {
          setNumberSegs(segs, cents, 2, minutes > 0);
          sevseg.setSegments(segs);
        }
      }
      break;
  }
}

void handleMoonMode() {
  int dateDisplay;
  byte moonPhase;
  int new_year;
  byte new_month;
  byte new_day;
  int full_year;
  byte full_month;
  byte full_day;

  uint8_t segs[MAXNUMDIGITS];

  if (previousWatchState != MOON) {
    moonState = MOON_ENTER;
  }

  if (modeButton.longPressed) {
    watchState = SHOW_TIME;
    return;
  }

  if (modeButton.pressedEvent) {
    watchState = SUN;
    return;
  }

  // flow control
  switch (moonState) {
    case MOON_ENTER:
      nextBlinkChange = currentMillis + SHOW_SET_TIME;
      setBlink = true;
      moonPhase = getMoonPhase(year, month, day);
      getNewMoon(year, month, day, &new_year, &new_month, &new_day);
      getFullMoon(year, month, day, &full_year, &full_month, &full_day);
      moonState = MOON_PHASE;
      break;
    case MOON_PHASE:
      if (upButton.pressedEvent) {
        moonState = MOON_NEW;
        nextBlinkChange = currentMillis + SHOW_SET_TIME;
        setBlink = true;
      }
      if (downButton.pressedEvent) {
        moonState = MOON_FULL;
        nextBlinkChange = currentMillis + SHOW_SET_TIME;
        setBlink = true;
      }
      break;
    case MOON_FULL:
      if (upButton.pressedEvent) {
        moonState = MOON_PHASE;
        nextBlinkChange = currentMillis + SHOW_SET_TIME;
        setBlink = true;
      }
      if (downButton.pressedEvent) {
        moonState = MOON_NEW;
        nextBlinkChange = currentMillis + SHOW_SET_TIME;
        setBlink = true;
      }
      break;
    case MOON_NEW:
      if (upButton.pressedEvent) {
        moonState = MOON_FULL;
        nextBlinkChange = currentMillis + SHOW_SET_TIME;
        setBlink = true;
      }
      if (downButton.pressedEvent) {
        moonState = MOON_PHASE;
        nextBlinkChange = currentMillis + SHOW_SET_TIME;
        setBlink = true;
      }
      break;
  }

  switch (moonState) {
    case MOON_ENTER:
      break;
    case MOON_PHASE:
      if (currentMillis > nextBlinkChange) {
        if (setBlink) {
          nextBlinkChange = currentMillis + SHOW_SET_TIME;
        } else {
          nextBlinkChange = currentMillis + SHOW_SET_TIME;
        }
        setBlink = !setBlink;
      }
      if (setBlink) {
        segs[0] = 0x37; // M
        segs[1] = 0x5C; // o
        segs[2] = 0x5C; // o
        segs[3] = 0x54; // n
        sevseg.setSegments(segs);
      } else {
        sevseg.setSegments(moonPhases[moonPhase]);
      }
      break;
    case MOON_FULL:
      if (currentMillis > nextBlinkChange) {
        if (setBlink) {
          nextBlinkChange = currentMillis + SHOW_SET_TIME;
        } else {
          nextBlinkChange = currentMillis + SHOW_SET_TIME;
        }
        setBlink = !setBlink;
      }
      if (setBlink) {
        segs[0] = 0x71; // F
        segs[1] = 0x1C; // u
        segs[2] = 0x30; // l
        segs[3] = 0x30; // l
        sevseg.setSegments(segs);
      } else {
        if (dateFormat == true) {
          dateDisplay = (full_day * 100) + full_month;
        } else {
          dateDisplay = (full_month * 100) + full_day;
        }
        sevseg.setNumber(dateDisplay, 2);  // 1 = leading zeros
      }
      break;
    case MOON_NEW:
      if (currentMillis > nextBlinkChange) {
        if (setBlink) {
          nextBlinkChange = currentMillis + SHOW_SET_TIME;
        } else {
          nextBlinkChange = currentMillis + SHOW_SET_TIME;
        }
        setBlink = !setBlink;
      }
      if (setBlink) {
        segs[0] = 0x37; // N
        segs[1] = 0x79; // E
        segs[2] = 0x3E; // W
        segs[3] = 0; // 
        sevseg.setSegments(segs);
      } else {
        if (dateFormat == true) {
          dateDisplay = (new_day * 100) + new_month;
        } else {
          dateDisplay = (new_month * 100) + new_day;
        }
        sevseg.setNumber(dateDisplay, 2);  // 1 = leading zeros
      }
      break;
  }
}

void handleSetLocaleMode() {
  static int set_tz_offset_hours;
  static byte set_tz_offset_minutes;
  static Coord set_latitude;
  static Coord set_longitude;

  uint8_t segs[MAXNUMDIGITS];

  if (previousWatchState != SET_LOCALE) {
    setBlink = false;
    nextBlinkChange = currentMillis + BLINK_INTERVAL;

    set_tz_offset_hours = (int)(tz_offset_minutes/60);
    set_tz_offset_minutes = (byte)(tz_offset_minutes % 60);

    set_latitude.positive = latitude.positive;
    set_latitude.deg = latitude.deg;
    set_latitude.minutes = latitude.minutes;
    set_latitude.seconds = latitude.seconds;

    set_longitude.positive = longitude.positive;
    set_longitude.deg = longitude.deg;
    set_longitude.minutes = longitude.minutes;
    set_longitude.seconds = longitude.seconds;
    
    setLocaleState = SET_TZ_OFFSET_HOUR;
  }

  if (modeButton.longPressed) {
    watchState = SHOW_TIME;
    return;
  }

  if (setButton.pressedEvent) {
    // Save changes
    tz_offset_minutes = set_tz_offset_hours*60 + set_tz_offset_minutes;

    latitude.positive = set_latitude.positive;
    latitude.deg = set_latitude.deg;
    latitude.minutes = set_latitude.minutes;
    latitude.seconds = set_latitude.seconds;

    longitude.positive = set_longitude.positive;
    longitude.deg = set_longitude.deg;
    longitude.minutes = set_longitude.minutes;
    longitude.seconds = set_longitude.seconds;
  
    // Return
    watchState = SUN;
    return;
  }

  // Flow control
  switch (setLocaleState) {
    case SET_TZ_OFFSET_HOUR:
      if (modeButton.pressedEvent) {
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = !setBlink;
        setLocaleState = SET_TZ_OFFSET_MINUTE;
        break;
      }
      if (upButton.pressedEvent || upButton.repeatedEvent) {
        set_tz_offset_hours = set_tz_offset_hours + 1;
      }
      if (downButton.pressedEvent || downButton.repeatedEvent) {
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
      if (modeButton.pressedEvent) {
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = !setBlink;
        setLocaleState = SET_LAT_DEG;
        break;
      }
      if (upButton.pressedEvent || upButton.repeatedEvent) {
        set_tz_offset_minutes = (set_tz_offset_minutes + 1) % 60;
      }
      if (downButton.pressedEvent || downButton.repeatedEvent) {
        set_tz_offset_minutes = (set_tz_offset_minutes + 59) % 60;
      }
      break;    
    case SET_LAT_DEG:
      if (modeButton.pressedEvent) {
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = !setBlink;
        setLocaleState = SET_LAT_MIN;
        break;
      }
      if (upButton.pressedEvent || upButton.repeatedEvent) {
        set_latitude.deg = set_latitude.deg + (set_latitude.positive ? 1 : -1);
      }
      if (downButton.pressedEvent || downButton.repeatedEvent) {
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
      if (modeButton.pressedEvent) {
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = !setBlink;
        setLocaleState = SET_LAT_SEC;
        break;
      }
      if (upButton.pressedEvent || upButton.repeatedEvent) {
        set_latitude.minutes = (set_latitude.minutes + 1) % 60;
      }
      if (downButton.pressedEvent || downButton.repeatedEvent) {
        set_latitude.minutes = (set_latitude.minutes + 59) % 60;
      }
      break;    
    case SET_LAT_SEC:
      if (modeButton.pressedEvent) {
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = !setBlink;
        setLocaleState = SET_LONG_DEG;
        break;
      }
      if (upButton.pressedEvent || upButton.repeatedEvent) {
        set_latitude.seconds = (set_latitude.seconds+1) % 60;
      }
      if (downButton.pressedEvent || downButton.repeatedEvent) {
        set_latitude.seconds = (set_latitude.seconds + 59) % 60;
      }
      break;
    case SET_LONG_DEG:
      if (modeButton.pressedEvent) {
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = !setBlink;
        setLocaleState = SET_LONG_MIN;
        break;
      }
      if (upButton.pressedEvent || upButton.repeatedEvent) {
        set_longitude.deg = set_longitude.deg + (set_longitude.positive ? 1 : -1);
      }
      if (downButton.pressedEvent || downButton.repeatedEvent) {
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
      if (modeButton.pressedEvent) {
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = !setBlink;
        setLocaleState = SET_LONG_SEC;
        break;
      }
      if (upButton.pressedEvent || upButton.repeatedEvent) {
        set_longitude.minutes = (set_longitude.minutes + 1) % 60;
      }
      if (downButton.pressedEvent || downButton.repeatedEvent) {
        set_longitude.minutes = (set_longitude.minutes + 59) % 60;
      }
      break;    
    case SET_LONG_SEC:
      if (modeButton.pressedEvent) {
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = !setBlink;
        setLocaleState = SET_TZ_OFFSET_HOUR;
        break;
      }
      if (upButton.pressedEvent || upButton.repeatedEvent) {
        set_longitude.seconds = (set_longitude.seconds+1) % 60;
      }
      if (downButton.pressedEvent || downButton.repeatedEvent) {
        set_longitude.seconds = (set_longitude.seconds + 59) % 60;
      }
      break;
  }

  // Display
  switch (setLocaleState) {
    case SET_TZ_OFFSET_HOUR:
      if (currentMillis > nextBlinkChange) {
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = !setBlink;
      }
      if (set_tz_offset_hours < 0) {
        segs[0] = 0x40;
        segs[1] = digitsSegments[(int)(-set_tz_offset_hours / 10) % 10];
        segs[2] = digitsSegments[(int)(-set_tz_offset_hours) % 10];
      } else {
        segs[0] = 0;
        segs[1] = digitsSegments[(int)(set_tz_offset_hours / 10) % 10];
        segs[2] = digitsSegments[(int)(set_tz_offset_hours) % 10];
      }
      segs[3] = 0x74; // h
      if (setBlink) {
         segs[0] = 0;
         segs[1] = 0;
         segs[2] = 0;
      }
      sevseg.setSegments(segs);
      break;
    case SET_TZ_OFFSET_MINUTE:
      if (currentMillis > nextBlinkChange) {
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = !setBlink;
      }
      segs[0] = 0;
      segs[1] = 0;
      segs[2] = digitsSegments[(int)(set_tz_offset_minutes / 10) % 10];
      segs[3] = digitsSegments[(int)(set_tz_offset_minutes) % 10];
      if (setBlink) {
         segs[2] = 0;
         segs[3] = 0;
      }
      sevseg.setSegments(segs);
      break;
    case SET_LAT_DEG:
      if (currentMillis > nextBlinkChange) {
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = !setBlink;
      }
      segs[0] = digitsSegments[(int)(set_latitude.deg / 100) % 10];
      segs[1] = digitsSegments[(int)(set_latitude.deg / 10) % 10];
      segs[2] = digitsSegments[(int)(set_latitude.deg) % 10];

      if (set_latitude.positive) {
        segs[3] = 0b00110111; // N
      } else {
        segs[3] = 0b01101101; // S
      }

      if (setBlink) {
         segs[0] = 0;
         segs[1] = 0;
         segs[2] = 0;
      }
      sevseg.setSegments(segs);
      break;
    case SET_LAT_MIN:
      if (currentMillis > nextBlinkChange) {
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = !setBlink;
      }
      segs[0] = 0;
      segs[1] = digitsSegments[(int)(set_latitude.minutes / 10) % 10];
      segs[2] = digitsSegments[(int)(set_latitude.minutes) % 10];
      segs[3] = 0b00100000; // '
      if (setBlink) {
         segs[1] = 0;
         segs[2] = 0;
      }
      sevseg.setSegments(segs);
      break;
    case SET_LAT_SEC:
      if (currentMillis > nextBlinkChange) {
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = !setBlink;
      }
      segs[0] = 0;
      segs[1] = digitsSegments[(int)(set_latitude.seconds / 10) % 10];
      segs[2] = digitsSegments[(int)(set_latitude.seconds) % 10];
      segs[3] = 0b00100010; // "
      if (setBlink) {
         segs[1] = 0;
         segs[2] = 0;
      }
      sevseg.setSegments(segs);
      break;
    case SET_LONG_DEG:
      if (currentMillis > nextBlinkChange) {
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = !setBlink;
      }
      segs[0] = digitsSegments[(int)(set_longitude.deg / 100) % 10];
      segs[1] = digitsSegments[(int)(set_longitude.deg / 10) % 10];
      segs[2] = digitsSegments[(int)(set_longitude.deg) % 10];
      if (set_longitude.positive) {
        segs[3] = 0b01111001; // E
      } else {
        segs[3] = 0b00111110; // W
      }
      if (setBlink) {
         segs[0] = 0;
         segs[1] = 0;
         segs[2] = 0;
      }
      sevseg.setSegments(segs);
      break;
    case SET_LONG_MIN:
      if (currentMillis > nextBlinkChange) {
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = !setBlink;
      }
      segs[0] = 0;
      segs[1] = digitsSegments[(int)(set_longitude.minutes / 10) % 10];
      segs[2] = digitsSegments[(int)(set_longitude.minutes) % 10];
      segs[3] = 0b00100000; // '
      if (setBlink) {
         segs[1] = 0;
         segs[2] = 0;
      }
      sevseg.setSegments(segs);
      break;
    case SET_LONG_SEC:
      if (currentMillis > nextBlinkChange) {
        nextBlinkChange = currentMillis + BLINK_INTERVAL;
        setBlink = !setBlink;
      }
      segs[0] = 0;
      segs[1] = digitsSegments[(int)(set_longitude.seconds / 10) % 10];
      segs[2] = digitsSegments[(int)(set_longitude.seconds) % 10];
      segs[3] = 0b00100010; // "
      if (setBlink) {
         segs[1] = 0;
         segs[2] = 0;
      }
      sevseg.setSegments(segs);
      break;
  }

}

void handleSunMode() {
  static float noon;
  static float sunrise;
  static float sunset;

  int displayHour;
  int displayMinutes;
  uint8_t segs[MAXNUMDIGITS];


  if (previousWatchState != SUN) {
    // compute the solar details only when the mode is entered
    getSolarDates(
      year,
      month,
      day,
      getDecimalCoord(latitude.positive ? latitude.deg : -latitude.deg, latitude.minutes, latitude.seconds),
      getDecimalCoord(longitude.positive ? longitude.deg : -longitude.deg, longitude.minutes, longitude.seconds),
      tz_offset_minutes,
      &noon,
      &sunrise,
      &sunset
    );
    sunState = SUN_ENTER;
  }

  if (modeButton.longPressed || modeButton.pressedEvent) {
    watchState = SHOW_TIME;
    return;
  }

  if (setButton.longPressed) {
    watchState = SET_LOCALE;
    return;
  }

  // flow control
  switch (sunState) {
    case SUN_ENTER:
      if (lastInteraction + SHOW_SET_TIME < currentMillis) {
        setBlink = false;
        nextBlinkChange = currentMillis + SHOW_SET_TIME;
        sunState = SUN_NOON;
      }
      break;
    case SUN_NOON:
      if (upButton.pressedEvent) {
        sunState = SUN_RISE;
        nextBlinkChange = currentMillis + SHOW_SET_TIME;
        setBlink = true;
      }
      if (downButton.pressedEvent) {
        sunState  = SUN_SET;
        nextBlinkChange = currentMillis + SHOW_SET_TIME;
        setBlink = true;
      }
      break;
    case SUN_SET:
      if (upButton.pressedEvent) {
        sunState = SUN_NOON;
        nextBlinkChange = currentMillis + SHOW_SET_TIME;
        setBlink = true;
      }
      if (downButton.pressedEvent) {
        sunState  = SUN_RISE;
        nextBlinkChange = currentMillis + SHOW_SET_TIME;
        setBlink = true;
      }
      break;
    case SUN_RISE:
      if (upButton.pressedEvent) {
        sunState = SUN_SET;
        nextBlinkChange = currentMillis + SHOW_SET_TIME;
        setBlink = true;
      }
      if (downButton.pressedEvent) {
        sunState  = SUN_NOON;
        nextBlinkChange = currentMillis + SHOW_SET_TIME;
        setBlink = true;
      }
      break;
  }

  // display
  switch (sunState) {
    case SUN_ENTER:
      segs[0] = 0x6D; //S
      segs[1] = 0x1C; //u
      segs[2] = 0x54; //n
      segs[3] = 0;
      sevseg.setSegments(segs);
      break;
    case SUN_NOON:
      if (currentMillis > nextBlinkChange) {
        nextBlinkChange = currentMillis + SHOW_SET_TIME;
        setBlink = !setBlink;
      }
      if (setBlink) {
        segs[0] = 0x54; // n
        segs[1] = 0x5C; // o
        segs[2] = 0x5C; // o
        segs[3] = 0x54; // n
      } else {
        displayHour = (int)(noon * 24) % 24;
        displayMinutes = ((int)(noon * 1440) - displayHour*60) % 60;
        setNumberSegs(segs, displayHour*100 + displayMinutes, -1, true);
      }      
      sevseg.setSegments(segs);
      break;
    case SUN_SET:
      if (currentMillis > nextBlinkChange) {
        nextBlinkChange = currentMillis + SHOW_SET_TIME;
        setBlink = !setBlink;
      }
      if (setBlink) {
        segs[0] = 0x6D; // S
        segs[1] = 0x6D; // S
        segs[2] = 0x79; // E
        segs[3] = 0x78; // t
      } else {
        displayHour = (int)(sunset * 24) % 24;
        displayMinutes = ((int)(sunset * 1440) - displayHour*60) % 60;
        setNumberSegs(segs, displayHour*100 + displayMinutes, -1, true);
      }
      sevseg.setSegments(segs);
      break;
    case SUN_RISE:
      if (currentMillis > nextBlinkChange) {
        nextBlinkChange = currentMillis + SHOW_SET_TIME;
        setBlink = !setBlink;
      }
      if (setBlink) {
        segs[0] = 0x50; // r
        segs[1] = 0x10; // i
        segs[2] = 0x6D; // S
        segs[3] = 0x79; // E
      } else {
        displayHour = (int)(sunrise * 24) % 24;
        displayMinutes = ((int)(sunrise * 1440) - displayHour*60) % 60;
        setNumberSegs(segs, displayHour*100 + displayMinutes, -1, true);
      }
      sevseg.setSegments(segs);

      break;
  }

}

// ----------------------------------------------------------
//                   Sleep Logic
// ----------------------------------------------------------
void goToSleep() {
  // Blank display & set brightness lower if desired
  sevseg.blank();
  sevseg.refreshDisplay();

  digitalWrite(17, LOW);
  watchState = SLEEPING;

  // Example low-level sleep (on AVR):
  ADCSRA &= ~(1 << 7);  // Disable ADC
  SMCR |= (1 << 2);     // Power-down mode bit
  SMCR |= 1;            // Enable sleep
  MCUCR |= (3 << 5);    // BOD disable (bits 5 & 6)
  MCUCR = (MCUCR & ~(1 << 5)) | (1 << 6);
  __asm__ __volatile__("sleep");
}

// ----------------------------------------------------------
//                Interrupt for Wake Button
// ----------------------------------------------------------
void isrWake() {
  // This is triggered by BUTTON_UP_PIN falling
  lastInteraction = millis();
  digitalWrite(17, HIGH);
  if (watchState == SLEEPING) {
    watchState = SHOW_TIME;
    readDateVars();
  }
}

void readDateVars() {
  now = Rtc.GetDateTime();
  hour = now.Hour();
  minute = now.Minute();
  day = now.Day();
  month = now.Month();
  year = now.Year();
  dayOfWeek = now.DayOfWeek();
}

// ----------------------------------------------------------
//                Button management
// ----------------------------------------------------------

void buttonInit(struct Button* button) {
  pinMode(button->pin, INPUT);
  button->lastReadChange = 0;
  button->previouslyPressed = false;
  button->pressed = false;
  button->pressedEvent = false;
  button->pressedEventTime = 0;
  button->longPressed = false;
  button->longPressedEvent = false;
  button->repeatedEvent = false;
  button->repeatedEventNextTime = 0;
}

void buttonUpdateState(struct Button* button) {
  bool readState;
  // reset event flags.
  button->pressedEvent = false;
  button->longPressedEvent = false;
  button->repeatedEvent = false;

  // We assume pin is active HIGH; adjust if reversed
  readState = (digitalRead(button->pin) == HIGH);
  // Debounce input
  if (button->pressed == readState && currentMillis >= button->lastReadChange + DEBOUNCE_TIME) {
    // button->pressed is stable
    if (!button->pressed) {
      // pin released, reset flags
      if (button->previouslyPressed) {
        button->previouslyPressed = false;
        button->pressedEventTime = 0;
        button->longPressed = false;
        button->longPressedEvent = false;
        button->repeatedEvent = false;
        button->repeatedEventNextTime = 0;
      }

    } else {
      // Button is pressed!
      if (!button->previouslyPressed) {
        // was just pressed.
        button->previouslyPressed = true;
        button->pressedEvent = true;
        button->pressedEventTime = currentMillis;

      } else if (!button->longPressed && currentMillis >= button->pressedEventTime + LONG_PRESS_DELAY) {
        button->longPressed = true;
        button->longPressedEvent = true;
        button->repeatedEventNextTime = currentMillis + REPEATED_EVENT_INTERVAL;
      
      } else if (button->longPressed && currentMillis >= button->repeatedEventNextTime) {
        button->repeatedEvent = true;
        button->repeatedEventNextTime = currentMillis + REPEATED_EVENT_INTERVAL;
      }
    }
  } else {
    // readState changed
    if (button->pressed != readState) {
      button->pressed = readState;
      button->lastReadChange = currentMillis;
    }
  }
}
