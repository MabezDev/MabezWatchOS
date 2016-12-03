#include<EEPROM.h>

// Leap year calulator expects year argument as years offset from 1970
#define LEAP_YEAR(Y)  ( ((1970+Y)>0) && !((1970+Y)%4) && ( ((1970+Y)%100) || !((1970+Y)%400) ) )
#define SECS_PER_MIN  (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24UL)


typedef struct TimeElements
{ 
  uint8_t Second; 
  uint8_t Minute; 
  uint8_t Hour; 
  uint8_t Wday;   // Day of week, sunday is day 1
  uint8_t Day;
  uint8_t Month; 
  uint8_t Year;   // Offset from 1970; 
} TimeElements ; 

uint32_t dateTime_t;
TimeElements  tm;

const short PROGMEM dayInMonth[12] = {31,28,31,30,31,30,31,31,30,31,30,31}; //does not account for leap year

void setup() {
  Serial.begin(9600);
  EEPROM.write(20,2016);
  

}

void loop() {
//  breakTime(28,tm);
//  Serial.print("Time: ");
//  Serial.print(tm.Hour);
//  Serial.print(":");
//  Serial.print(tm.Minute);
//  Serial.print(":");
//  Serial.print(tm.Second);
//  Serial.println();
//  tm.Hour = 2;
//  tm.Minute = 4;
//  tm.Second = 0;
//  tm.Day = 4;
//  tm.Month = 1;
//  tm.Year = 0;
  tm.Hour = 0;
  tm.Minute = 7;
  tm.Second = 0;
  tm.Day = 1;
  tm.Month = 1;
  tm.Year = 0;
  Serial.println(makeTime(tm));

  delay(2000);
}


void breakTime(uint32_t timeInput, struct TimeElements &tm){
// Break the given time_t into time components
// This is a more compact version of the C library localtime function
// Note that year is offset from 1970 !!!

  uint8_t year;
  uint8_t month, monthLength;
  uint32_t time;
  unsigned long days;

  time = (uint32_t)timeInput;
  tm.Second = time % 60;
  time /= 60; // Now it is minutes
  tm.Minute = time % 60;
  time /= 60; // Now it is hours
  tm.Hour = time % 24;
  time /= 24; // Now it is days
  tm.Wday = ((time + 4) % 7) + 1;  // Sunday is day 1 
  
  year = 0;  
  days = 0;
  while((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= time) {
    year++;
  }
  tm.Year = year; // Year is offset from 1970 
  
  days -= LEAP_YEAR(year) ? 366 : 365;
  time  -= days; // Now it is days in this year, starting at 0
  
  days=0;
  month=0;
  monthLength=0;
  for (month=0; month<12; month++) {
    if (month==1) { // February
      if (LEAP_YEAR(year)) {
        monthLength=29;
      } else {
        monthLength=28;
      }
    } else {
      monthLength = dayInMonth[month];
    }
    
    if (time >= monthLength) {
      time -= monthLength;
    } else {
        break;
    }
  }
  tm.Month = month + 1;  // Jan is month 1  
  tm.Day = time + 1;     // Day of month
}


uint32_t makeTime(struct TimeElements &tm){   
// Assemble time elements into "unix" format 
// Note year argument is offset from 1970
  
  int i;
  uint32_t seconds;

  // Seconds from 1970 till 1 jan 00:00:00 of the given year
  seconds= tm.Year*(SECS_PER_DAY * 365);
  for (i = 0; i < tm.Year; i++) {
    if (LEAP_YEAR(i)) {
      seconds +=  SECS_PER_DAY;   // Add extra days for leap years
    }
  }
  
  // Add days for this year, months start from 1
  for (i = 1; i < tm.Month; i++) {
    if ( (i == 2) && LEAP_YEAR(tm.Year)) { 
      seconds += SECS_PER_DAY * 29;
    } else {
      seconds += SECS_PER_DAY * dayInMonth[i-1];  // MonthDay array starts from 0
    }
  }
  seconds+= (tm.Day-1) * SECS_PER_DAY;
  seconds+= tm.Hour * SECS_PER_HOUR;
  seconds+= tm.Minute * SECS_PER_MIN;
  seconds+= tm.Second;
  return (uint32_t)seconds; 
}
