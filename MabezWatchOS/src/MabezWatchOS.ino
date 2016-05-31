#include <Arduino.h>

#include <u8g_i2c.h>
#include <i2c_t3.h>
#include <DS1307RTC.h> //this works with out clock (DS3231) but we will have to implemnt our own alarm functions
#include <EEPROM.h>

#define HWSERIAL Serial1

//needed for calculating Free RAM on ARM based MC's
#ifdef __arm__
  extern "C" char* sbrk(int incr);
#else  // __ARM__
  extern char *__brkval;
  extern char __bss_end;
#endif  // __arm__

//u8g lib object without the c++ wrapper due to lack of support of the OLED
u8g_t u8g;

/*
 * This has now been fully updated to the teensy LC, with an improved transmission algorithm.

   Currently with no power saving techniques the whole thing runs on just 28millisamps, therfore
        240 mah battery / 28ma current draw = 8.5 hours of on time
   Battery Update: Left to discharge whilst connected to the App, we got 8 hours and 22 minutes, very close to the 8 hours 30mins we calculated
 *
 * Update:
 *  -(04/04/16)UI Improved vastly.
 *  -(19/04/16)Fixed notification sizes mean we can't SRAM overflow, leaves about 1k of SRAM for dynamic allocations i.e message or finalData (See todo for removal of all dynamic allocations)
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
    -(31/05/16) Added battery charging algorithm, works well enough to know when a single lithium cell is charged
 *
 *  Buglist:
    -midnight on the digital clock widget produces three zeros must investigate
        - might be due to the fact a itoa func make a number wwith 3 characters
 *
 *
 *  Todo:
 *    -Use touch capacitive sensors instead of switches to simplify PCB and I think it will be nicer to use. -TESTED
 *    -Maybe use teensy 3.2, more powerful, with 64k RAM, and most importantly has a RTC built in
 *    -add reply phrases, if they are short answers like yeah, nah, busy right now etc.
 *    -Could use a on screen keyabord but it miught be too much hassle to use properly.
 *    -text wrapping on the full notification page
 *    -add a input time out where if the user does not interact with the watch the widget goes back to a clock one - DONE
          - change the widget to the favorite stored in the settings - DONE
 *    - fix the alert function to have two pulses without delay (Fix could be aleRTCounter set it to 2 then vibrate and -- from aleRTCounter) - DONE
 *    - use isPrintable on a char to find out if we can send it (maybe do thios on the phone side)
 *    - finish timer app (use our system click to time the seconds) - BASIC FUNCTIONALITY COMPLETE - [TESTED]
          - need to alert the suer when the timer is up - DONE
          - make it look better
 *    -settings page
          - favourite widget (will default to once no input is recieved) - DONE
 *    -use eeprom to store the settings so we dont have to set them - DONE
 *    -the lithium charger board has a two leds one to show charging one to show chargin done, need to hook into these to read when we are charging(to charge the RTC  batt(trickle charge))
       and to change the status on the watch to charging etc
 *    - add software turn off
 *    - add hardware power on/off
 *    - add more weatherinfo screen where we can see the forecast for the next days or hours etc
      - tell the user no messages found on the messages screen
      - write a generic menu function that takes a function as a parameter, the function will write 1 row of data
      - Sometime the BT module can get confused (or the OS? or is the app?) and we can't connect till we rest the Watch
          - Pin 11 (on the HM-11) held low for 100ms will reset the HM-11 Module.
          - or send AT+RESET to reset it aswell
      - request data from phone?, tell phone we have read a notification?
          - App is ready to recieve data from Module, just need to figure what we want
              - Maybe instead of randomly sending the weather data we coudl request it instead?
                  -request 5 day forecast?
              - request phone battery level
              -  we could tell the phone we have ran out of mem, so keep the rest of the notifications in the queue till there is space [DONE]
      - Add time stamp for notifications? - (Would need to modify notification structure but we dont need any data from the phone use out RTC), we have space due to the 15 char limit on the titles
      - Keep tabs on Weather data to mkae sure its still valid (make sure were not displaying data that is hours old or something)
      - add otion for alert to go away after x amount of seconds?
      - F() all strings - [DONE] (But makes no difference on ARM based MC's)
      - store tags like <i> as progmem constants for better readablity
      - Switch to DS3231 RTC - [DONE]  but we need to implement our methods to get temperature, and set alarms by translating code from another lib
          - Use the alarm functionality of this module to create an alarm page( only 2 alarms though I Believe)
          - We can also get the temperature outside with it
      - add a battery chargin widget which shows more info about the battery chargin like its current percent (display a graphic?)
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

      - Add a charging low power state where we draw little to no current,
          so the lithium charger can do its job, we can deep sleep and wake up in intervals as when it is charging we can't do much with it anyways
          - The snooze lib does not work with my version on teensyduino, will need to downgrade

 */

//input vars
bool button_ok = false;
bool lastb_ok = false;
bool button_up = false;
bool lastb_up = false;
bool button_down = false;
bool lastb_down = false;

#define CONFIRMATION_TIME 80 //length in time the button has to be pressed for it to be a valid press
#define INPUT_TIME_OUT 60000 //60 seconds
#define TOUCH_THRESHOLD 1200

//need to use 4,2,1 as no combination of any of the numbers makes the same number, where as 1,2,3 1+2 = 3 so there is no individual state.
#define UP_ONLY  4
#define OK_ONLY  2
#define DOWN_ONLY  1
#define UP_OK  (UP_ONLY|OK_ONLY)
#define UP_DOWN  (UP_ONLY|DOWN_ONLY)
#define DOWN_OK  (OK_ONLY|DOWN_ONLY)
#define ALL_THREE (UP_ONLY|OK_ONLY|DOWN_ONLY)
#define NONE_OF_THEM  0

//#define isButtonPressed(pin)  (digitalRead(pin) == LOW)
#define isButtonPressed(pin) (touchRead(pin) > TOUCH_THRESHOLD)

int lastVector = 0;
long prevButtonPressed = 0;

//serial retrieval vars
char message[100]; // serial read buffer
char *messagePtr; //this could be local to the receiving loop
char finalData[250];//data set buffer
int finalDataIndex = 0; //index is required as we dunno when we stop
int messageIndex = 0;
bool readyToProcess = false;
bool receiving = false;
bool checkingTag = false;
char messageBuffer[3];

//date contants
String PROGMEM months[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
String PROGMEM days[7] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
const short PROGMEM dayInMonth[12] = {31,28,31,30,31,30,31,31,30,31,30,31}; //does not account for leap year

//weather vars
bool weatherData = false;
char weatherDay[4];
char weatherTemperature[4];
char weatherForecast[25];
int timeWeGotWeather[2] = {0,0};


//time and date constants
tmElements_t tm;
time_t t;
const int PROGMEM clockRadius = 32;
const int PROGMEM clockUpdateInterval = 1000;

//time and date vars
bool gotUpdatedTime = false;
int clockArray[3] = {0,0,0};
int dateArray[4] = {0,0,0,0};

long prevMillis = 0;

//notification data structure
typedef struct{
  char packageName[15];
  char title[15];
  char text[150];
  int dateReceived[2];
  int textLength;
} Notification;

//notification vars
int notificationIndex = 0;
int PROGMEM notificationMax = 10;
Notification notifications[10];
bool wantNotifications = true;
bool shouldRemove = false;

//pin constants
const int PROGMEM OK_BUTTON = 17;
const int PROGMEM DOWN_BUTTON = 16;
const int PROGMEM UP_BUTTON = 15;
const int PROGMEM BATT_READ = A6;
const int PROGMEM VIBRATE_PIN = 10;
const int PROGMEM CHARGING_STATUS_PIN = 9;

//navigation constants
const int PROGMEM HOME_PAGE = 0;
const int PROGMEM NOTIFICATION_MENU = 1;
const int PROGMEM NOTIFICATION_BIG = 2;
const int PROGMEM TIMER = 4;
const int PROGMEM SETTINGS = 5;
const int PROGMEM ALARM_PAGE = 6;

const int PROGMEM ALERT = 10;

//UI constants
const int PROGMEM MENU_ITEM_HEIGHT = 16;
const int PROGMEM FONT_HEIGHT = 12; //need to add this to the y for all DrawStr functions

//navigation vars
int pageIndex = 0;
int menuSelector = 0;
int widgetSelector = 0;
const int numberOfNormalWidgets = 6; // actually 3, 0,1,2.
const int numberOfDebugWidgets = 1;
int numberOfWidgets = 0;
int numberOfPages = 6; // actually 7 (0-6 = 7)

const int x = 6;
int y = 0;
int Y_OFFSET = 0;

int lineCount = 0;
int currentLine = 0;

//batt monitoring
float batteryVoltage = 0;
int batteryPercentage = 0;
bool isCharging = false;
bool voltageReached = false; // we determine this by seeing if the batt percent is over 95% and isCharging is false
const int BATT_CHARGED_THRESHOLD = 95;
const int BATT_CURRENT_CHARGE_TIME = 720; // 12 min charge intervals
bool isCharged = false;
int chargeCurrentTimer = 0;

//timer variables
int timerArray[3] = {0,0,0}; // h/m/s
bool isRunning = false;
int timerIndex = 0;


bool locked = false;

//connection
bool isConnected = false;
int connectedTime = 0;

//settings
const int numberOfSettings = 2;
String PROGMEM settingKey[numberOfSettings] = {"Favourite Widget:","Debug Widgets:"};
const int PROGMEM settingValueMin[numberOfSettings] = {0,0};
const int PROGMEM settingValueMax[numberOfSettings] = {numberOfPages,1};
int settingValue[numberOfSettings] = {0,0}; //default

//alert popup
int lastPage = -1;
char alertText[20]; //20 chars that fit
int alertTextLen = 0;
int alertVibrationCount = 0;
bool vibrating = false;
long prevAlertMillis = 0;

//icons
const byte PROGMEM BLUETOOTH_CONNECTED[] = {
   0x31, 0x52, 0x94, 0x58, 0x38, 0x38, 0x58, 0x94, 0x52, 0x31
};

int loading = 3; // time the loading screen is show for

//drawing buffers used for character rendering
char numberBuffer[2]; //2 numbers
const int charsThatFit = 20; //only with default font. 0-20 = 21 chars
char lineBuffer[21]; // 21 chars                             ^^
int charIndex = 0;

//alarm vars
bool activeAlarms[2] = {false,false};
int alarmTime[3] = {0,0,0}; //alarm time: hours, mins, dateDay
const int PROGMEM alarmMaxValues[3] = {23,59,6};
short alarmToggle = 0; // alarm1 = 0, alarm2 = 1
short prevAlarmToggle = 1;
int alarmIndex = 0;
const int ALARM_ADDRESS = 20;//start at 30, each alarm has 4 stored values, active,hours,mins,dateDay

//used for power saving
bool idle = false;

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

void setup(void) {
  Serial.begin(9600);
  HWSERIAL.begin(9600);

  pinMode(OK_BUTTON,INPUT_PULLUP);
  pinMode(DOWN_BUTTON,INPUT_PULLUP);
  pinMode(UP_BUTTON,INPUT_PULLUP);

  pinMode(BATT_READ,INPUT);
  pinMode(CHARGING_STATUS_PIN,INPUT);
  pinMode(VIBRATE_PIN,OUTPUT);

  u8g_prepare();

  for(int i = 0; i < numberOfSettings; i++){
    settingValue[i] = readFromEEPROM(i);
  }

  //eeprom reset
  /*for(int j=0; j < EEPROM.length(); j ++){
    saveToEEPROM(j,0);
  }*/

  activeAlarms[0] = readFromEEPROM(ALARM_ADDRESS);
  activeAlarms[1] = readFromEEPROM(ALARM_ADDRESS + 5);

  messagePtr = &message[0]; // could have used messagePtr = message

  //setup batt read pin
  analogReference(DEFAULT);
  analogReadResolution(10);// 2^10 = 1024
  analogReadAveraging(32);//smoothing

  //HWSERIAL.print("AT"); to cut the connection so we have to reconnect if the watch crashes

  HWSERIAL.print("AT");

  Serial.println(F("MabezWatch OS Loaded!"));
}

void u8g_prepare(void) {
  u8g_InitComFn(&u8g, &u8g_dev_sh1106_128x64_2x_i2c, u8g_com_hw_i2c_fn);
  u8g_SetFont(&u8g, u8g_font_6x12);
}

/*
* Draw generic menu method
*/

void showMenu(int numberOfItems,void itemInMenuFunction(int),bool showSelector){
  for(int i=0; i < numberOfItems + 1;i++){
    int startY = 0;
    if(i==menuSelector && showSelector){
        u8g_DrawStr(&u8g,0,y+Y_OFFSET+FONT_HEIGHT,">");
    }
    if(i!=numberOfItems){
        itemInMenuFunction(i); //draw our custom menuItem
    } else {
      u8g_DrawStr(&u8g,x + 3,y + Y_OFFSET+FONT_HEIGHT, "Back");
    }
    y += MENU_ITEM_HEIGHT;
    u8g_DrawFrame(&u8g,x,startY,128,y +Y_OFFSET);
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
    RTC.read(tm);
    clockArray[0] = tm.Hour;
    clockArray[1] = tm.Minute;
    clockArray[2] = tm.Second;
    dateArray[0] = tm.Day;
    dateArray[1] = tm.Month;
    dateArray[2] = tmYearToCalendar(tm.Year);
    dateArray[3]  = tm.Wday;


    // update battery stuff
    batteryVoltage = getBatteryVoltage();
    batteryPercentage = ((batteryVoltage - 3)/1.2)*100; // Gives a percentage range between 4.2 and 3 volts
    isCharging = digitalRead(CHARGING_STATUS_PIN);
    //make sure we never display a negative percentage
    if(batteryPercentage < 0){
      batteryPercentage = 0;
    } else if(batteryPercentage > 100){
      batteryPercentage = 100;
    }
    //check if we are fully charged
    if(isCharging && (batteryPercentage > BATT_CHARGED_THRESHOLD) && !voltageReached){ // wait till weve dropped to BATT_CHARGED_THRESHOLD% to start displaying the percentage again
      voltageReached = true;
      chargeCurrentTimer = BATT_CURRENT_CHARGE_TIME; //12 min charge Intervals mins need to do some research too find how long it will actually take
    } else {
      if(chargeCurrentTimer > 0 && voltageReached){
        if(isCharging){//keep checking were still charging
          chargeCurrentTimer--;
        } else {
          chargeCurrentTimer = 0; // if we stop charging via disconnect rest the timer
        }
      } else {
        if(batteryPercentage > 95){ //once we drop to 90% allow charging again
          isCharged = true;
        } else {
          voltageReached = false;
          isCharged = false;
        }
      }
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

    Serial.print(F("Free RAM:"));
    Serial.println(FreeRam());
    Serial.println(F("=============================================="));

    //Alarm checks
    if(activeAlarms[0]){
      if(RTC.alarm(ALARM_1)){
        createAlert("ALARM1",6,10);
        Serial.println("ALARM 1 HAD GONE OFF");
        activeAlarms[0] = false;
        saveToEEPROM(ALARM_ADDRESS,activeAlarms[0]);
      }
    }
    if(activeAlarms[1]){
      if(RTC.alarm(ALARM_2)){
        createAlert("ALARM2",6,10);
        Serial.println("ALARM 2 HAD GONE OFF");
        activeAlarms[1] = false;
        saveToEEPROM(ALARM_ADDRESS + 5,activeAlarms[1]);
      }
    }


    HWSERIAL.print("<b>");
    HWSERIAL.print(batteryPercentage);
    HWSERIAL.print(",");
    HWSERIAL.print(batteryVoltage);
    HWSERIAL.print(",");
    HWSERIAL.print(chargeCurrentTimer);
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
  u8g_FirstPage(&u8g);
  do {
    if(loading !=0){
      u8g_DrawXBMP(&u8g,55,12,21,24,LOGO);
      u8g_DrawStr(&u8g,42,55,"Loading...");
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
  } while( u8g_NextPage(&u8g) );
    handleInput();
    while(HWSERIAL.available()){
      message[messageIndex] = char(HWSERIAL.read());//store char from serial command
      messageIndex++;
      if(messageIndex >= 99){
        //this message is too big something went wrong flush the message out the system and break the loop
        Serial.println(F("Error message overflow, flushing buffer and discarding message."));
        messageIndex = 0;
        memset(message, 0, sizeof(message)); //resetting array
        HWSERIAL.flush();
        break;
      }
      delay(1);
  }
  if(!HWSERIAL.available()){
    if(messageIndex > 0){
      Serial.print(F("Message: "));
      for(int i=0; i < messageIndex; i++){
        Serial.print(message[i]);
      }
      Serial.println();


      if(startsWith(message,"OK",2)){
          if(startsWith(message,"OK+C",4)){
            isConnected = true;
            Serial.println(F("Connected!"));
          } else if(startsWith(message,"OK+L",4)){
            isConnected = false;
            Serial.println(F("Disconnected!"));
            //reset vars like got updated time and weather here also
          } else {
            //the message was broken and we have no way of knowing if we connected or not so just send the disconnect and try again manually
            Serial.println(F("Error connecting, retry."));
            HWSERIAL.print("AT");
          }
          messageIndex = 0;
          memset(message, 0, sizeof(message));
        }
      if(!receiving && startsWith(message,"<",1)){
        receiving = true;
      }else if(receiving && (message=="<n>" || message=="<d>" || message=="<w>")) {
        Serial.println(F("Message data missing, ignoring."));
        //we never recieved the end tag of a previous message
        //reset vars
        messageIndex = 0;
        memset(message, 0, sizeof(message));
        resetTransmissionVariables();
      }else if((messageIndex < 2) && receiving){//doesn't contain a full tag and we are receiving
        //store this message and combine the next one and check if it equals <f>
        if(!checkingTag){
          for(int j = 0; j < messageIndex; j++){
            messageBuffer[j] = message[j];
          }
          checkingTag = true;
        } else {
          int currentAmount = sizeof(messageBuffer);
          if((currentAmount + (messageIndex + 1)) == 3){ //make sure it's a tag and it doesnt exceed the array index
            //we have found a tag
            for(int k = (currentAmount - 1); k < (3 - (messageIndex + 1)); k++){
                messageBuffer[k] = message[k];
            }
            //if its an <f> then we finish else we carry on
            if(startsWith(messageBuffer,"<f>",3)){
              Serial.println("We Found a finish tag that got corrupted!");
              // make ready to process true cuz were done
              readyToProcess = true;
            }
            //reset checkingTag flag
            checkingTag = false;
            //reset array
            memset(messageBuffer,0,sizeof(messageBuffer));
          }
        }
      }
      if(!startsWith(message,"<f>",3)){
        if(startsWith(message,"<i>",3)){
          // move pointer on to remove out first 3 chars
          messagePtr += 3;
          while(*messagePtr != '\0'){ //'\0' is the end of string character. when we recieve things in serial we need to add this at the end
            finalData[finalDataIndex] = *messagePtr; // *messagePtr derefereces the pointer so it points to the data
            messagePtr++; // this increased the ptr location in this case by one, if it were an int array it would be by 4 to get the next element
            finalDataIndex++;
          }
          //reset the messagePtr once done
          messagePtr = message;
        } else {
          if(!((finalDataIndex+messageIndex) >= 249)){ //check the data will fit int he char array
            for(int i=0; i < messageIndex; i++){
              finalData[finalDataIndex] = message[i];
              finalDataIndex++;
            }
          } else {
            Serial.println(F("FinalData is full, but there was more data to add. Discarding data."));
            messageIndex = 0;
            memset(message, 0, sizeof(message));
            resetTransmissionVariables();
          }
        }
      } else {
        receiving = false;
        readyToProcess = true;
      }
      //reset index
      messageIndex = 0;
      memset(message, 0, sizeof(message)); // clears array
    }
  }

  if(readyToProcess){
    Serial.print(F("Received: "));
    Serial.println(finalData);
    if(startsWith(finalData,"<n>",3)){
      if(notificationIndex < (notificationMax - 1)){ // 0-7 == 8
        getNotification(finalData,finalDataIndex);
        vibrate(2); //vibrate
        if(notificationIndex > (notificationMax - 2)){ //tell the app we are out of space and hold the notifications
          HWSERIAL.print("<e>");
          wantNotifications = false;
        }
      } else {
        Serial.println(F("Max notifications Hit."));
      }
    } else if(startsWith(finalData,"<w>",3)){
      getWeatherData(finalData,finalDataIndex);
    } else if(startsWith(finalData,"<d>",3)){
      getTimeFromDevice(finalData,finalDataIndex);
    }
    resetTransmissionVariables();
  }
  // update the system
  updateSystem();
}

/*
* Page Methods
*/

void alarmPage(){
  u8g_DrawStr(&u8g, 24,23,"H");
  u8g_DrawStr(&u8g, 54,23,"M");
  u8g_DrawStr(&u8g, 84,23,"D");

  intTo2Chars(alarmTime[0]);
  u8g_DrawStr(&u8g, 24,35,numberBuffer);
  intTo2Chars(alarmTime[1]);
  u8g_DrawStr(&u8g, 54,35,numberBuffer);
  //draw day of week set up to a week in advance
  int dayAhead = dateArray[3] - 1; //set current day
  for(int i = 0; i < alarmTime[2]; i++){ // add the exra days
    dayAhead++;
    if(dayAhead > 6){
      dayAhead = 0;
    }
  }

  //check if alarms are active
  if(alarmToggle!=prevAlarmToggle){
    for(int k = 0; k < 2; k++){
      alarmTime[k] = 0;
    }
    int address = 0;
    if(alarmToggle == 0 && activeAlarms[alarmToggle]){
      address = ALARM_ADDRESS + 1;
    } else if(alarmToggle == 1 && activeAlarms[alarmToggle]) {
      address = ALARM_ADDRESS + 5;
    }
    alarmTime[0] = readFromEEPROM(address);
    address++;
    alarmTime[1] = readFromEEPROM(address);
    address++;
    int date = readFromEEPROM(address);
    address++;
    int monthSet = readFromEEPROM(address);
    if(date == dateArray[0]){
      alarmTime[2] = 0;
    } else {
      alarmTime[2] = (date + dayInMonth[monthSet - 1]) - dateArray[0];
    }
    prevAlarmToggle = alarmToggle;
  }

  u8g_DrawStr(&u8g, 84,35,days[dayAhead].c_str());

  if(alarmToggle == 0){
    u8g_DrawStr(&u8g, 0,0 + FONT_HEIGHT,"One");
  } else {
    u8g_DrawStr(&u8g, 0,0 + FONT_HEIGHT,"Two");
  }


  if(!activeAlarms[alarmToggle]){
    u8g_DrawStr(&u8g, 34,54,"Set");
  } else {
    u8g_DrawStr(&u8g, 30,54,"Unset");
  }

  u8g_DrawStr(&u8g, 70,54,"Swap");


  drawSelectors(alarmIndex);
}

void timerPage(){
  u8g_DrawStr(&u8g, 24,23,"H");
  u8g_DrawStr(&u8g, 54,23,"M");
  u8g_DrawStr(&u8g, 84,23,"S");

  intTo2Chars(timerArray[0]);
  u8g_DrawStr(&u8g, 24,35,numberBuffer);
  intTo2Chars(timerArray[1]);
  u8g_DrawStr(&u8g, 54,35,numberBuffer);
  intTo2Chars(timerArray[2]);
  u8g_DrawStr(&u8g, 84,35,numberBuffer);

  drawSelectors(timerIndex);

  if(isRunning){
    u8g_DrawStr(&u8g, 30,54,"Stop");
  } else {
    u8g_DrawStr(&u8g, 30,54,"Start");
  }

  u8g_DrawStr(&u8g, 70,54,"Reset");


}

void drawSelectors(int index){
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
      case 3: u8g_DrawFrame(&u8g,28,45,34,13); break; //draw rectangle around this option
      case 4: u8g_DrawFrame(&u8g,68,45,34,13); break; //draw rectangle around this option
    }
  }
}

void alertPage(){
  u8g_SetFont(&u8g, u8g_font_6x12);
  int xOffset = (alertTextLen * 6)/2;
  u8g_DrawStr(&u8g,64 - xOffset, 32,alertText);
  u8g_DrawStr(&u8g,64 - 60, 50,"Press OK to dismiss.");
  //remeber to add vibrate if needed
}

void notificationMenuPage(){
  if(shouldRemove){
    //remove the notification once read
    removeNotification(menuSelector);
    shouldRemove = false;
    //menuSelector = 0; //dont put it back to zero as when new message com in the order will get screwed
  }
  if(notificationIndex == 0){
    u8g_DrawStr(&u8g,30,32,"No Messages.");
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

void notificationMenuPageItem(int position){
  //draw title
  u8g_DrawStr(&u8g,x+3,y+Y_OFFSET+FONT_HEIGHT,notifications[position].title);

  u8g_SetFont(&u8g, u8g_font_04b_03);
  { //draw timestamp
    intTo2Chars(notifications[position].dateReceived[0]);
    u8g_DrawStr(&u8g,x+95,y + Y_OFFSET + FONT_HEIGHT,numberBuffer);
    u8g_DrawStr(&u8g,x+103,y + Y_OFFSET + FONT_HEIGHT,":");
    intTo2Chars(notifications[position].dateReceived[1]);
    u8g_DrawStr(&u8g,x+106,y + Y_OFFSET + FONT_HEIGHT,numberBuffer);
  }
  u8g_SetFont(&u8g, u8g_font_6x12);
}

void settingsMenuItem(int position){
  u8g_DrawStr(&u8g,x+3,y+Y_OFFSET+FONT_HEIGHT,settingKey[position].c_str());
  u8g_DrawStr(&u8g,x+3 + 104,y+Y_OFFSET+FONT_HEIGHT,itoa(settingValue[position],numberBuffer,10));
}


void notificationFullPage(int chosenNotification){
  int lines = 0;
  charIndex = 0; //make sure we rest index

  int textLength = notifications[chosenNotification].textLength;
  /*char *ptr = notifications[chosenNotification].text;
  while(*ptr != '\0'){
    textLength++;
    ptr++;
  }*/

  if(textLength > charsThatFit){
    for(int i=0; i < textLength; i++){
      if((charIndex >= charsThatFit) || i == (textLength - 1)){ // i == textLength so we catch what we ahve of a line if we dont have a complete line
        lineBuffer[charIndex] = notifications[chosenNotification].text[i]; //catch the last char
        u8g_DrawStr(&u8g,0,lines * 10 + FONT_HEIGHT + Y_OFFSET, lineBuffer); //draw the line
        lines++;
        charIndex = 0;
        memset(lineBuffer,0,sizeof(lineBuffer)); //reset the buffer we only do this because if a line is not 20 chars long the previos lines chars will be displayed
      } else {
        lineBuffer[charIndex] = notifications[chosenNotification].text[i];
        charIndex++;
      }
    }
  } else {
    u8g_DrawStr(&u8g,0,FONT_HEIGHT,notifications[chosenNotification].text);
    lines++;
  }
  lineCount = (lines); //- 1);
  intTo2Chars(lineCount);
  u8g_DrawStr(&u8g,64,50 + FONT_HEIGHT,numberBuffer);
  u8g_DrawStr(&u8g, 30, 50, String(textLength).c_str());
}

void homePage(int hour, int minute,int second){
  u8g_DrawCircle(&u8g,32,32,30,U8G_DRAW_ALL);
  u8g_DrawCircle(&u8g,32,32,29,U8G_DRAW_ALL);
  u8g_DrawStr(&u8g,59-32,2+ FONT_HEIGHT,"12");
  u8g_DrawStr(&u8g,59-32 + 3,45+ FONT_HEIGHT,"6");
  u8g_DrawStr(&u8g,7,32 + 3,"9");
  u8g_DrawStr(&u8g,53,32   + 3,"3");

  if(clockArray[0] > 12){
    u8g_DrawStr(&u8g,0,64,"PM");
  } else {
    u8g_DrawStr(&u8g,0,64,"AM");
  }


  float hours = (((hour * 30) + ((minute/2))) * (PI/180));
  int x2 = 32 + (sin(hours) * (clockRadius/2));
  int y2 = 32 - (cos(hours) * (clockRadius/2));
  u8g_DrawLine(&u8g,32,32,x2,y2); //hour hand

  float minutes = ((minute * 6) * (PI/180));
  int xx2 = 32 + (sin(minutes) * (clockRadius/1.4));
  int yy2 = 32 - (cos(minutes) * (clockRadius/1.4));
  u8g_DrawLine(&u8g,32,32,xx2,yy2);//minute hand

  float seconds = ((second * 6) * (PI/180));
  int xxx2 = 32 + (sin(seconds) * (clockRadius/1.3));
  int yyy2 = 32 - (cos(seconds) * (clockRadius/1.3));
  u8g_DrawLine(&u8g,32,32,xxx2,yyy2);//second hand

  if(settingValue[1] == 1){
    numberOfWidgets = numberOfNormalWidgets + numberOfDebugWidgets;
  } else {
    numberOfWidgets = numberOfNormalWidgets;
  }

  switch(widgetSelector){
    case 0: timeDateWidget(); break;
    case 1: digitalClockWidget(); break;
    case 2: weatherWidget(); break;
    case 3: u8g_SetFont(&u8g, u8g_font_7x14); u8g_DrawStr(&u8g,70,39 + 3,"Messages"); u8g_SetFont(&u8g, u8g_font_6x12); break;
    case 4: u8g_SetFont(&u8g, u8g_font_7x14); u8g_DrawStr(&u8g,80,39 + 3,"Timer"); u8g_SetFont(&u8g, u8g_font_6x12); break;
    case 5: u8g_SetFont(&u8g, u8g_font_7x14); u8g_DrawStr(&u8g,70,39 + 3,"Settings"); u8g_SetFont(&u8g, u8g_font_6x12); break;
    case 6: u8g_SetFont(&u8g, u8g_font_6x12); u8g_DrawStr(&u8g,73,42 + 3,"Reset"); u8g_DrawXBMP(&u8g,110,36,8,10,BLUETOOTH_CONNECTED); break;
    case 7: u8g_SetFont(&u8g, u8g_font_7x14); u8g_DrawStr(&u8g,75,42 + 3,"Alarms"); u8g_SetFont(&u8g, u8g_font_6x12); break;
  }

  //status bar - 15 px high for future icon ref
  u8g_DrawFrame(&u8g,75,0,53,15);
  //separator line between notifications and bt icon on the status bar
  u8g_DrawLine(&u8g,116,0,116,14);
  //batt voltage and connection separator
  u8g_DrawLine(&u8g,97,0,97,14);

  //notification indicator

  u8g_DrawStr(&u8g,119,-1 + FONT_HEIGHT,itoa(notificationIndex,numberBuffer,10));

  //battery voltage
  if(!isCharging && !isCharged){
    if(batteryPercentage != 100){
      char temp[3];
      u8g_DrawStr(&u8g,77,11,itoa(batteryPercentage,temp,10));
      u8g_DrawStr(&u8g,89,11,"%");
    }
  } else if(isCharged){
    //need draw something here to signify we are fully charged
    u8g_DrawStr(&u8g,77,11,itoa(100,numberBuffer,10)); //atm we are just drawing without the percentage
  } else {
    //draw symbol to show that we are charging here
    u8g_DrawXBMP(&u8g,80,2,14,12,CHARGING);
  }

  //connection icon
  if(isConnected){
    u8g_DrawXBMP(&u8g,102,2,8,10,BLUETOOTH_CONNECTED);
  } else {
    u8g_DrawStr(&u8g,102,11,"NC");
  }
}

/*
* Home page widgets
*/

void timeDateWidget(){
  //display date from RTC
  u8g_SetFont(&u8g, u8g_font_7x14);
  u8g_DrawStr(&u8g,72,20+14,days[dateArray[3] - 1].c_str());
  intTo2Chars(dateArray[0]);
  u8g_DrawStr(&u8g,72,34+14,numberBuffer);
  u8g_DrawStr(&u8g,90,34+14,months[dateArray[1] - 1].c_str());
  intTo2Chars(dateArray[2]);
  u8g_DrawStr(&u8g,72,48+14,numberBuffer);
  u8g_SetFont(&u8g, u8g_font_6x12);
}

void digitalClockWidget(){
  u8g_SetFont(&u8g, u8g_font_7x14);
  u8g_DrawFrame(&u8g,68,28,60,16);
  intTo2Chars(clockArray[0]);
  u8g_DrawStr(&u8g,69,28+14,numberBuffer);
  u8g_DrawStr(&u8g,83,28+14,":");
  intTo2Chars(clockArray[1]);
  u8g_DrawStr(&u8g,91,28+14,numberBuffer);
  u8g_DrawStr(&u8g,105,28+14,":");
  intTo2Chars(clockArray[2]);
  u8g_DrawStr(&u8g,113,28+14,numberBuffer);
  u8g_SetFont(&u8g, u8g_font_6x12);
}

void weatherWidget(){
  if(weatherData){
    //change fonts
    u8g_SetFont(&u8g, u8g_font_7x14);
    u8g_DrawStr(&u8g,72,19+FONT_HEIGHT,weatherDay);

    u8g_SetFont(&u8g, u8g_font_04b_03);
    intTo2Chars(timeWeGotWeather[0]);
    u8g_DrawStr(&u8g,105,15+ FONT_HEIGHT,numberBuffer);
    u8g_DrawStr(&u8g,113,15+ FONT_HEIGHT,":");
    intTo2Chars(timeWeGotWeather[1]);
    u8g_DrawStr(&u8g,116,15+ FONT_HEIGHT,numberBuffer);

    u8g_SetFont(&u8g, u8g_font_6x12);

    u8g_DrawStr(&u8g,72,28+FONT_HEIGHT,weatherTemperature);
    u8g_DrawStr(&u8g,102,28+FONT_HEIGHT,"C");

    int index = 0;
    charIndex = 0;
    if(contains(weatherForecast,' ',sizeof(weatherForecast))){
      for(int i=0; i < sizeof(weatherForecast); i++){
        if(weatherForecast[i]==' ' || weatherForecast[i] == 0){ // == 0  find the end of the data
          u8g_DrawStr(&u8g,72,(38+FONT_HEIGHT) + (index * 10),lineBuffer);//draw it
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
      u8g_DrawStr(&u8g,72,38+FONT_HEIGHT,weatherForecast);
    }


  } else {
    //print that weather is not available
    u8g_SetFont(&u8g, u8g_font_7x14);
    u8g_DrawStr(&u8g,70,34 + 3,"Weather");
    u8g_DrawStr(&u8g,70,50 + 3,"Data N/A");
    u8g_SetFont(&u8g, u8g_font_6x12);
  }
}

/*
* Input handling methods
*/

void handleInput(){
  int  vector = getConfirmedInputVector();
    if(vector!=lastVector){
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
      alarmTime[alarmIndex]++;
      if(alarmTime[alarmIndex] > alarmMaxValues[alarmIndex]){
        alarmTime[alarmIndex] = alarmMaxValues[alarmIndex];
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

void menuUp(int size){
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
      alarmTime[alarmIndex]--;
      if(alarmTime[alarmIndex] < 0){
        alarmTime[alarmIndex] = 0;
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
        resetBTModule();
    } else if(widgetSelector == 7){
      pageIndex = ALARM_PAGE;
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
      activeAlarms[alarmToggle] = !activeAlarms[alarmToggle];
      if(activeAlarms[alarmToggle]){
        if((dateArray[0] + alarmTime[2]) > dayInMonth[dateArray[1] - 1]){
          setAlarm(alarmToggle, alarmTime[0], alarmTime[1], ((dateArray[0] + alarmTime[2]) - dayInMonth[dateArray[1] - 1]),dateArray[1]); // (dateArray[0] + alarmTime[2]) == current date plus the days in advance we want to set the
          Serial.print("Alarm set for the : ");
          Serial.println(((dateArray[0] + alarmTime[2]) - dayInMonth[dateArray[1] - 1]));
        } else {
          setAlarm(alarmToggle, alarmTime[0], alarmTime[1], (dateArray[0] + alarmTime[2]),dateArray[1]);
          Serial.print("Alarm set for the : ");
          Serial.println((dateArray[0] + alarmTime[2]));
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

int getConfirmedInputVector()
{
  static int lastConfirmedVector = 0;
  static int lastVector = -1;
  static long unsigned int heldVector = 0L;

  // Combine the inputs.
  int rawVector =
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

void getWeatherData(char weatherItem[],int len){
  char *weaPtr = weatherItem;
  weaPtr+=3; //remove the tag
  int charIndex = 0;
  int index = 0;
  for(int i=0; i < len;i++){
    char c = *weaPtr; //derefence pointer to get value in char[]
    if(c=='<'){
      //split the t and more the next item
      weaPtr+=2;
      index++;
      charIndex = 0;
    } else {
      if(index==0){
        weatherDay[charIndex] = c;
        charIndex++;
      } else if(index==1){
        weatherTemperature[charIndex] = c;
        charIndex++;
      } else if(index==2){
        weatherForecast[charIndex] = c;
        charIndex++;
      }
    }
    weaPtr++; //move pointer along
  }
  Serial.print(F("Day: "));
  Serial.println(weatherDay);
  Serial.print(F("Temperature: "));
  Serial.println(weatherTemperature);
  Serial.print(F("Forecast: "));
  Serial.println(weatherForecast);
  for(int l=0; l < 2; l++){
    timeWeGotWeather[l] = clockArray[l];
  }
  weatherData = true;
}

void getNotification(char notificationItem[],int len){
  //split the <n>
  char *notPtr = notificationItem;
  notPtr+=3; //'removes' the first 3 characters
  int index = 0;
  int charIndex = 0;
  for(int i=0; i < len;i++){
    char c = *notPtr; //dereferences point to find value
    if(c=='<'){
      //split the i and more the next item
      notPtr+=2; // on two becuase this char is one
      index++;
      charIndex = 0;
    } else {
      if(index==0){
        notifications[notificationIndex].packageName[charIndex] = c;
        charIndex++;
      }else if(index==1){
        notifications[notificationIndex].title[charIndex] = c;
        charIndex++;
      } else if(index==2){
        notifications[notificationIndex].text[charIndex] = c;
        charIndex++;
      }
    }
    notPtr++; //move along
  }
  //finally get the timestamp of whenwe recieved the notification
  notifications[notificationIndex].dateReceived[0] = clockArray[0];
  notifications[notificationIndex].dateReceived[1] = clockArray[1];

  notifications[notificationIndex].textLength = charIndex;

  Serial.print(F("Notification title: "));
  Serial.println(notifications[notificationIndex].title);
  Serial.print(F("Notification text: "));
  Serial.println(notifications[notificationIndex].text);
  Serial.print("Text length: ");
  Serial.println(notifications[notificationIndex].textLength);
  notificationIndex++;
}



void getTimeFromDevice(char message[], int len){
  //sample data
  //<d>24 04 2016 17:44:46
  Serial.print(F("Date data: "));
  Serial.println(message);
  char buf[4];//max 2 chars
  int charIndex = 0;
  int dateIndex = 0;
  int clockLoopIndex = 0;
     bool gotDate = false;
     for(int i = 3; i< len;i++){ // i = 3 skips first 3 chars
      if(!gotDate){
       if(message[i]==' '){
          dateArray[dateIndex] = atoi(buf);
          charIndex = 0;
          dateIndex++;
          if(dateIndex >= 3){
            gotDate = true;
            charIndex = 0;
            memset(buf, 0, sizeof(buf));
          }
       } else {
        buf[charIndex] = message[i];
        charIndex++;

       }
      } else {
        if(message[i]==':'){
            clockArray[clockLoopIndex] = atoi(buf); //ascii to int
            charIndex = 0;
            clockLoopIndex++;
        } else {
          buf[charIndex] = message[i];
          charIndex++;
        }
      }
     }
     //Read from the RTC
     RTC.read(tm);
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

void setAlarm(int alarmType, int hours, int minutes, int date,int month){
  int start = 0;
  if(alarmType == 0){
    RTC.setAlarm(ALM1_MATCH_DATE, minutes, hours, date); // minutes // hours // date (28th of the month)
    Serial.println("ALARM1 Set!");
    start = ALARM_ADDRESS;
  } else {
    RTC.setAlarm(ALM2_MATCH_DATE, minutes, hours, date); // minutes // hours // date (28th of the month)
    Serial.println("ALARM2 Set!");
    start = ALARM_ADDRESS + 4;
  }
  Serial.print("Starting write at address: ");
  Serial.println(start);
  saveToEEPROM(start,true); //set alarm active
  start++;
  saveToEEPROM(start,hours);
  start++;
  saveToEEPROM(start,minutes);
  start++;
  saveToEEPROM(start,date);
  start++;
  saveToEEPROM(start,month);
}

void createAlert(char text[],int len, int vibrationTime){
  if(len < 20){
    lastPage = pageIndex;
    alertTextLen = len;
    for(int i =0; i < len; i++){
      alertText[i] = text[i];
    }
    vibrate(vibrationTime);
    pageIndex = ALERT;
  } else {
    Serial.println(F("Not Creating Alert, text to big!"));
  }
}

void vibrate(int vibrationTime){
  alertVibrationCount = (vibrationTime) * 2;// double it as we toggle vibrate twice a second
}

void saveToEEPROM(int address,int value){
  if(address < EEPROM.length()){
    EEPROM.write(address,value);
  }
}

int readFromEEPROM(int address){
    return EEPROM.read(address);
}

void drawTriangle(int x, int y, int size, int direction){
  // some triangle are miss-shapen need to fix
  switch (direction) {
    case 1: u8g_DrawTriangle(&u8g,x,y,x+size,y, x+(size/2), y+(size/2)); break; //down
    case 2: u8g_DrawTriangle(&u8g,x,y,x+size,y, x+(size/2), y-(size/2)); break; //up
    case 3: u8g_DrawTriangle(&u8g,x+size,y,x,y-(size/2),x+size,y-size); break; // left
    case 4: u8g_DrawTriangle(&u8g,x + size,y-(size/2),x,y,x,y-size); break; // right
  }
}

void setClockTime(int hours,int minutes,int seconds, int days, int months, int years){
  tm.Hour = hours;
  tm.Minute = minutes;
  tm.Second = seconds;
  tm.Day = days;
  tm.Month = months;
  tm.Year = CalendarYrToTm(years);
  t = makeTime(tm);

  if(RTC.set(t) == 1) { // Success
    setTime(t);
    Serial.println(F("Writing time to RTC was successfull!"));
    gotUpdatedTime= true;
  } else {
    Serial.println(F("Writing to clock failed!"));
  }


}

void resetBTModule(){
  HWSERIAL.print("AT"); //disconnect
  delay(100); //need else the module won't see the commands as two separate ones
  HWSERIAL.print("AT+RESET"); //then reset
  createAlert("BT Module Reset.",16,0);
}

void resetTransmissionVariables(){
  finalDataIndex = 0; //reset final data
  memset(finalData, 0, sizeof(finalData)); // clears array - (When add this code to the OS we need to add the memsets to the resetTransmission() func)
  readyToProcess = false;
}

void removeNotification(int pos){
  if ( pos >= notificationIndex + 1 ){
    Serial.println(F("Can't delete notification."));
  } else {
    //need to zero out the array or stray chars will overlap with notifications
    memset(notifications[pos].text,0,sizeof(notifications[pos].text));
    memset(notifications[pos].title,0,sizeof(notifications[pos].title));
    memset(notifications[pos].packageName,0,sizeof(notifications[pos].packageName));
    for ( int c = pos ; c < (notificationIndex - 1) ; c++ ){
       notifications[c] = notifications[c+1];
    }
    Serial.print(F("Removed notification at position: "));
    Serial.println(pos);
    //lower the index
    notificationIndex--;
  }
}

float getBatteryVoltage(){
  /*
   * WARNING: Add voltage divider to bring batt voltage below 3.3v at all times! Do this before pluggin in the Batt or will destroy the Pin in a best case scenario
   * and will destroy the teensy in a worst case.
   */
   float reads = 0;
   for(int i=0; i<100; i++){
    reads+= analogRead(BATT_READ);
   }
   // R1 = 2000, R2 = 3300
   // Vin = (Vout * (R1 + R2)) / R2
  return ((reads/100) * (3.3 / 1024) * (3300 + 2000))/(3300);
}

/*
* Utility methods
*/

void intTo2Chars(int number){
  memset(numberBuffer,0,sizeof(numberBuffer));
  if(number < 10){
    //not working atm
    numberBuffer[0] = '0';
    numberBuffer[1] = (number + 48);

  } else {
    itoa(number,numberBuffer,10);
  }
}

bool startsWith(char data[], char charSeq[], int len){
    for(int i=0; i < len; i++){
      if(!(data[i]==charSeq[i])){
        return false;
      }
    }
    return true;
}

bool contains(char data[], char character, int lenOfData){
  for(int i = 0; i < lenOfData; i++){
    if(data[i] == character){
      return true;
    }
  }
  return false;
}

int FreeRam() {
  char top;
  #ifdef __arm__
    return &top - reinterpret_cast<char*>(sbrk(0));
  #else  // __arm__
    return __brkval ? &top - __brkval : &top - &__bss_end;
  #endif  // __arm__
}
