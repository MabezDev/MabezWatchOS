#ifndef __M_SYSTEM__
#define __M_SYSTEM__

#include <Arduino.h>
#include "inc/globals.h"
#include "inc/eeprom.h"
#include "inc/pin_defs.h"
#include <itoa.h>

/*
* System methods
*/

void setAlarm(short alarmType, short hours, short minutes, short date, short month, short year);
void vibrate(short vibrationTime);
void drawTriangle(short x, short y, short size, short direction);
void setClockTime(short hours, short minutes, short seconds, short days, short months, short years);
void resetBTModule();
void removeNotificationById(int id);
void removeNotification(short pos);
float getBatteryVoltage();
void setText(char *c, char *textPtr, short len);

void breakTime(uint32_t timeInput, struct TimeElements &tm);
uint32_t makeTime(struct TimeElements &tm);
void createAlert(char const *text, short len, short vibrationTime);

void vibrate(short vibrationTime);
void drawTriangle(short x, short y, short size, short direction);
void setClockTime(short hours, short minutes, short seconds, short days, short months, short years);
short getCheckSum(char initPayload[], short pIndex, bool startPacket);
void completeReset(bool sendReset);

#endif