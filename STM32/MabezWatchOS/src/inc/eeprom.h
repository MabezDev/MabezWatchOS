#ifndef __M_EEPROM__
#define __M_EEPROM__

#include <Arduino.h>
#include <EEPROM.h>

short readFromEEPROM(short address);
long EEPROMReadlong(long address);
void EEPROMWritelong(int address, long value);
void saveToEEPROM(short address, short value);


#endif