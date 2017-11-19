
#ifndef __M_GLOBAL__
#define __M_GLOBAL__

#include<Arduino.h>
#include <Arduino.h>
#include <Wire.h>
#include <RTClock.h>

//fonts
#include <Adafruit_GFX.h>
#include "Fonts/myfont.h"

// #include<MAX17043.h> // will be using this as our lipo monitor
#include <Adafruit_SH1106.h>

#define HWSERIAL Serial2 //change back to Serial2 when we use bluetooth // PA2 & PA3

// Leap year calulator expects year argument as years offset from 1970
#define LEAP_YEAR(Y) (((1970 + Y) > 0) && !((1970 + Y) % 4) && (((1970 + Y) % 100) || !((1970 + Y) % 400)))
#define SECS_PER_MIN (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY (SECS_PER_HOUR * 24UL)
#define FONT_WIDTH 5
#define FONT_HEIGHT 7

// time/date structure
typedef struct TimeElements
{
    uint8_t Second;
    uint8_t Minute;
    uint8_t Hour;
    uint8_t Wday; // Day of week, sunday is day 1
    uint8_t Day;
    uint8_t Month;
    uint8_t Year; // Offset from 1970;
} TimeElements;

TimeElements tm; // real time clock time structure

//notification vars
short notificationIndex = 0;
const short notificationMax = 30; // can increase this or increase the text size as we have 20kb RAM on the STM32

//TODO: increase the size of notification text, maybe use a set of small, large and medium text sizes in a class like structure, not sure if I can do that with structs, if not I will use classes

typedef struct
{
    char packageName[15];
    char title[15];
    short dateReceived[2];
    short textLength;
    //char text[250];
    char *textPointer; //points to a char array containing the text, replaces the raw text
    short textType;    // used to find or remove in the correct array
    int id;
} Notification;

Notification notifications[notificationMax];
bool wantNotifications = true; // used to tell the app that we have no more room for notifications and it should hold them in a queue
bool shouldRemove = false;     //  used to remove notifications once read

//navigation constants
const short PROGMEM HOME_PAGE = 0;
const short PROGMEM NOTIFICATION_MENU = 1;
const short PROGMEM NOTIFICATION_BIG = 2;
const short PROGMEM TIMER = 4;
const short PROGMEM SETTINGS = 5;
const short PROGMEM ALARM_PAGE = 6;
const short PROGMEM ALERT = 10;

//UI constants
const short PROGMEM MENU_ITEM_HEIGHT = 16;

//navigation vars
short pageIndex = 0;
short menuSelector = 0;
short widgetSelector = 3;
const short numberOfNormalWidgets = 6; // actually 3, 0,1,2.
const short numberOfDebugWidgets = 1;
short numberOfWidgets = 0;
short numberOfPages = 6; // actually 7 (0-6 = 7)

const short x = 6;
short y = 0;
short Y_OFFSET = 0;

short lineCount = 0;
short currentLine = 0;

//batt monitoring - soon to be redundant just the charging flag required for the tp4056 led output as were using a MAX17043 dedicateed fuel gauge
float batteryVoltage = 0;
short batteryPercentage = 0;
bool isCharging = false;
bool prevIsCharging = false;
bool isCharged = false;
bool started = false; //flag to identify whether its the start or the end of a charge cycle

//timer variables
short timerArray[3] = {0, 0, 0}; // h/m/s
bool isRunning = false;
short timerIndex = 0;

bool locked = false;

//connection
bool isConnected = false;
short connectedTime = 0;

//settings
const short numberOfSettings = 2;
String PROGMEM settingKey[numberOfSettings] = {"Favourite Widget:", "Debug Widgets:"};
const short PROGMEM settingValueMin[numberOfSettings] = {0, 0};
const short PROGMEM settingValueMax[numberOfSettings] = {numberOfPages, 1};
short settingValue[numberOfSettings] = {0, 0}; //default

//alert popup
short lastPage = -1;
char alertText[20]; //20 chars that fit
short alertTextLen = 0;
short alertVibrationCount = 0;
bool vibrating = false;
long prevAlertMillis = 0;
// only supports the timestamp for one alert currently
short alertRecieved[2] = {0, 0};

short loading = 3; // time the loading screen is show for

//drawing buffers used for character rendering
char numberBuffer[4];          //2 numbers //cahnged to 3 till i find what is taking 3 digits and overflowing
const short charsThatFit = 20; //only with default font. 0-20 = 21 chars
char lineBuffer[21];           // 21 chars                             ^^
short charIndex = 0;

//alarm vars
typedef struct Alm
{
    short alarmTime[3] = {0, 0, 0};
    uint32_t time;
    bool active = false;
    String name;
} Alm;

Alm alarms[2];
TimeElements alarmTm;                                // used for calculating alarm time
const short PROGMEM alarmMaxValues[3] = {23, 59, 6}; // 23 hours in advance, 59 minutes in advance before it increases the hours, 6 days in advance
short alarmToggle = 0;                               // alarm1 = 0, alarm2 = 1
short prevAlarmToggle = 1;
short alarmIndex = 0;
const short ALARM_ADDRESS = 20;    //start at 20,
const short ALARM_DATA_LENGTH = 5; // five bytes required per alarm, 1 byte for active and 4 bytes that store a long of the alarm time in seconds

//used for power saving
bool idle = false;

#endif