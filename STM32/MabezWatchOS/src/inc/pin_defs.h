#ifndef __M_PIN_DEFS__
#define __M_PIN_DEFS__

#include<Arduino.h>

//pin constants - most of these will need to be adjusted for the STM32F1
const short PROGMEM OK_BUTTON = PB3;
const short PROGMEM DOWN_BUTTON = PB4;
const short PROGMEM UP_BUTTON = PB5;
const short PROGMEM BATT_READ = 0;
const short PROGMEM VIBRATE_PIN = 10;
const short PROGMEM CHARGING_STATUS_PIN = 9;
const short PROGMEM BT_POWER = 21;


#endif