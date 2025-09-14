#ifndef TPWTIMECOMMONS_H_INCLUDED
#define TPWTIMECOMMONS_H_INCLUDED

const byte weekDays[7][4] = {
  { 0x6D, 0x1C, 0x54, 0x00 },  // 0: SundAY   = 0x2D 0x1C 0x54 0x5E 0x77 0x6E
  { 0x37, 0x5C, 0x54, 0x00 },  // 1: MondAY   = 0x37 0x5C 0x54 0x5E 0x77 0x6E
  { 0x78, 0x1C, 0x79, 0x00 },  // 2: tuESdaY  = 0x78 0x1C 0x79 0x2D 0x5E 0x77 0x6E
  { 0x3E, 0x79, 0x5E, 0x00 },  // 3: WednSdAY = 0x3E 0x17 0x5E 0x54 0x2D 0x5E 0x77 0x6E
  { 0x78, 0x74, 0x1C, 0x00 },  // 4: thurSdAY = 0x78 0x74 0x1C 0x50 0x2D 0x5E 0x77 0x6E
  { 0x71, 0x50, 0x10, 0x00 },  // 5: FridAY   = 0x71 0x50 0x10 0x5E 0x77 0x6E
  { 0x6D, 0x77, 0x78, 0x00 }   // 6: SAturdAY = 0x2D 0x77 0x78 0x1C 0x50 0x5E 0x77 0x6E
};

bool isLeapYear(int year) {
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
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

#endif
