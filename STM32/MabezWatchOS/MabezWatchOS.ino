#include <Arduino.h>
#include <RTClock.h> 
#include <EEPROM.h>
// #include <Time.h> not required
#include <itoa.h>


//fonts
#include<Adafruit_GFX.h>
#include "Fonts/myfont.h"

// #include<MAX17043.h> // will be using this as our lipo monitor
#include<Adafruit_SH1106.h>

#define HWSERIAL Serial2 //change back to Serial2 when we use bluetooth // PA2 & PA3

// Leap year calulator expects year argument as years offset from 1970
#define LEAP_YEAR(Y)  ( ((1970+Y)>0) && !((1970+Y)%4) && ( ((1970+Y)%100) || !((1970+Y)%400) ) )
#define SECS_PER_MIN  (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24UL)

//needed for calculating Free RAM on ARM based MC's
#ifdef __arm__
  extern "C" char* sbrk(short incr);
#else  // __ARM__
  extern char *__brkval;
  extern char __bss_end;
#endif  // __arm__

//u8g lib object without the c++ wrapper due to lack of support of the OLED
//// u8g_t u8g;

// display object for sh11106
Adafruit_SH1106 display(0);

RTClock rt (RTCSEL_LSE);  // Initialise RTC with LSE

/*
 * This has now been fully updated to the teensy LC, with an improved transmission algorithm.

   Currently with no power saving techniques the whole thing runs on just 28millisamps, therfore
        240 mah battery / 28ma current draw = 8.5 hours of on time
   Battery Update: Left to discharge whilst connected to the App, we got 8 hours and 22 minutes, very close to the 8 hours 30mins we calculated
 *
 * Update:
 *  -(04/04/16)UI Improved vastly.
 *  -(19/04/16)Fixed notification sizes mean we can't SRAM overflow, leaves about 1k of SRAM for dynamic allocations i.e message or data (See todo for removal of all dynamic allocations)
 *  -(25/04/16)Fixed RTC not setting/receiving the year
 *  -(26/04/16)Added a menu/widget service for the main page
 *  -(27/04/16)Fixed RTC showing AM instead of PM and visa versa
 *  -(28/04/16)OLED has ceased to work, might have killed it with static, as I don't see how it could have broken just like that. When new oled arrives continue with the timer app,
 *             will work on capacitive touch buttons, the app and other stuff that doesn't require the use of the OLED.
 *  -(19/05/16)This firmware now only uses char arrays(No more heap fragmentation!) and now can support the detction of multibutton presses
 *  -(19/05/16)New OLED, finished the timer app's basic functionality.
 *  -(20/05/16) Settings page is impemented but need to add the eeprom storage functionality
 *  -(22/05/16) Added genric function to display a menu, we use a function as a parameter and that function displays one menu item
    -(25/05/16) Code efficency improvements. Again cleaned out all usages of String (except progmem constants), in theory we should never crash from running out of memory
                - As of this update, we have 815 bytes of RAM, and about 10K of progmem available on Teensy LC
    -(25/05/16) Added vibration system methods
    -(26/05/16) [MAJOR] We now tell the app to hold the notifications in a queue till we are ready to read them, this solves all the memory concerns had
    -(26/05/16) Switched from DS1302 RTC to smaller, better DS3231
    -(30/05/16) Added battery charging algorithm, works well enough to know when a single lithium cell is charged
    -(30/05/16) Added alarm page and alarm functionality for both alarms available on the DS3231
    -(31/05/16) The Tp4056 charger board I am using uses a open collector system, after running a wire directly from the TP4056 charge indicator pin, into pulled up pin
                and it works! Luckily we draw less C/10 (Capacity of the battery/10) so the charge can terminate successfully!
    -(31/05/16) Switched all (bar one) ints to shorts and gather about 230 bytes of RAM extra, and saved storage space too

    -(30/11/2016) Moving to STM32 platform, starting conversion after weeks of trying to get the oled to play with it. Finally acheived that tonight but we need to switch to new GFX lib, should be a reasonalbel easy swap (hopefully)
    -(01/12/2016) All software functionality back to previous state, need to hook up bluetooth and figure out a good way to do our own alarms and figure out capactive touch buttons?
    -(02/12/2016) Fixed all functionality witht he RTC, checked and the main loop still receives messages perfectly, waiting for hm-11 to do a full test with the app
    -(10/12/2016) Added software alarm system, and fixed various bugs witht he previous system
    -(01/01/2017) Added a new notification storage system that I have been working on, allows for a semi dynamic use of space, everything is still statically allocated but smaller messages are put in a small text array etc
 *
 *  Buglist:
    -midnight on the digital clock widget produces three zeros must investigate
        - might be due to the fact a itoa func make a number wwith 3 characters (numberBuffer overflow)
 *
 *
 *  Todo:
 *    -Use touch capacitive sensors instead of switches to simplify PCB and I think it will be nicer to use. - [DONE]
 *    -Maybe use teensy 3.2, more powerful, with 64k RAM, and most importantly has a RTC built in [OPTIONAL]
 *    -add reply phrases, if they are short answers like yeah, nah, busy right now etc. - [MINOR]
 *    -Could use a on screen keyboard but it might be too much hassle to use properly. - [OPTIONAL]
 *    -text wrapping on the full notification page - [NOT POSSIBLE], we dont have enough ram to play with to textwrap
 *    -add a input time out where if the user does not interact with the watch the widget goes back to a clock one - [DONE]
          - change the widget to the favorite stored in the settings - [DONE]
 *    - fix the alert function to have two pulses without delay (Fix could be aleRTCounter set it to 2 then vibrate and -- from aleRTCounter) - [DONE]
 *    - use isPrintable on a char to find out if we can send it (maybe do thios on the phone side) - [NEED TESTING]
 *    - finish timer app (use our system click to time the seconds) - [DONE]
          - need to alert the suer when the timer is up - DONE
          - make it look better - [DONE]
 *    -settings page
          - favourite widget (will default to once no input is recieved) - DONE
 *        -use eeprom to store the settings so we dont have to set them - DONE
 *    -the lithium charger board has a two leds one to show charging one to show chargin done, need to hook into these to read when we are charging(to charge the RTC  batt(trickle charge))
       and to change the status on the watch to charging etc - [DONE]
 *    - add more weatherinfo screen where we can see the forecast for the next days or hours etc - [OPTIONAL]
      - tell the user no messages found on the messages screen - [DONE]
      - write a generic menu function that takes a function as a parameter, the function will write 1 row of data - [DONE]
      - Sometime the BT module can get confused (or the OS? or is the app?) and we can't connect till we rest the Watch - [DONE]
          - Pin 11 (on the HM-11) held low for 100ms will reset the HM-11 Module.
          - or send AT+RESET to reset it aswell
      - request data from phone?, tell phone we have read a notification?
          - App is ready to recieve data from Module, just need to figure what we want
              - Maybe instead of randomly sending the weather data we coudl request it instead?
                  -request 5 day forecast?
              - request phone battery level
              -  we could tell the phone we have ran out of mem, so keep the rest of the notifications in the queue till there is space [DONE]
      - Add time stamp for notifications? - (Would need to modify notification structure but we dont need any data from the phone use out RTC),
              we have space due to the 15 char limit on the titles - [DONE]
      - Keep tabs on Weather data to make sure its still valid (make sure were not displaying data that is hours old or something) - [DONE]
      - add otion for alert to go away after x amount of seconds?
      - F() all strings - [DONE] (But makes no difference on ARM based MC's)
      - store tags like <i> as progmem constants for better readablity
      - Switch to DS3231 RTC - [DONE]  but we need to implement our methods to get temperature, and set alarms by translating code from another lib
          - Use the alarm functionality of this module to create an alarm page( only 2 alarms though I Believe) - [DONE]
          - We can also get the temperature outside with it
      - add a battery charging widget which shows more info about the battery chargin like its current percent (display a graphic?) - [NOT POSSIBLE] we don't have that data to display
      - implement a CPU clockdown when idle
          - will need to account for the slower clocks when doing timing events
      - once weve built it, turn off usb power
      - code efficeincy could be improved by only using one index variable instead of an indx for each page as only on page can be accessed at a time
          - also change ints to shorts when the length is not needed
      - alarm page started:
          - need work out how to make sure the date is a valid date, may need to use the RTC lib make time to get valid date
          - store alarm deets in eeprom -DONE
          - add alarm selector fucntionality -DONE
          // -replace the daysInMNonth array by using the RTC make time functinality?
      - The snooze lib too big and not really what I was looking for, I wanted dynamic CPU freq changes,
          so clock down to 8mhz or something when we havent't interacted with the watch, then ramp up when we are interacting, but this will cause problems with serial comms I think
      - add software turn on/off?
          - instead of actually powering off, hibernate (using snooze lib), in this mode we will only draw micro amps, add interrupt on the OK button to wake up
          - hibernate current = 240/0.344 = 697 Hours, need to power BT moudle from a pin, and turn off before going into low power, also blank the oled so no current draw there
             - bt module draws about 7.5  ma, so we need as high current pin, probably pin 5
      - add hardware power on/off

      - STM32 ToDo Specfifics
        - http://stm32duino.com/viewtopic.php?t=610 - implement some power saving techniques like the ones here
        - add alarm symbol on homepage to show its set
        - currently alarms can be set in the past, once we have a back up battery, implement checks to stop this
        - second alarm not saving or loaded to eeprom correctly, investigate[DONE]
        - Add a toggle to have a same alarm everyday
        - correct the display of the loading logo
        - add support for holding down a button to keep increasing it
      - STM32 Buglist
        - if a <i> is not correclt ydetected the whole message is messed up, need to separate tags and maybe think about a ok packet sent back to the app
        - After removing a notifications, all other notification titles are blank on the NOTIFICATION_MENU page - [FIXED]
        - sometimes removing notifications does not wipe the text! Not sure why must investigate further


 */

//input vars
bool button_ok = false;
bool lastb_ok = false;
bool button_up = false;
bool lastb_up = false;
bool button_down = false;
bool lastb_down = false;

#define CONFIRMATION_TIME 0 //80 change back when we go back to capactive //length in time the button has to be pressed for it to be a valid press
#define INPUT_TIME_OUT 60000 //60 seconds
#define TOUCH_THRESHOLD 1200 // value will change depending on the capacitance of the material

//need to use 4,2,1 as no combination of any of the numbers makes the same number, where as 1,2,3 1+2 = 3 so there is no individual state.
#define UP_ONLY  4
#define OK_ONLY  2
#define DOWN_ONLY  1
#define UP_OK  (UP_ONLY|OK_ONLY)
#define UP_DOWN  (UP_ONLY|DOWN_ONLY)
#define DOWN_OK  (OK_ONLY|DOWN_ONLY)
#define ALL_THREE (UP_ONLY|OK_ONLY|DOWN_ONLY)
#define NONE_OF_THEM  0

#define FONT_WIDTH 5
#define FONT_HEIGHT 7 

#define isButtonPressed(pin)  (digitalRead(pin) == LOW) //this was old with buttons
//#define isButtonPressed(pin) (touchRead(pin) > TOUCH_THRESHOLD)

short lastVector = 0;
long prevButtonPressed = 0;

//serial retrieval vars
const short MAX_DATA_LENGTH = 500; // sum off all bytes of the notification struct with 50 bytes left for message tags i.e <n>
char payload[300]; // serial read buffer for data segments(payloads)
char data[MAX_DATA_LENGTH];//data set buffer
short dataIndex = 0; //index is required as we dunno when we stop
bool transmissionSuccess = false;
bool receiving = false; // are we currently recieving data?
short checkSum = 0;
char type;

//date contants
String PROGMEM months[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
String PROGMEM days[7] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
const short PROGMEM dayInMonth[12] = {31,28,31,30,31,30,31,31,30,31,30,31}; //does not account for leap year

// time/date structure
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

TimeElements  tm; // real time clock time structure

//weather vars
bool weatherData = false;
char weatherDay[4];
char weatherTemperature[4];
char weatherForecast[25];
short timeWeGotWeather[2] = {0,0};

const short PROGMEM clockRadius = 32;
const short PROGMEM clockUpdateInterval = 1000;

//time and date vars
bool gotUpdatedTime = false;
short clockArray[3] = {0,0,0}; // HH:MM:SS
short dateArray[4] = {0,0,0,0}; // DD/MM/YYYY, last is Day of Week

long prevMillis = 0;

//TODO: increase the size of notification text, maybe use a set of small, large and medium text sizes in a class like structure, not sure if I can do that with structs, if not I will use classes

typedef struct{
  char packageName[15];
  char title[15];
  short dateReceived[2];
  short textLength;
  //char text[250];
  char *textPointer; //points to a char array containing the text, replaces the raw text
  short textType; // used to find or remove in the correct array
} Notification;

const short SMALL = 0;
const short NORMAL = 1;
const short LARGE = 2;

const short MSG_SIZE[3] = {25,250,750};
short textIndexes[3] = {0,0,0};

char SmallText[50][25];
char NormalText[25][250];
char LargeText[5][750];

void *types[3] = {SmallText,NormalText,LargeText}; // store a pointer of each matrix, later to be casted to a char *

//notification vars
short notificationIndex = 0;
const short notificationMax = 30; // can increase this or increase the text size as we have 20kb RAM on the STM32

Notification notifications[notificationMax]; 
bool wantNotifications = true; // used to tell the app that we have no more room for notifications and it should hold them in a queue
bool shouldRemove = false; //  used to remove notifications once read

//pin constants - most of these will need to be adjusted for the STM32F1
const short PROGMEM OK_BUTTON = PB3;
const short PROGMEM DOWN_BUTTON = PB4;
const short PROGMEM UP_BUTTON = PB5;
const short PROGMEM BATT_READ = 0;
const short PROGMEM VIBRATE_PIN = 10;
const short PROGMEM CHARGING_STATUS_PIN = 9;
const short PROGMEM BT_POWER = 21; 

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
short timerArray[3] = {0,0,0}; // h/m/s
bool isRunning = false;
short timerIndex = 0;


bool locked = false;

//connection
bool isConnected = false;
short connectedTime = 0;

//settings
const short numberOfSettings = 2;
String PROGMEM settingKey[numberOfSettings] = {"Favourite Widget:","Debug Widgets:"};
const short PROGMEM settingValueMin[numberOfSettings] = {0,0};
const short PROGMEM settingValueMax[numberOfSettings] = {numberOfPages,1};
short settingValue[numberOfSettings] = {0,0}; //default

//alert popup
short lastPage = -1;
char alertText[20]; //20 chars that fit
short alertTextLen = 0;
short alertVibrationCount = 0;
bool vibrating = false;
long prevAlertMillis = 0;
// only supports the timestamp for one alert currently
short alertRecieved[2] = {0,0};

short loading = 3; // time the loading screen is show for

//drawing buffers used for character rendering
char numberBuffer[4]; //2 numbers //cahnged to 3 till i find what is taking 3 digits and overflowing
const short charsThatFit = 20; //only with default font. 0-20 = 21 chars
char lineBuffer[21]; // 21 chars                             ^^
short charIndex = 0;

//alarm vars
typedef struct Alm {
  short alarmTime[3] = {0,0,0};
  uint32_t time;
  bool active = false;
  String name;
} Alm;

Alm alarms[2];
TimeElements alarmTm; // used for calculating alarm time
const short PROGMEM alarmMaxValues[3] = {23,59,6}; // 23 hours in advance, 59 minutes in advance before it increases the hours, 6 days in advance
short alarmToggle = 0; // alarm1 = 0, alarm2 = 1
short prevAlarmToggle = 1;
short alarmIndex = 0;
const short ALARM_ADDRESS = 20;//start at 20, 
const short ALARM_DATA_LENGTH = 5; // five bytes required per alarm, 1 byte for active and 4 bytes that store a long of the alarm time in seconds

//used for power saving
bool idle = false;


//icons
const byte PROGMEM BLUETOOTH_CONNECTED[] = {
   0x31, 0x52, 0x94, 0x58, 0x38, 0x38, 0x58, 0x94, 0x52, 0x31
};


//Logo for loading
const byte PROGMEM LOGO[] = {
  0x00, 0x00, 0x00, 0x1e, 0x00, 0x0f, 0x3e, 0x80, 0x0f, 0x3e, 0x80, 0x0f,
   0x7e, 0xc0, 0x0f, 0x6e, 0xc0, 0x0e, 0xee, 0xc0, 0x0e, 0xce, 0x60, 0x0e,
   0xce, 0x61, 0x0e, 0xce, 0x71, 0x0e, 0x8e, 0x31, 0x0e, 0x8e, 0x3b, 0x0e,
   0x0e, 0x1b, 0x0e, 0x0e, 0x1f, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
   0x0e, 0x0e, 0x0e, 0x0e, 0x00, 0x0e, 0x0e, 0x00, 0x0e, 0x0e, 0x00, 0x0e,
   0x0e, 0x00, 0x0e, 0x0e, 0x00, 0x0e, 0x0e, 0x00, 0x0e, 0x00, 0x00, 0x00
};

const byte PROGMEM CHARGING[] = {
  0x00, 0x00, 0x00, 0x00, 0xf0, 0x03, 0x10, 0x02, 0x18, 0x1e, 0x1e, 0x02,
   0x1e, 0x02, 0x18, 0x1e, 0x10, 0x02, 0xf0, 0x03, 0x00, 0x00, 0x00, 0x00
};

const byte PROGMEM CHARGED[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x0f, 0x24, 0x09, 0x24, 0x19,
   0x24, 0x19, 0x24, 0x09, 0xfc, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
 };

void setup(void) {
  Serial.begin(9600);
  
  while(!Serial.isConnected());
  
  HWSERIAL.begin(9600);

  display.begin();
  
  pinMode(OK_BUTTON,INPUT_PULLUP);
  pinMode(DOWN_BUTTON,INPUT_PULLUP);
  pinMode(UP_BUTTON,INPUT_PULLUP);

  pinMode(BATT_READ,INPUT);
  pinMode(CHARGING_STATUS_PIN,INPUT_PULLUP);

  pinMode(BT_POWER,OUTPUT);
  pinMode(VIBRATE_PIN,OUTPUT);

  //turn on BT device
  digitalWrite(BT_POWER, HIGH);


  for(short i = 0; i < numberOfSettings; i++){
    settingValue[i] = readFromEEPROM(i);
  }

  //eeprom reset - for first time run
//  for(short j=0; j < 128; j++){
//    saveToEEPROM(j,0);
//  }
  
  alarms[0].active = readFromEEPROM(ALARM_ADDRESS);
  alarms[1].active = readFromEEPROM(ALARM_ADDRESS + ALARM_DATA_LENGTH);
  alarms[0].time = EEPROMReadlong(ALARM_ADDRESS + 1); // plus one to get actual time not the toggle
  alarms[1].time = EEPROMReadlong(ALARM_ADDRESS + ALARM_DATA_LENGTH + 1);
  
  alarms[0].name = "Alarm One";
  alarms[1].name = "Alarm Two";

  alarmTm.Second = 0;

  //cut the connection so we have to reconnect if the watch crashes and resets
  HWSERIAL.print("AT");

  Serial.println(F("MabezWatch OS Loaded!"));

  // test notification through serial - line by line is how our app sends the payloads
  //<n>com.mabezdev<t>Hello<e>Test Message     - Meta data with the rest of the space used by text, <e> signifying the end of meta data
  //<i> this 2nd payloads data                 - 100 byte payloads of data
  //<i> we can always add more payloads
  //<i> with the data interval tag
  //<f>                                        - finsih with the <f> tag
  //getNotification("<n>com.mabezdev<t>HelloLongTit<e>Test Message this 2nd payloads data",64);
}

void drawStr(int x, int y, char const* text){
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(x,y);
  display.print(text);
}

/*
* Draw generic menu method
*/

void showMenu(short numberOfItems,void itemInMenuFunction(short),bool showSelector){
  for(short i=0; i < numberOfItems + 1;i++){
    short startY = 0;
    if(i==menuSelector && showSelector){
        drawStr(0,y+Y_OFFSET+4,">");
    }
    if(i!=numberOfItems){
        itemInMenuFunction(i); //draw our custom menuItem
    } else {
        drawStr(x + 3,y + Y_OFFSET + 4, "Back");
    }
    y += MENU_ITEM_HEIGHT;
    display.drawRect(x,startY,128,y +Y_OFFSET,WHITE);
  }
  y = 0;
}

/*
* System tick method
*/

void updateSystem(){
  long currentInterval = millis();
  if((currentInterval - prevMillis) >= clockUpdateInterval){
    prevMillis = currentInterval;

    //update RTC info
    breakTime(rt.getTime(),tm);
    clockArray[0] = tm.Hour;
    clockArray[1] = tm.Minute;
    clockArray[2] = tm.Second;
    dateArray[0] = tm.Day;
    dateArray[1] = tm.Month;
    dateArray[2] = tm.Year;
    dateArray[3]  = tm.Wday;


    // update battery stuff
    //batteryVoltage = getBatteryVoltage();
    //batteryPercentage = ((batteryVoltage - 3.4)/1.2)*100; // Gives a percentage range between 4.2 and 3 volts, after testing the cut off voltage is 3.4v
                                                          // in furutire shout change to 3.5 and when we get to 0% "Shutdown" (deep sleep)
    batteryPercentage = 89;
    isCharging = !digitalRead(CHARGING_STATUS_PIN);

    if((isCharging != prevIsCharging)){
      if(started){ //flag to find if its the start or the end of a charge
        isCharged = true;
        started = false;
      } else {
        started = true;
      }
      prevIsCharging = isCharging;
    } else if(batteryPercentage <= 95 && isCharged){ //reset the charged flag
      isCharged = false;
    }

    //make sure we never display a negative percentage
    if(batteryPercentage < 0){
      batteryPercentage = 0;
    } else if(batteryPercentage > 100){
      batteryPercentage = 100;
    }

    //check if we have space for new notifications
    if(notificationIndex < (notificationMax - 4) && !wantNotifications){
      Serial.println("Ready to receive notifications.");
      HWSERIAL.print("<n>");
      wantNotifications = true;
    }

    //loading screen counter
    if(loading!=0){
      loading--;
    }

    //timer countdown algorithm
    if(isRunning){
      if(timerArray[2] > 0){
        timerArray[2]--;
      } else {
        if(timerArray[1] > 0){
          timerArray[1]--;
          timerArray[2] = 59;
        } else {
          if(timerArray[0] > 0){
            timerArray[0]--;
            timerArray[1] = 59;
            timerArray[2] = 59;
          } else {
            isRunning = false;
            createAlert("Timer Finished!",15,5);
          }
        }
      }
    }

    Serial.println(F("=============================================="));
    if(isConnected){
      connectedTime++;
      Serial.print(F("Connected for "));
      Serial.print(connectedTime);
      Serial.println(F(" seconds."));
    } else {
      Serial.print(F("Connected Status: "));
      Serial.println(isConnected);
      connectedTime = 0;
    }

    Serial.print("Battery Level: ");
    Serial.print(batteryPercentage);
    Serial.println("%");


    Serial.print("Battery Status: ");
    if(isCharged){
      Serial.println("Fully charged");
    } else if(isCharging){
      Serial.println("Charging");
    } else {
      Serial.println("Running down");
    }

    Serial.print("Idle power save: ");
    if(idle){
      Serial.println("Active");
    } else {
      Serial.println("Not Active");
    }
    
    Serial.print(F("Number of Notifications: "));
    Serial.println(notificationIndex);
    Serial.println("Distribution: ");
    Serial.print("Small: ");
    Serial.print(textIndexes[0]);
    Serial.print(", Normal: ");
    Serial.print(textIndexes[1]);
    Serial.print(", Large: ");
    Serial.println(textIndexes[2]);
    
    Serial.print(F("Time: "));
    Serial.print(clockArray[0]);
    Serial.print(F(":"));
    Serial.print(clockArray[1]);
    Serial.print(F(":"));
    Serial.print(clockArray[2]);
    Serial.print(F("    Date: "));
    Serial.print(dateArray[0]);
    Serial.print(F("/"));
    Serial.print(dateArray[1]);
    Serial.print(F("/"));
    Serial.println(dateArray[2]+1970);

    Serial.print(F("Free RAM:"));
    Serial.println(FreeRam());
    Serial.println(F("=============================================="));

    // check alarms here
    for(int i=0; i < 2; i++){
      if(alarms[i].active){
        // need to check if the current time is greater than our combined alarm time (i.e use makeTime() to calculate which number is greater)
//        Serial.print("Current time: ");
//        Serial.println(makeTime(tm));
//        Serial.print("Alarms time: ");
//        Serial.println(alarms[i].time);
        long difference = makeTime(tm) - alarms[i].time;
        Serial.print(alarms[i].name);
        Serial.print(", Time till alert: ");
        Serial.println(difference);
        //check if alarm has gone off
        if(difference > 0){
          // if the alarm has gone off within the last minute, create and alert else just turn off the alarm
          if(difference <= 60){
            createAlert(alarms[i].name.c_str(),9,10);
            Serial.print(alarms[i].name);
            Serial.println(" HAS GONE OFF");
            Serial.println("Creating alert!");
          } 
          alarms[i].active = false;
          short address = i == 0 ? ALARM_ADDRESS : ALARM_ADDRESS + ALARM_DATA_LENGTH;
          saveToEEPROM(address,alarms[i].active);
        }
      }
    }

    // send data back to the app about the system
//    HWSERIAL.print("<b>");
//    HWSERIAL.print(batteryPercentage);
//    HWSERIAL.print(",");
//    HWSERIAL.print(batteryVoltage);
//    HWSERIAL.print(",");
//    HWSERIAL.print(isCharging);
  }

  if(((currentInterval - prevAlertMillis) >= 250) && alertVibrationCount > 0){ //quarter a second
    prevAlertMillis = currentInterval;
    vibrating = !vibrating;
    alertVibrationCount--;
    if(vibrating){
      digitalWrite(VIBRATE_PIN, HIGH);
    } else {
      digitalWrite(VIBRATE_PIN, LOW);
    }
    if(alertVibrationCount == 0 && vibrating){ //make we don't leave it vibrating
      vibrating = false;
    }
  }
}

/*
* Main Loop
*/

void loop(void) {
    //start picture loop
    display.clearDisplay();
    {
      // all draw methods must be called between clearDisplay() and display()
      if(loading > 0){
        display.drawBitmap(55,12,LOGO,21,24,1); // logo is draw weirdly atm, need to fix
        drawStr(42,55,"Loading...");
      } else {
        switch(pageIndex){
          case 0: homePage(clockArray[0],clockArray[1],clockArray[2]); break;
          case 1: notificationMenuPage(); break;
          case 2: notificationFullPage(menuSelector); break;
          case 4: timerPage(); break;
          case 5: settingsPage(); break;
          case 6: alarmPage(); break;
          case 10: alertPage(); break;
        }
      }
    }
    display.display();  // end picture loop by displaying the data we changed
   
    handleInput();

    short payloadIndex = 0;
  while(HWSERIAL.available()){
      payload[payloadIndex] = char(HWSERIAL.read()); //store char from serial command
      payloadIndex++;
      if(payloadIndex > 300){
        Serial.println(F("Error message overflow, flushing buffer and discarding message."));
        Serial.print("Before discard: ");
        Serial.println(payload);
        memset(payload, 0, sizeof(payload)); //resetting array
        payloadIndex = 0;
        break;
      }
      //delay(1);
      //delayMicroseconds(250);
  }  
  if(!HWSERIAL.available()){
    if(payloadIndex > 0){ // we have recived something
      Serial.print(F("Message: "));
      Serial.println(payload);

      // Handle connection packet from HM-11
      if(startsWith(payload,"OK",2)){
          if(startsWith(payload,"OK+C",4)){
            isConnected = true;
            Serial.println(F("Connected!"));
          } else if(startsWith(payload,"OK+L",4)){
            isConnected = false;
            Serial.println(F("Disconnected!"));
            //reset vars like got updated time and weather here also
          } else {
            //the message was broken and we have no way of knowing if we connected or not so just send the disconnect and try again manually
            Serial.println(F("Error connecting, retry."));
            HWSERIAL.print("AT");
          }
          memset(payload, 0, sizeof(payload));
      }
      
      if(startsWith(payload,"<*>",3)){
        if(!receiving){
          Serial.print("Found a new packet initializer of type ");
          type = payload[3]; // 4th char will always be the same type
          Serial.println(type);
          uint8_t numChars = payloadIndex - 4;
          char checkSumChars[numChars];
          for(int i = 0; i < numChars; i++){
            checkSumChars[i] = payload[i+4];
          }
          checkSum = atoi(checkSumChars);
          Serial.print("Checksum char length: ");
          Serial.println(checkSum);
          receiving = true;
          HWSERIAL.print("<ACK>"); // send acknowledge packet, then app will send the contents to the watch
        } else {
          Serial.println("Recived a new packet init when we weren't expecting one. Resetting all transmission variabels for new packet.");
          completeReset(true);  // tell the app we weren't expecting a packet, so restart completely
        }
      } else if(startsWith(payload,"<!>",3)){
        Serial.println("<!> detected! Reseeting transmission vars.");
        completeReset(false);
      } else {
        // there will be no more <i> tags just chunks of data continuously streamed (less than 100 bytes per payload still though)
        if(receiving){ 
          // now we just add the text to the data till we reach the checksum length
          if(dataIndex + payloadIndex < MAX_DATA_LENGTH){
            for(int j = 0; j < payloadIndex; j++){ // add the payload to the final data
              data[dataIndex] = payload[j];
              dataIndex++;
            }
//            Serial.print("Current final data: ");
//            Serial.println(data);
          } else {
            Serial.println("Error! Final data is full!");
          }
          Serial.print("Data index: ");
          Serial.println(dataIndex);
          if(dataIndex == checkSum){
            if(data[dataIndex - 1] == '*'){ // check the last chars is our checksumChar = *
//              Serial.println();
//              Serial.println("End of message, final contents: ");
//              Serial.println(data);
//              Serial.println();
              dataIndex--; // remove the * checksum char
              data[dataIndex] = '\0'; //hard remove the * checkSum char
              receiving  = false;
              transmissionSuccess = true;
            } else {
              // failed the checkSum, tell the watch to resend
              Serial.println("Checksum failed, asking App for resend.");
              completeReset(true);
            }
          } else if(dataIndex > checkSum){
            // something has gone wrong
            Serial.println("We received more data than we were expecting, asking App for resend.");
            completeReset(true);
          }
        }
      }
      
    }
  }

  if(transmissionSuccess){
    switch(type){
      case 'n':
        if(notificationIndex < (notificationMax - 1)){ // 0-7 == 8
        getNotification(data,dataIndex);
        vibrate(2); //vibrate
        if(notificationIndex > (notificationMax - 2)){ //tell the app we are out of space and hold the notifications
          HWSERIAL.print("<e>");
          wantNotifications = false;
        }
      } else {
        Serial.println(F("Max notifications Hit."));
      }
        break;
      case 'w':
        Serial.println("Weather data processed!");
        getWeatherData(data,dataIndex);
        break;
      case 'd':
        Serial.println("Date/Time data processed!");
        getTimeFromDevice(data,dataIndex);
        break;
      }
    transmissionSuccess = false; //reset once we have given the data to the respective function
    memset(data, 0, sizeof(data)); // wipe final data ready for next notification
    dataIndex = 0; // and reset the index
    
    // finally tell the watch we are ready for a new packet
    HWSERIAL.print("<OK>");
  }
  
  if(payloadIndex > 0){
    memset(payload, 0, sizeof(payload)); //reset payload for next block of text
    payloadIndex = 0;
  }
  
  // update the system
  updateSystem();
}


void completeReset(bool sendFail){
  memset(data, 0, sizeof(data)); // wipe final data ready for next notification
  dataIndex = 0; // and reset the index
  receiving  = false;
  transmissionSuccess = false;
  if(sendFail){
    HWSERIAL.print("<FAIL>");
  }
}

/*
* Page Methods
*/

void alarmPage(){
   drawStr(24,16,"H");
   drawStr(54,16,"M");
   drawStr(84,16,"D");

  intTo2Chars(alarms[alarmToggle].alarmTime[0]);
  drawStr(20,28,numberBuffer);
  intTo2Chars(alarms[alarmToggle].alarmTime[1]);
  drawStr(50,28,numberBuffer);
  //draw day of week set up to a week in advance
  short dayAhead = dateArray[3] - 1; //set current day
  for(short i = 0; i < alarms[alarmToggle].alarmTime[2]; i++){ // add the exra days
    dayAhead++;
    if(dayAhead > 6){
      dayAhead = 0;
    }
  }

  //check if alarms are active
  if(alarmToggle!=prevAlarmToggle){
    for(short k=0; k < 2; k++){
        alarms[alarmToggle].alarmTime[k] = 0;
    }
    short address = 0;
    if(alarmToggle == 0 && alarms[alarmToggle].active){
      address = ALARM_ADDRESS + 1; // + 1 for first value
    } else if(alarmToggle == 1 && alarms[alarmToggle].active) {
      address = ALARM_ADDRESS + ALARM_DATA_LENGTH + 1; // +1 for first value of the second alarm
    }
    
    alarms[alarmToggle].time = EEPROMReadlong(address);
    
    Serial.print(alarms[alarmToggle].name);
    Serial.print(F(" time from EEPROM: "));
    Serial.println(alarms[alarmToggle].time);
    
    breakTime(alarms[alarmToggle].time,alarmTm); // brak the time in seconds down to something the user will understand HH:MM:SS DD/MM/YYYY
    
    alarms[alarmToggle].alarmTime[0] = alarmTm.Hour;
    alarms[alarmToggle].alarmTime[1] = alarmTm.Minute;
    short date = alarmTm.Day;
    short monthSet = alarmTm.Month;
    
    if(date == dateArray[0]){
      alarms[alarmToggle].alarmTime[2] = 0;
    } else {
      alarms[alarmToggle].alarmTime[2] = (date + dayInMonth[monthSet - 1]) - dateArray[0];
    }

    Serial.println(alarms[alarmToggle].name);
    Serial.print("Time: ");
    Serial.print(alarms[alarmToggle].alarmTime[0]);
    Serial.print(":");
    Serial.print(alarms[alarmToggle].alarmTime[1]);
    Serial.println(":0");
    
    Serial.print("Date: ");
    Serial.print(date);
    Serial.print("/");
    Serial.print(monthSet);
    Serial.print("/");
    Serial.print(alarmTm.Year);
    Serial.println();
   
      
    // toggle between 1 and 0
    prevAlarmToggle = alarmToggle;
  }

  drawStr(74,28,days[dayAhead].c_str());

  if(alarmToggle == 0){
    drawStr(0,0 ,"One");
  } else {
    drawStr(0,0 ,"Two");
  }


  if(!alarms[alarmToggle].active){
     drawStr(34,54,"Set");
  } else {
     drawStr(30,54,"Unset");
  }

   drawStr(70,54,"Swap");


  drawSelectors(alarmIndex);
}

void timerPage(){
   drawStr(24,18,"H");
   drawStr(54,18,"M");
   drawStr(84,18,"S");

  intTo2Chars(timerArray[0]);
   drawStr(20,28,numberBuffer);
  intTo2Chars(timerArray[1]);
   drawStr(50,28,numberBuffer);
  intTo2Chars(timerArray[2]);
   drawStr(80,28,numberBuffer);

  drawSelectors(timerIndex);

  if(isRunning){
     drawStr(30,54,"Stop");
  } else {
     drawStr(30,54,"Start");
  }

   drawStr(70,54,"Reset");


}

void drawSelectors(short index){
  if(locked){
    switch (index) {
      case 0: drawTriangle(22,10,8,2); drawTriangle(22,38,8,1); break;
      case 1: drawTriangle(52,10,8,2); drawTriangle(52,38,8,1); break;
      case 2: drawTriangle(82,10,8,2); drawTriangle(82,38,8,1); break;
    }
  } else {
    switch (index) {
      case 0: drawTriangle(22,8,8,1); break;
      case 1: drawTriangle(52,8,8,1);break;
      case 2: drawTriangle(82,8,8,1); break;
      case 3: display.drawRect(28,45 + 5,34,13,WHITE);break; //draw rectangle around this option
      case 4: display.drawRect(68,45 + 5,34,13,WHITE); break; //draw rectangle around this option
    }
  }
}

void alertPage(){
  // draw the message
  short xOffset = (alertTextLen * 6)/2;
  drawStr(64 - xOffset, 12,alertText);
  drawStr(4, 50,"Press OK to dismiss.");
  //draw timestamp
  short centreY = 32;
  intTo2Chars(alertRecieved[0]);
  drawStr(64 - 15, centreY,numberBuffer);
  drawStr(64 - 3 ,centreY,":");
  intTo2Chars(alertRecieved[1]);
  drawStr(64 + 3,centreY,numberBuffer);
  
}

void notificationMenuPage(){
  if(shouldRemove){
    //remove the notification once read
    removeNotification(menuSelector);
    shouldRemove = false;
    //menuSelector = 0; //dont put it back to zero as when new message com in the order will get screwed
  }
  if(notificationIndex == 0){
     drawStr(30,32,"No Messages.");
  }
  showMenu(notificationIndex, notificationMenuPageItem,true);
}

void settingsPage(){
  bool showCursor = true;
  if(locked){
    showCursor = false;
  }
  showMenu(numberOfSettings,settingsMenuItem,showCursor);
}

void notificationMenuPageItem(short position){
  //draw title
  drawStr(x+3,y + Y_OFFSET + 4,notifications[position].title);

  //draw timestamp
  intTo2Chars(notifications[position].dateReceived[0]);
  drawStr(x+95,y + Y_OFFSET + 4 ,numberBuffer);
  drawStr(x+103,y + Y_OFFSET + 4 ,":");
  intTo2Chars(notifications[position].dateReceived[1]);
  drawStr(x+106,y + Y_OFFSET + 4,numberBuffer);
}

void settingsMenuItem(short position){
  drawStr(x+3,y+Y_OFFSET + 4,settingKey[position].c_str());
  drawStr(x+3 + 104,y+Y_OFFSET + 4,itoa(settingValue[position],numberBuffer,10));
}


void notificationFullPage(short chosenNotification){
  
  // Calculate lines based on text length so our input handler know how to change Y_OFFSET
  lineCount = (notifications[chosenNotification].textLength / 24) + 1; // 24 charaters per line default font of 5x7
  
  // this is all we need to print the notification text with the new style, the sh1106 lib takes care of the wrapping for us
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, Y_OFFSET);
  display.print(notifications[chosenNotification].textPointer);

  //DEBUG
  intTo2Chars(lineCount);
  drawStr(64,50 ,numberBuffer);
}

void homePage(short hour, short minute,short second){
  display.drawCircle(32,32,30,WHITE);
  display.drawCircle(32,32,29,WHITE);
   drawStr(59-32,8,"12");
   drawStr(59-32 + 2,52,"6");
   drawStr(7,32,"9");
   drawStr(53,32,"3");

  if(clockArray[0] > 12){
     drawStr(0,56,"PM");
  } else {
     drawStr(0,56,"AM");
  }


  float hours = (((hour * 30) + ((minute/2))) * (PI/180));
  short x2 = 32 + (sin(hours) * (clockRadius/2));
  short y2 = 32 - (cos(hours) * (clockRadius/2));
  display.drawLine(32,32,x2,y2,WHITE); //hour hand

  float minutes = ((minute * 6) * (PI/180));
  short xx2 = 32 + (sin(minutes) * (clockRadius/1.4));
  short yy2 = 32 - (cos(minutes) * (clockRadius/1.4));
  display.drawLine(32,32,xx2,yy2,WHITE);//minute hand

  float seconds = ((second * 6) * (PI/180));
  short xxx2 = 32 + (sin(seconds) * (clockRadius/1.3));
  short yyy2 = 32 - (cos(seconds) * (clockRadius/1.3));
  display.drawLine(32,32,xxx2,yyy2,WHITE);//second hand

  if(settingValue[1] == 1){
    numberOfWidgets = numberOfNormalWidgets + numberOfDebugWidgets;
  } else {
    numberOfWidgets = numberOfNormalWidgets;
  }

  switch(widgetSelector){
    case 0: timeDateWidget(); break;
    case 1: digitalClockWidget(); break;
    case 2: weatherWidget(); break;
    case 3:  drawStr(70,36 + 3,"Messages"); break;
    case 4:  drawStr(80,36 + 3,"Timer"); break;
    case 5:  drawStr(70,36 + 3,"Settings"); break;
    case 7:  drawStr(73,36 + 3,"Reset"); display.drawBitmap(110,36,BLUETOOTH_CONNECTED,8,10,WHITE); break;
    case 6:  drawStr(75,36 + 3,"Alarms"); break;
  }

  //status bar - 15 px high for future icon ref
  display.drawRect(70,0,58,15,WHITE);
  //separator line between notifications and bt icon on the status bar
  display.drawRect(111,0,111,15,WHITE);
  //batt voltage and connection separator
  display.drawRect(92,0,92,15,WHITE);

  //notification indicator
  short xChar = notificationIndex < 10 ? 118 : 113; // If we have two charaters to draw change the draw position to fit it
  drawStr(xChar,4 ,itoa(notificationIndex,numberBuffer,10));

  //battery voltage
  if(!isCharging && !isCharged){
    if(batteryPercentage != 100){
      drawStr(72,4,itoa(batteryPercentage,numberBuffer,10));
      drawStr(84,4,"%");
    }
  } else if(isCharged){
    //fully charged symbol here
    display.drawBitmap(74,1,CHARGED,14,12,WHITE);
  } else {
    //draw symbol to show that we are charging here
    display.drawBitmap(75,2,CHARGING,14,12,WHITE);
  }

  //connection icon
  if(isConnected){
    display.drawBitmap(97,2,BLUETOOTH_CONNECTED,8,10,WHITE);
  } else {
    drawStr(97,4,"NC");
  }
}

/*
* Home page widgets
*/

void timeDateWidget(){
  //display date from RTC
  drawStr(72,20+4,days[dateArray[3] - 1].c_str());
  intTo2Chars(dateArray[0]);
  drawStr(72,34+4,numberBuffer);
  drawStr(90,34+4,months[dateArray[1] - 1].c_str());
  intTo2Chars(dateArray[2] + 1970);
  drawStr(72,48+4,numberBuffer);
}

void digitalClockWidget(){
  display.drawRect(68,28,60,16,WHITE);
  intTo2Chars(clockArray[0]);
  drawStr(70,29 + 4,numberBuffer);
  drawStr(84,29 + 4,":");
  intTo2Chars(clockArray[1]);
  drawStr(92,29 + 4,numberBuffer);
  drawStr(106,29 + 4,":");
  intTo2Chars(clockArray[2]);
  drawStr(114,29 + 4,numberBuffer);
}

void weatherWidget(){
  if(weatherData){
    drawStr(72,19,weatherDay);
    
    display.setFont(&twop3pt7b); //TODO: 3pt font is required but need a clear font, preferable a pure ascii one
    intTo2Chars(timeWeGotWeather[0]);
    drawStr(95,22,numberBuffer);
    drawStr(105,22,":");
    intTo2Chars(timeWeGotWeather[1]);
    drawStr(110,22,numberBuffer);
    display.setFont(); // reverts back to default
    
    drawStr(72,28,weatherTemperature);
    drawStr(85,28,"C");

    short index = 0;
    charIndex = 0;
    if(contains(weatherForecast,' ',sizeof(weatherForecast))){
      for(short i=0; i < sizeof(weatherForecast); i++){
        if(weatherForecast[i]==' ' || weatherForecast[i] == 0){ // == 0  find the end of the data
          drawStr(72,(38) + (index * 10),lineBuffer);//draw it
          index++;
          charIndex = 0; //reset index for next line
          memset(lineBuffer,0,sizeof(lineBuffer));//reset buffer
          if(index == 3){ //after 2 lines break
            break;
          }
        } else {
          lineBuffer[charIndex] = weatherForecast[i]; //fill buffer
          charIndex++;
        }
      }
    } else {
      drawStr(72,38,weatherForecast);
    }


  } else {
    //print that weather is not available
    // u8g_SetFont(&u8g, // u8g_font_7x14);
    drawStr(70,24,"Weather");
    drawStr(70,40,"Data N/A");
    // u8g_SetFont(&u8g, // u8g_font_6x12);
  }
}

/*
* Input handling methods
*/

void handleInput(){
  short  vector = getConfirmedInputVector();
   if(vector!=lastVector){
    // TODO: once we've clicked a new button if we keep holding it we should keep handling that event, i.e keep increasing a number if we hold the button - implement this
      if (vector == UP_DOWN){
        Serial.println(F("Dual click detected!"));
        handleDualClick();
      } else if (vector == UP_ONLY){
        handleUpInput();
      } else if(vector == DOWN_ONLY){
        handleDownInput();
      } else if(vector == OK_ONLY){
        handleOkInput();
      } else if(vector == ALL_THREE){
        Serial.println(F("Return to menu Combo"));
        pageIndex = HOME_PAGE; // take us back to the home page
        widgetSelector = settingValue[0];// and our fav widget
      }
      prevButtonPressed = millis();
      idle = false;
    }
      if(vector == NONE_OF_THEM){
        if(((millis() - prevButtonPressed) > INPUT_TIME_OUT) && (prevButtonPressed != 0)){
          Serial.println(F("Time out input"));
          prevButtonPressed = 0;
          pageIndex = HOME_PAGE; // take us back to the home page
          widgetSelector = settingValue[0]; //and our fav widget
          Y_OFFSET = 0;// reset the Y_OFFSET so if we come off a page with an offset it doesnt get bugged
          currentLine = 0;
          idle = true;
        }
      }
    lastVector = vector;
}

void handleDualClick(){
    // currently just return to home
    pageIndex = HOME_PAGE;
}

void handleUpInput(){
  Serial.println(F("Up Click Detected"));
  if(pageIndex == HOME_PAGE){
    widgetSelector++;
    if(widgetSelector > numberOfWidgets){
      widgetSelector = 0;
    }
  } else if(pageIndex == NOTIFICATION_MENU){
    menuUp(notificationIndex);
  } else if(pageIndex == NOTIFICATION_BIG){
    if( (lineCount - currentLine) >= 6){
    //this scrolls down
    Y_OFFSET -= FONT_HEIGHT;
    currentLine++;
  }
  } else if(pageIndex == TIMER){
    if(locked){
      timerArray[timerIndex]++;
    } else {
      timerIndex++;
      if(timerIndex > 4){
        timerIndex = 0;
      }
    }
  } else if(pageIndex == SETTINGS){
    //check if were locked first (changing value)
    if(locked){
      settingValue[menuSelector]++;
      if(settingValue[menuSelector] > settingValueMax[menuSelector]){
        settingValue[menuSelector] = settingValueMax[menuSelector];;
      }
    } else {
      menuUp(numberOfSettings);
    }
  } else if(pageIndex == ALARM_PAGE){
    if(locked){
      alarms[alarmToggle].alarmTime[alarmIndex]++;
      if(alarms[alarmToggle].alarmTime[alarmIndex] > alarmMaxValues[alarmIndex]){
        alarms[alarmToggle].alarmTime[alarmIndex] = alarmMaxValues[alarmIndex];
      }
    } else {
      alarmIndex++;
      if(alarmIndex > 4){
        alarmIndex = 0; // 3 is max
      }
    }
  } else {
    Serial.println(F("Unknown Page."));
  }
}

void menuUp(short size){
  menuSelector++;
  //check here if we need scroll up to get the next items on the screen//check here if we nmeed to scroll down to get the next items
  if((menuSelector >= 4) && (((size + 1) - menuSelector) > 0)){//0,1,2,3 = 4 items
    //shift the y down
    Y_OFFSET -= MENU_ITEM_HEIGHT;
  }
  if(menuSelector >= size){
     //menuSelector = 0;
     menuSelector = size;
  }
}

void menuDown(){
  menuSelector--;
  if(menuSelector < 0){
     menuSelector = 0;
     //menuSelector = notificationIndex + 1;
  }
  //plus y
  if((menuSelector >= 3)){
    Y_OFFSET += MENU_ITEM_HEIGHT;
  }
}

void handleDownInput(){
  Serial.println(F("Down Click Detected"));
  if(pageIndex == HOME_PAGE){
    widgetSelector--;
    if(widgetSelector < 0){
      widgetSelector = numberOfWidgets;
    }
  } else if(pageIndex == NOTIFICATION_MENU){
    menuDown();
  } else if(pageIndex == NOTIFICATION_BIG){
    if(currentLine > 0){
    //scrolls back up
    Y_OFFSET += FONT_HEIGHT;
    currentLine--;
  }
  } else if(pageIndex == TIMER){
    if(locked){
      timerArray[timerIndex]--;
      if(timerArray[timerIndex] < 0){
        timerArray[timerIndex] = 0;
      }
    } else {
      timerIndex--;
      if(timerIndex < 0){
        timerIndex = 4;
      }
    }
  } else if(pageIndex == SETTINGS){
    if(locked){
      settingValue[menuSelector]--;
      if(settingValue[menuSelector] < settingValueMin[menuSelector]){
        settingValue[menuSelector] = settingValueMin[menuSelector];
      }
    } else {
      menuDown();
    }
  } else if(pageIndex == ALARM_PAGE){
    if(locked){
      alarms[alarmToggle].alarmTime[alarmIndex]--;
      if(alarms[alarmToggle].alarmTime[alarmIndex] < 0){
        alarms[alarmToggle].alarmTime[alarmIndex] = 0;
      }
    } else {
      alarmIndex--;
      if(alarmIndex < 0){
        alarmIndex = 4; // 3 is max
      }
    }
  } else {
    Serial.println(F("Unknown Page."));
  }
}

void handleOkInput(){
  Serial.println(F("OK Click Detected"));
  if(pageIndex == HOME_PAGE){
    if(widgetSelector == 3){
      pageIndex = NOTIFICATION_MENU;
    } else if(widgetSelector == 4){
      pageIndex = TIMER;
    } else if(widgetSelector == 5){
      pageIndex = SETTINGS;
    } else if(widgetSelector == 6){
      pageIndex = ALARM_PAGE;
    } else if(widgetSelector == 7){
      resetBTModule();
    }
    Y_OFFSET = 0;
  } else if(pageIndex == NOTIFICATION_MENU){
    if(menuSelector != notificationIndex){//last one is the back item
      Y_OFFSET = 0;
      pageIndex = NOTIFICATION_BIG;
    } else {
      menuSelector = 0; //reset the selector
      pageIndex = HOME_PAGE;// go back to list of notifications
    }
  } else if(pageIndex == NOTIFICATION_BIG){
    shouldRemove = true;
    if(menuSelector > 3){
      Y_OFFSET = -1* (menuSelector - 3) * MENU_ITEM_HEIGHT; //return to place
    } else {
      Y_OFFSET = 0;
    }
    lineCount = 0;//reset number of lines
    currentLine = 0;// reset currentLine back to zero
    pageIndex = NOTIFICATION_MENU;
  } else if(pageIndex == TIMER){
    if(timerIndex==3){
      isRunning = !isRunning; //start/stop timer
    } else if(timerIndex == 4){
      Serial.println(F("Resetting timer."));
      isRunning = false;
      timerArray[0] = 0;
      timerArray[1] = 0;
      timerArray[2] = 0;
    }else {
      locked = !locked; //lock or unlock into a digit so we can manipulate it
    }
  } else if(pageIndex == SETTINGS){
    if(menuSelector==(numberOfSettings)){ //thisis the back button
      menuSelector = 0;
      pageIndex = HOME_PAGE;
      //dont reset the widgetIndex to give the illusion we just came from there
    } else {
      locked = !locked;
      if(!locked){
        saveToEEPROM(menuSelector,settingValue[menuSelector]);
      }
    }
  } else if(pageIndex == ALERT){
    memset(alertText,0,sizeof(alertText)); //reset the alertText
    alertTextLen = 0; //reset the index
    pageIndex = lastPage; //go back
  } else if(pageIndex == ALARM_PAGE){
    //handle alarmInput
    if(alarmIndex == 3){
      alarms[alarmToggle].active = !alarms[alarmToggle].active;
      if(alarms[alarmToggle].active){
        if((dateArray[0] + alarms[alarmToggle].alarmTime[2]) > dayInMonth[dateArray[1] - 1]){
          setAlarm(alarmToggle, alarms[alarmToggle].alarmTime[0], alarms[alarmToggle].alarmTime[1], ((dateArray[0] + alarms[alarmToggle].alarmTime[2]) - dayInMonth[dateArray[1] - 1]),dateArray[1],dateArray[2]); // (dateArray[0] + alarmTime[2]) == current date plus the days in advance we want to set the
          //Serial.print("Alarm set for the : ");
          //Serial.println(((dateArray[0] + alarms[alarmToggle].alarmTime[2]) - dayInMonth[dateArray[1] - 1]));
        } else {
          setAlarm(alarmToggle, alarms[alarmToggle].alarmTime[0], alarms[alarmToggle].alarmTime[1], (dateArray[0] + alarms[alarmToggle].alarmTime[2]),dateArray[1],dateArray[2]);
          //Serial.print("Alarm set for the : ");
          //Serial.println((dateArray[0] + alarms[alarmToggle].alarmTime[2]));
        }

      }

   } else if(alarmIndex == 4){
     if(alarmToggle == 0){
       alarmToggle = 1;
     } else {
       alarmToggle = 0;
     }
   } else {
     locked = !locked;
   }
  } else {
    Serial.println(F("Unknown Page."));
  }
}

short getConfirmedInputVector()
{
  static short lastConfirmedVector = 0;
  static short lastVector = -1;
  static long unsigned int heldVector = 0L;

  // Combine the inputs.
  short rawVector =
    isButtonPressed(OK_BUTTON) << 2 |
    isButtonPressed(DOWN_BUTTON) << 1 |
    isButtonPressed(UP_BUTTON) << 0;

  /*Serial.print("Okay Button: ");
  Serial.println(touchRead(OK_BUTTON));
  Serial.print("Down Button: ");
  Serial.println(touchRead(DOWN_BUTTON));
  Serial.print("Up Button: ");
  Serial.println(touchRead(UP_BUTTON));*/

  // On a change in vector, don't return the new one!
  if (rawVector != lastVector)
  {
    heldVector = millis();
    lastVector = rawVector;
    return lastConfirmedVector;
  }

  // We only update the confirmed vector after it has
  // been held steady for long enough to rule out any
  // accidental/sloppy half-presses or electric bounces.
  //
  long unsigned heldTime = (millis() - heldVector);
  if (heldTime >= CONFIRMATION_TIME)
  {
    lastConfirmedVector = rawVector;
  }

  return lastConfirmedVector;
}

/*
* Data Proccessing methods.
*/

void getWeatherData(char weatherItem[],short len){
  Serial.println("Recieved from Serial: ");
  char *printPtr = weatherItem;
  for(int i=0; i < len; i++){
    Serial.print(*(printPtr++));
  }
  Serial.println();
  
  short index = 0;
  short charIndex = 0;
  short textCount = 0;
  
  char* weaPtr = weatherItem; 
  while(*weaPtr != '\0'){
    if(*(weaPtr) == '<' && *(weaPtr + 1) == 'i' && *(weaPtr + 2) == '>'){
      weaPtr+=2; // on two becuase this char is one
      index++;
      charIndex = 0;
    } else {
      if(index==0){
        weatherDay[charIndex] = *weaPtr;
        charIndex++;
      }else if(index==1){
        weatherTemperature[charIndex] = *weaPtr;
        charIndex++;
      } else if(index==2){
        weatherForecast[charIndex] = *weaPtr;
        charIndex++;
      }
    }
    weaPtr++;
  }
  
  Serial.print(F("Day: "));
  Serial.println(weatherDay);
  Serial.print(F("Temperature: "));
  Serial.println(weatherTemperature);
  Serial.print(F("Forecast: "));
  Serial.println(weatherForecast);
  for(short l=0; l < 2; l++){
    timeWeGotWeather[l] = clockArray[l];
  }
  weatherData = true;
}

// <n>com.mabezdev<t>Hello<e>Test you cunty<i>askhjdgahkjshdgasd<e>

void getNotification(char notificationItem[],short len){
  Serial.println("Recieved from Serial: ");
  char *printPtr = notificationItem;
  for(int i=0; i < len; i++){
    Serial.print(*(printPtr++));
  }
  Serial.println();

  short index = 0;
  short charIndex = 0;
  short textCount = 0;
  
  char* notPtr = notificationItem; 
  while(*notPtr != '\0'){
    //Serial.println(*notPtr);
    if(*(notPtr) == '<' && *(notPtr + 1) == 'i' && *(notPtr + 2) == '>'){
      notPtr+=2; // on two becuase this char is one
      index++;
      charIndex = 0;
    } else {
      if(index==0){
        notifications[notificationIndex].packageName[charIndex] = *notPtr;
        charIndex++;
      }else if(index==1){
        notifications[notificationIndex].title[charIndex] = *notPtr;
        charIndex++;
      } else if(index==2){
        notifications[notificationIndex].textLength =  (len - textCount); 
        notifications[notificationIndex].textType = determineType(notifications[notificationIndex].textLength);
        addTextToNotification(&notifications[notificationIndex],notPtr,notifications[notificationIndex].textLength);
        break;
      }
    }
    notPtr++;
    textCount++; // used to calculate the final text size by subtracting from the length of the packet
  }
  
  //finally get the timestamp of whenwe recieved the notification
  notifications[notificationIndex].dateReceived[0] = clockArray[0];
  notifications[notificationIndex].dateReceived[1] = clockArray[1];

  Serial.print(F("Notification title: "));
  Serial.println(notifications[notificationIndex].title);
  Serial.print(F("Notification text: "));
  Serial.println(notifications[notificationIndex].textPointer);
  Serial.print("Text length: ");
  Serial.println(notifications[notificationIndex].textLength);

  // dont forget to increment the index
  notificationIndex++;
}

uint8_t determineType(int len){
  if(len < MSG_SIZE[0]){
    return 0;
  } else if(len < MSG_SIZE[1]){
    return 1;
  } else {
    return 2;
  }
}

void addTextToNotification(Notification *notification, char *textToSet, short len){
  // gives us the location to start writing our new text
  char *textPointer = (char*)(types[notification->textType]); //pointer to the first element in the respective array, i.e Small, Normal Large
  /*
   * Move the pointer along an element at a time using the MSG_SIZE array to get the size of an element based on type, 
   * then times that by the index we want to add the text to
   */
  textPointer += (MSG_SIZE[notification->textType] * textIndexes[notification->textType]);
  // write the text to the new found location
  setText(textToSet, textPointer, len);
  // store a reference to the textpointer for later removal
  notification->textPointer = textPointer;
  notification->textLength = len;

//  Serial.print("Index for type ");
//  Serial.print(notification->textType);
//  Serial.print(" is ");
//  Serial.println(textIndexes[notification->textType]);
  
  textIndexes[notification->textType]++; //increment the respective index
  
}



void getTimeFromDevice(char message[], short len){
  //sample data
  //24 04 2016 17 44 46
  Serial.print(F("Date data: "));
  for(int p = 0; p < len; p++){
    Serial.print(message[p]);
  }
  Serial.println();
  
  char buf[4];//max 2 chars
  short index = 0;
  short bufferIndex = 0;
  bool switchArray = false;

  for(int i = 0; i < len; i++){
    if(message[i] == ' ' || i == (len - 1)){
      switchArray = index >= 3;
      if(!switchArray){
        dateArray[index] = atoi(buf);
      } else {
        clockArray[index - 3] = atoi(buf);
      }
      index++;
      memset(buf,0,sizeof(buf));
      bufferIndex = 0;
    } else {
      buf[bufferIndex] = message[i];
      bufferIndex++;
    }
  }
     
 //Read from the RTC
 breakTime(rt.getTime(),tm);
 //Compare to time from Device(only minutes and hours doesn't have to be perfect)
 if(!((tm.Hour == clockArray[0] && tm.Minute == clockArray[1] && dateArray[0] == tm.Day && dateArray[1] == tm.Month && dateArray[2] == tm.Year))){
    setClockTime(clockArray[0],clockArray[1],clockArray[2],dateArray[0],dateArray[1],dateArray[2]);
    Serial.println(F("Setting the clock!"));
 } else {
    //if it's correct we do not have to set the RTC and we just keep using the RTC's time
    Serial.println(F("Clock is correct already!"));
    gotUpdatedTime = true;
 }

}

/*
* System methods
*/

void setAlarm(short alarmType, short hours, short minutes, short date,short month,short year){
  short start = alarmType == 0 ? ALARM_ADDRESS : ALARM_ADDRESS + ALARM_DATA_LENGTH;
//  if(alarmType == 0){
//    RTC.setAlarm(ALM1_MATCH_DATE, minutes, hours, date); // minutes // hours // date (28th of the month)
//    Serial.println("ALARM1 Set!");
//    start = ALARM_ADDRESS;
//  } else {
//    RTC.setAlarm(ALM2_MATCH_DATE, minutes, hours, date); // minutes // hours // date (28th of the month)
//    Serial.println("ALARM2 Set!");
//    start = ALARM_ADDRESS + 4;
//  }

  /*
   * As our RTC not longer has alarm functionality we need to write it in software, this should be pretty simple as we already wrote the values to eeprom
   * instead of checking if the interrupt has gone off we should just check current time/date with the alarms ones, make sure not to miss it though!
   */
  Serial.println("Setting alarm for: ");
  Serial.print("Time: ");
  Serial.print(hours);
  Serial.print(":");
  Serial.print(minutes);
  Serial.println(":0");
  Serial.print("Date: ");
  Serial.print(date);
  Serial.print("/");
  Serial.print(month);
  Serial.print("/");
  Serial.print(year);
  Serial.println();
   
  uint32_t time;
  alarmTm.Hour = hours;
  alarmTm.Minute = minutes;
  alarmTm.Day = date;
  alarmTm.Month = month;
  alarmTm.Second = 0;
  alarmTm.Year = year; // - 1970
  time = makeTime(alarmTm);
  alarms[alarmType].time = time;
  
  Serial.print("Alarm makeTime: ");
  Serial.println(alarms[alarmType].time);
  Serial.print("Starting write at address: ");
  Serial.println(start);
  
  saveToEEPROM(start,true); //set alarm active
  start++;
  EEPROMWritelong(start,time);// need to write long
}

void createAlert(char const* text,short len, short vibrationTime){
  if(len < 20){
    lastPage = pageIndex;
    alertTextLen = len;
    for(short i =0; i < len; i++){
      alertText[i] = text[i];
    }
    // set the timestamp
    alertRecieved[0] = clockArray[0];
    alertRecieved[1] = clockArray[1];
    vibrate(vibrationTime);
    pageIndex = ALERT;
  } else {
    Serial.println(F("Not Creating Alert, text to big!"));
  }
}

void vibrate(short vibrationTime){
  alertVibrationCount = (vibrationTime) * 2;// double it as we toggle vibrate twice a second
}

void saveToEEPROM(short address,short value){
  //if(address < EEPROM.length()){
    EEPROM.write(address,value);
  //}
}

short readFromEEPROM(short address){
    return EEPROM.read(address);
}

void drawTriangle(short x, short y, short size, short direction){
  // some triangle are miss-shapen need to fix
  switch (direction) {
    case 1: display.drawTriangle(x,y,x+size,y, x+(size/2), y+(size/2),WHITE); /* u8g_DrawTriangle(&u8g,x,y,x+size,y, x+(size/2), y+(size/2)); */ break; //down
    case 2: display.drawTriangle(x,y,x+size,y, x+(size/2), y-(size/2),WHITE);/* u8g_DrawTriangle(&u8g,x,y,x+size,y, x+(size/2), y-(size/2)); */ break; //up
    case 3: display.drawTriangle(x+size,y,x,y-(size/2),x+size,y-size,WHITE);/* u8g_DrawTriangle(&u8g,x+size,y,x,y-(size/2),x+size,y-size); */ break; // left
    case 4: display.drawTriangle(x + size,y-(size/2),x,y,x,y-size,WHITE);/* u8g_DrawTriangle(&u8g,x + size,y-(size/2),x,y,x,y-size); */ break; // right
  }
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

void setClockTime(short hours,short minutes,short seconds, short days, short months, short years){
  tm.Hour = hours;
  tm.Minute = minutes;
  tm.Second = seconds;
  tm.Day = days;
  tm.Month = months;
  tm.Year = years - 1970; //offset
  Serial.print(F("Time: "));
    Serial.print(clockArray[0]);
    Serial.print(F(":"));
    Serial.print(clockArray[1]);
    Serial.print(F(":"));
    Serial.print(clockArray[2]);
    Serial.print(F("    Date: "));
    Serial.print(dateArray[0]);
    Serial.print(F("/"));
    Serial.print(dateArray[1]);
    Serial.print(F("/"));
    Serial.println(dateArray[2]);
  rt.setTime(makeTime(tm));
}

void resetBTModule(){
  HWSERIAL.print("AT"); //disconnect
  delay(100); //need else the module won't see the commands as two separate ones
  HWSERIAL.print("AT+RESET"); //then reset
  createAlert("BT Module Reset.",16,0);
}


void removeNotification(short pos){
  if ( pos >= notificationIndex + 1 ){
    Serial.println(F("Can't delete notification."));
  } else {
    //need to zero out the array or stray chars will overlap with notifications
    memset(notifications[pos].title,0,sizeof(notifications[pos].title));
    memset(notifications[pos].packageName,0,sizeof(notifications[pos].packageName));
    removeTextFromNotification(&notifications[pos]);
    for ( short c = pos; c < (notificationIndex - 1) ; c++ ){
       notifications[c] = notifications[c+1];
    }
    Serial.print(F("Removed notification at index: "));
    Serial.println(pos);
    //lower the index
    notificationIndex--;
  }
}

void removeTextFromNotification(Notification *notification){
  Serial.print("Trying to remove text from a notification of type ");
  bool found = false;
  Serial.println(notification->textType);
  char *arrIndexPtr = (char*)(types[notification->textType]); // find the begining of the respective array, i.e SMALL,NORMAL,LARGE
  for(int i=0; i < textIndexes[notification->textType];i++){ // look through all valid elements
    if(notification->textPointer == arrIndexPtr){ // more 'safe way' of comparing pointers to avoid compiler optimisations
      Serial.print("Found the text to be wiped at index ");
      Serial.print(i);
      Serial.print(" in array of type ");
      Serial.println(notification->textType);
      found = true;
      for ( short c = i ; c < (textIndexes[notification->textType] - 1) ; c++ ){
        // move each block into the index before it, basically Array[c] = Array[c+1], but done soley using memory modifying methods
         memcpy((char*)(types[notification->textType]) + (c * MSG_SIZE[notification->textType]),(char*)(types[notification->textType]) + ((c+1) *  MSG_SIZE[notification->textType]), MSG_SIZE[notification->textType]);
      }
      textIndexes[notification->textType]--; // remeber to decrease the index once we have removed it
    }
    arrIndexPtr += MSG_SIZE[notification->textType]; // if we haven't found our pointer, move the next elemnt by moving our pointer along
  }
  if(!found){
    Serial.print("Failed to find the pointer for text : ");
    Serial.println(notification->textPointer);
  }
}

//void removeNotification(short pos){
//  if ( pos >= notificationIndex + 1 ){
//    Serial.println(F("Can't delete notification."));
//  } else {
//    //need to zero out the array or stray chars will overlap with notifications
//    memset(notifications[pos].textPointer,0,sizeof(notifications[pos].textPointer));
//    memset(notifications[pos].title,0,sizeof(notifications[pos].title));
//    memset(notifications[pos].packageName,0,sizeof(notifications[pos].packageName));
//    for ( short c = pos ; c < (notificationIndex - 1) ; c++ ){
//       notifications[c] = notifications[c+1];
//    }
//    Serial.print(F("Removed notification at position: "));
//    Serial.println(pos);
//    //lower the index
//    notificationIndex--;
//  }
//}

float getBatteryVoltage(){ // depreciated soon, will be using MAX17043
  /*
   * WARNING: Add voltage divider to bring batt voltage below 3.3v at all times! Do this before pluggin in the Batt or will destroy the Pin in a best case scenario
   * and will destroy the teensy in a worst case.
   */
   float reads = 0;
   for(short i=0; i<100; i++){
    reads+= analogRead(BATT_READ);
   }
   // R1 = 2000, R2 = 3300
   // Vin = (Vout * (R1 + R2)) / R2
  return ((reads/100) * (3.3 / 1024) * (3300 + 2000))/(3300);
}

/*
* Utility methods
*/

void setText(char* c, char* textPtr, short len){
  int i = 0;
  while(i < len){
    // set the actual array value to the next value in the setText String
    *(textPtr++) = *(c++);
    i++;
  }
}

//This function will write a 4 byte (32bit) long to the eeprom at
//the specified address to address + 3.
void EEPROMWritelong(int address, long value)
      {
      //Decomposition from a long to 4 bytes by using bitshift.
      //One = Most significant -> Four = Least significant byte
      byte four = (value & 0xFF);
      byte three = ((value >> 8) & 0xFF);
      byte two = ((value >> 16) & 0xFF);
      byte one = ((value >> 24) & 0xFF);

      //Write the 4 bytes into the eeprom memory.
      EEPROM.write(address, four);
      EEPROM.write(address + 1, three);
      EEPROM.write(address + 2, two);
      EEPROM.write(address + 3, one);
      }

//This function will return a 4 byte (32bit) long from the eeprom
//at the specified address to address + 3.
long EEPROMReadlong(long address)
      {
      //Read the 4 bytes from the eeprom memory.
      long four = EEPROM.read(address);
      long three = EEPROM.read(address + 1);
      long two = EEPROM.read(address + 2);
      long one = EEPROM.read(address + 3);

      //Return the recomposed long by using bitshift.
      return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
      }


void intTo2Chars(short number){
  memset(numberBuffer,0,sizeof(numberBuffer));
  if(number < 10){
    //not working atm
    numberBuffer[0] = '0';
    numberBuffer[1] = char(number + 48);

  } else {
    itoa(number,numberBuffer,10);
  }
}

bool startsWith(char data[], char charSeq[], short len){
    for(short i=0; i < len; i++){
      if(!(data[i]==charSeq[i])){
        return false;
      }
    }
    return true;
}

bool contains(char data[], char character, short lenOfData){
  for(short i = 0; i < lenOfData; i++){
    if(data[i] == character){
      return true;
    }
  }
  return false;
}

short FreeRam() {
  char top;
  #ifdef __arm__
    return &top - reinterpret_cast<char*>(sbrk(0));
  #else  // __arm__
    return __brkval ? &top - __brkval : &top - &__bss_end;
  #endif  // __arm__
}

