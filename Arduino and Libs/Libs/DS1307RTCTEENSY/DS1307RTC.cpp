/*
 * DS1307RTC.h - library for DS1307 RTC
  
  Copyright (c) Michael Margolis 2009
  This library is intended to be uses with Arduino Time library functions

  The library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
  
  30 Dec 2009 - Initial release
  5 Sep 2011 updated for Arduino 1.0
 */


#if defined (__AVR_ATtiny84__) || defined(__AVR_ATtiny85__) || (__AVR_ATtiny2313__)
#include <TinyWire1M.h>
#define Wire1 TinyWire1M
#else
#include <i2c_t3.h>
#endif
#include "DS1307RTC.h"

#define DS1307_CTRL_ID 0x68 

DS1307RTC::DS1307RTC()
{
  Wire1.begin();
}
  
// PUBLIC FUNCTIONS
time_t DS1307RTC::get()   // Aquire data from buffer and convert to time_t
{
  tmElements_t tm;
  if (read(tm) == false) return 0;
  return(makeTime(tm));
}

bool DS1307RTC::set(time_t t)
{
  tmElements_t tm;
  breakTime(t, tm);
  return write(tm); 
}

// Aquire data from the RTC chip in BCD format
bool DS1307RTC::read(tmElements_t &tm)
{
  uint8_t sec;
  Wire1.beginTransmission(DS1307_CTRL_ID);
#if ARDUINO >= 100  
  Wire1.write((uint8_t)0x00); 
#else
  Wire1.send(0x00);
#endif  
  if (Wire1.endTransmission() != 0) {
    exists = false;
    return false;
  }
  exists = true;

  // request the 7 data fields   (secs, min, hr, dow, date, mth, yr)
  Wire1.requestFrom(DS1307_CTRL_ID, tmNbrFields);
  if (Wire1.available() < tmNbrFields) return false;
#if ARDUINO >= 100
  sec = Wire1.read();
  tm.Second = bcd2dec(sec & 0x7f);   
  tm.Minute = bcd2dec(Wire1.read() );
  tm.Hour =   bcd2dec(Wire1.read() & 0x3f);  // mask assumes 24hr clock
  tm.Wday = bcd2dec(Wire1.read() );
  tm.Day = bcd2dec(Wire1.read() );
  tm.Month = bcd2dec(Wire1.read() );
  tm.Year = y2kYearToTm((bcd2dec(Wire1.read())));
#else
  sec = Wire1.receive();
  tm.Second = bcd2dec(sec & 0x7f);   
  tm.Minute = bcd2dec(Wire1.receive() );
  tm.Hour =   bcd2dec(Wire1.receive() & 0x3f);  // mask assumes 24hr clock
  tm.Wday = bcd2dec(Wire1.receive() );
  tm.Day = bcd2dec(Wire1.receive() );
  tm.Month = bcd2dec(Wire1.receive() );
  tm.Year = y2kYearToTm((bcd2dec(Wire1.receive())));
#endif
  if (sec & 0x80) return false; // clock is halted
  return true;
}

bool DS1307RTC::write(tmElements_t &tm)
{
  // To eliminate any potential race conditions,
  // stop the clock before writing the values,
  // then restart it after.
  Wire1.beginTransmission(DS1307_CTRL_ID);
#if ARDUINO >= 100  
  Wire1.write((uint8_t)0x00); // reset register pointer  
  Wire1.write((uint8_t)0x80); // Stop the clock. The seconds will be written last
  Wire1.write(dec2bcd(tm.Minute));
  Wire1.write(dec2bcd(tm.Hour));      // sets 24 hour format
  Wire1.write(dec2bcd(tm.Wday));   
  Wire1.write(dec2bcd(tm.Day));
  Wire1.write(dec2bcd(tm.Month));
  Wire1.write(dec2bcd(tmYearToY2k(tm.Year))); 
#else  
  Wire1.send(0x00); // reset register pointer  
  Wire1.send(0x80); // Stop the clock. The seconds will be written last
  Wire1.send(dec2bcd(tm.Minute));
  Wire1.send(dec2bcd(tm.Hour));      // sets 24 hour format
  Wire1.send(dec2bcd(tm.Wday));   
  Wire1.send(dec2bcd(tm.Day));
  Wire1.send(dec2bcd(tm.Month));
  Wire1.send(dec2bcd(tmYearToY2k(tm.Year)));   
#endif
  if (Wire1.endTransmission() != 0) {
    exists = false;
    return false;
  }
  exists = true;

  // Now go back and set the seconds, starting the clock back up as a side effect
  Wire1.beginTransmission(DS1307_CTRL_ID);
#if ARDUINO >= 100  
  Wire1.write((uint8_t)0x00); // reset register pointer  
  Wire1.write(dec2bcd(tm.Second)); // write the seconds, with the stop bit clear to restart
#else  
  Wire1.send(0x00); // reset register pointer  
  Wire1.send(dec2bcd(tm.Second)); // write the seconds, with the stop bit clear to restart
#endif
  if (Wire1.endTransmission() != 0) {
    exists = false;
    return false;
  }
  exists = true;
  return true;
}

unsigned char DS1307RTC::isRunning()
{
  Wire1.beginTransmission(DS1307_CTRL_ID);
#if ARDUINO >= 100  
  Wire1.write((uint8_t)0x00); 
#else
  Wire1.send(0x00);
#endif  
  Wire1.endTransmission();

  // Just fetch the seconds register and check the top bit
  Wire1.requestFrom(DS1307_CTRL_ID, 1);
#if ARDUINO >= 100
  return !(Wire1.read() & 0x80);
#else
  return !(Wire1.receive() & 0x80);
#endif  
}

void DS1307RTC::setCalibration(char calValue)
{
  unsigned char calReg = abs(calValue) & 0x1f;
  if (calValue >= 0) calReg |= 0x20; // S bit is positive to speed up the clock
  Wire1.beginTransmission(DS1307_CTRL_ID);
#if ARDUINO >= 100  
  Wire1.write((uint8_t)0x07); // Point to calibration register
  Wire1.write(calReg);
#else  
  Wire1.send(0x07); // Point to calibration register
  Wire1.send(calReg);
#endif
  Wire1.endTransmission();  
}

char DS1307RTC::getCalibration()
{
  Wire1.beginTransmission(DS1307_CTRL_ID);
#if ARDUINO >= 100  
  Wire1.write((uint8_t)0x07); 
#else
  Wire1.send(0x07);
#endif  
  Wire1.endTransmission();

  Wire1.requestFrom(DS1307_CTRL_ID, 1);
#if ARDUINO >= 100
  unsigned char calReg = Wire1.read();
#else
  unsigned char calReg = Wire1.receive();
#endif
  char out = calReg & 0x1f;
  if (!(calReg & 0x20)) out = -out; // S bit clear means a negative value
  return out;
}

// PRIVATE FUNCTIONS

// Convert Decimal to Binary Coded Decimal (BCD)
uint8_t DS1307RTC::dec2bcd(uint8_t num)
{
  return ((num/10 * 16) + (num % 10));
}

// Convert Binary Coded Decimal (BCD) to Decimal
uint8_t DS1307RTC::bcd2dec(uint8_t num)
{
  return ((num/16 * 10) + (num % 16));
}

bool DS1307RTC::exists = false;

DS1307RTC RTC = DS1307RTC(); // create an instance for the user

