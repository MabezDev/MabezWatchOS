#include <Arduino.h>

#include <u8g_i2c.h>
#include <i2c_t3.h>
#include <Time.h>
#include <DS1302RTC.h>
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

//RTC object
DS1302RTC RTC(12,11,13);// 12 is reset/chip select, 11 is data, 13 is clock

/*
 * This has now been fully updated to the teensy LC, with an improved transmission algorithm.
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
 *
 *  Buglist:
 *  -if a program fails to send the <f> tag correctly, we will run out of ram as we have already allocated RAM for notifcations
    (Solution remove string (Use char[]), only allow max payload size of something) - NEEDS TESTING
 *
 *  Todo:
 *    -Use touch capacitive sensors instead of switches to simplify PCB and I think it will be nicer to use. -TESTED
 *    -Maybe use teensy 3.2, more powerful, with 64k RAM, and most importantly has a RTC built in
 *    -add month and year - DONE
 *    -add reply phrases, if they are short answers like yeah, nah, busy right now etc.
 *    -Could use a on screen keyabord but it miught be too much hassle to use properly.
 *    - [MAJOR] need to completely remove string from this sketch, and use char[] instead - DONE
 *    -text wrapping on the full notification page
 *    -remove all the handle input functions, add one that doies all using the current page to decide what input to use - DONE
 *    -add a input time out where if the user does not interact with the watch the widget goes back to a clock one - DONE
          - change the widget to the favorite stored in the settings
 *    - fix the alert function to have two pulses without delay (Fix could be alertcounter set it to 2 then vibrate and -- from alertcounter)
 *    - use isPrintable on a char to find out if we can send it (maybe do thios on the phone side)
 *    - finish timer app (use our system click to time the seconds) - BASIC FUNCTIONALITY COMPLETE
          - need to alert the suer when the timer is up
          - make it look better
 *    -settings page
          - favourite widget (will default to once no input is recieved)
 *    -use eeprom to store the settings so we dont have to set them
 *    -the lithium charger board has a two leds one to show chargin one to show chargin done, need to hook into these to read when we are charging(to charge the RTC  batt(trickle charge))
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
      - Add time stamp for notifications? - (Would NOT need to modify notification structure as we can just use out RTC)
      - Keep tabs on Weather data to mkae sure its still valid (make sure were not displaying data that is hours old or something)
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

//need to use 4,2,1 as no combination of any of the numbers makes the same number, where as 1,2,3 1+2 = 3 so there is no individual state.
#define UP_ONLY  4
#define OK_ONLY  2
#define DOWN_ONLY  1
#define UP_OK  (UP_ONLY|OK_ONLY)
#define UP_DOWN  (UP_ONLY|DOWN_ONLY)
#define DOWN_OK  (OK_ONLY|DOWN_ONLY)
#define ALL_THREE (UP_ONLY|OK_ONLY|DOWN_ONLY)
#define NONE_OF_THEM  0

#define isButtonPressed(pin)  (digitalRead(pin) == LOW)

int lastVector = 0;
long prevButtonPressed = 0;

//serial retrieval vars
//serial retrieval vars
char message[100];
char *messagePtr; //this could be local to the receiving loop
char finalData[250];
int finalDataIndex = 0; //index is required as we dunno when we stop
int messageIndex = 0;
//int chunkCount = 0;
bool readyToProcess = false;
bool receiving = false;

//date contants
String PROGMEM months[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
String PROGMEM days[7] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
//weather vars
bool weatherData = false;
char weatherDay[4];
char weatherTemperature[4];
char weatherForecast[25];


//time and date constants
tmElements_t tm;
time_t t;
int PROGMEM clockRadius = 32;
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
} Notification;

//notification vars
int notificationIndex = 0;
int PROGMEM notificationMax = 10;
Notification notifications[10];

bool shouldRemove = false;

//pin constants
const int PROGMEM OK_BUTTON = 4;
const int PROGMEM DOWN_BUTTON = 5;
const int PROGMEM UP_BUTTON = 3;
const int PROGMEM BATT_READ = A6;
const int PROGMEM VIBRATE_PIN = 10;

//navigation constants
const int PROGMEM HOME_PAGE = 0;
const int PROGMEM NOTIFICATION_MENU = 1;
const int PROGMEM NOTIFICATION_BIG = 2;
const int PROGMEM TIMER = 4;
const int PROGMEM SETTINGS = 5;

const int PROGMEM ALERT = 6;

//UI constants
const int PROGMEM MENU_ITEM_HEIGHT = 16;
const int PROGMEM FONT_HEIGHT = 12; //need to add this to the y for all DrawStr functions

//navigation vars
int pageIndex = 0;
int menuSelector = 0;
int widgetSelector = 0;
const int numberOfNormalWidgets = 5; // actually 3, 0,1,2.
const int numberOfDebugWidgets = 1;
int numberOfWidgets = 0;
int numberOfPages = 5; // actually 6

const int x = 6;
int y = 0;
int Y_OFFSET = 0;

int lineCount = 0;
int currentLine = 0;

//batt monitoring
float batteryVoltage = 0;
int batteryPercentage = 0;

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
String alertText; //remeber to replace this with a char[] version
bool alertVibrate = false;

//icons
const byte PROGMEM BLUETOOTH_CONNECTED[] = {
   0x31, 0x52, 0x94, 0x58, 0x38, 0x38, 0x58, 0x94, 0x52, 0x31
};

int loading = 3; // time the loading screen is show for

const byte PROGMEM LOGO[] = {
  0x00, 0x00, 0x00, 0x1e, 0x00, 0x0f, 0x3e, 0x80, 0x0f, 0x3e, 0x80, 0x0f,
   0x7e, 0xc0, 0x0f, 0x6e, 0xc0, 0x0e, 0xee, 0xc0, 0x0e, 0xce, 0x60, 0x0e,
   0xce, 0x61, 0x0e, 0xce, 0x71, 0x0e, 0x8e, 0x31, 0x0e, 0x8e, 0x3b, 0x0e,
   0x0e, 0x1b, 0x0e, 0x0e, 0x1f, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
   0x0e, 0x0e, 0x0e, 0x0e, 0x00, 0x0e, 0x0e, 0x00, 0x0e, 0x0e, 0x00, 0x0e,
   0x0e, 0x00, 0x0e, 0x0e, 0x00, 0x0e, 0x0e, 0x00, 0x0e, 0x00, 0x00, 0x00
};

void setup(void) {
  Serial.begin(9600);
  HWSERIAL.begin(9600);

  pinMode(OK_BUTTON,INPUT_PULLUP);
  pinMode(DOWN_BUTTON,INPUT_PULLUP);
  pinMode(UP_BUTTON,INPUT_PULLUP);

  pinMode(BATT_READ,INPUT);
  pinMode(VIBRATE_PIN,OUTPUT);

  RTC.haltRTC(false);
  RTC.writeEN(false);

  u8g_prepare();

  readSettingsFromEEPROM(); // load the settings in from EEPROM

  messagePtr = &message[0]; // could have used messagePtr = message

  //setup batt read pin
  analogReference(DEFAULT);
  analogReadResolution(10);// 2^10 = 1024
  analogReadAveraging(32);//smoothing

  /*
   * HWSERIAL.print("AT"); to cut the connection so we have to reconnect if the watch crashes
   */
   HWSERIAL.print("AT");

   Serial.println("MabezWatch OS Loaded!");
}

void u8g_prepare(void) {
  u8g_InitComFn(&u8g, &u8g_dev_sh1106_128x64_2x_i2c, u8g_com_hw_i2c_fn);
  u8g_SetFont(&u8g, u8g_font_6x12);
}

void drawClock(int hour, int minute,int second){
  //need to add am pm thing
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
    numberOfWidgets = numberOfNormalWidgets ;
  }

  switch(widgetSelector){
    case 0: timeDateWidget(); break;
    case 1: digitalClockWidget(); break;
    case 2: weatherWidget(); break;
    case 3: u8g_SetFont(&u8g, u8g_font_7x14); u8g_DrawStr(&u8g,70,39 + 3,"Messages"); u8g_SetFont(&u8g, u8g_font_6x12); break;
    case 4: u8g_SetFont(&u8g, u8g_font_7x14); u8g_DrawStr(&u8g,80,39 + 3,"Timer"); u8g_SetFont(&u8g, u8g_font_6x12); break;
    case 5: u8g_SetFont(&u8g, u8g_font_7x14); u8g_DrawStr(&u8g,70,39 + 3,"Settings"); u8g_SetFont(&u8g, u8g_font_6x12); break;
    case 6: u8g_SetFont(&u8g, u8g_font_6x12); u8g_DrawStr(&u8g,73,42 + 3,"Reset"); u8g_DrawXBMP(&u8g,110,36,8,10,BLUETOOTH_CONNECTED); break;
  }

  //status bar - 15 px high for future icon ref
  u8g_DrawFrame(&u8g,75,0,53,15);
  //separator line between notifications and bt icon on the status bar
  u8g_DrawLine(&u8g,116,0,116,14);
  //batt voltage and connection separator
  u8g_DrawLine(&u8g,97,0,97,14);

  //notification indicator
  u8g_DrawStr(&u8g,119,-1 + FONT_HEIGHT,String(notificationIndex).c_str());

  //battery voltage
  String batt = String(batteryPercentage) + "%";
  u8g_DrawStr(&u8g,77,11,batt.c_str());
  //connection icon
  if(isConnected){
    u8g_DrawXBMP(&u8g,102,2,8,10,BLUETOOTH_CONNECTED);
  } else {
    u8g_DrawStr(&u8g,102,11,"NC");
  }
}

void timeDateWidget(){
  //display date from RTC
  u8g_SetFont(&u8g, u8g_font_7x14);
  u8g_DrawStr(&u8g,72,20+14,days[dateArray[3] - 1].c_str());
  u8g_DrawStr(&u8g,72,34+14,intTo2Chars(dateArray[0]).c_str());
  u8g_DrawStr(&u8g,90,34+14,months[dateArray[1] - 1].c_str());
  u8g_DrawStr(&u8g,72,48+14,String(dateArray[2]).c_str());
  u8g_SetFont(&u8g, u8g_font_6x12);
}

void digitalClockWidget(){
  u8g_SetFont(&u8g, u8g_font_7x14);
  u8g_DrawFrame(&u8g,68,28,60,16);
  u8g_DrawStr(&u8g,69,28+14,intTo2Chars(clockArray[0]).c_str());
  u8g_DrawStr(&u8g,83,28+14,":");
  u8g_DrawStr(&u8g,91,28+14,intTo2Chars(clockArray[1]).c_str());
  u8g_DrawStr(&u8g,105,28+14,":");
  u8g_DrawStr(&u8g,113,28+14,intTo2Chars(clockArray[2]).c_str());
  u8g_SetFont(&u8g, u8g_font_6x12);
}

void alertPage(){
  u8g_SetFont(&u8g, u8g_font_6x12);
  int xOffset = (sizeof(alertText) * 6)/2;
  u8g_DrawStr(&u8g,64 - xOffset, 32,alertText.c_str());
  u8g_DrawStr(&u8g,64 - 60, 50,"Press OK to dismiss.");
  //remeber to add vibrate if needed
}

void showMenu(int numberOfItems,void itemInMenuFunction(int),bool showSelector){
  for(int i=0; i < numberOfItems + 1;i++){
    int startY = 0;
    if(i==menuSelector && showSelector){
        u8g_DrawStr(&u8g,0,y+Y_OFFSET+FONT_HEIGHT,">");
    }
    if(i!=numberOfItems){
      //u8g_DrawStr(&u8g,x+3,y+Y_OFFSET+FONT_HEIGHT,notifications[i].title);
        itemInMenuFunction(i);
    } else {
      u8g_DrawStr(&u8g,x + 3,y + Y_OFFSET+FONT_HEIGHT, "Back");
    }
    y += MENU_ITEM_HEIGHT;
    u8g_DrawFrame(&u8g,x,startY,128,y +Y_OFFSET);
  }
  y = 0;
}

String intTo2Chars(int number){
  if(number < 10){
    return "0" + String(number);
  } else {
    return String(number);
  }

}

void weatherWidget(){
  if(weatherData){
    //change fonts
    u8g_SetFont(&u8g, u8g_font_7x14);
    u8g_DrawStr(&u8g,72,19+FONT_HEIGHT,weatherDay);
    u8g_DrawStr(&u8g,72,32+FONT_HEIGHT,weatherTemperature);

    int index = 0;
    String twoLines[2];
    for(int i=0; i < sizeof(weatherForecast); i++){
      if(weatherForecast[i]==' '){
        index++;
        if(index == 3){
          break;
        }
      } else {
        twoLines[index] += weatherForecast[i];
      }
    }

    u8g_SetFont(&u8g, u8g_font_6x12);

  if(index == 0){
    u8g_DrawStr(&u8g,72,42+FONT_HEIGHT,weatherForecast);
  } else {
    u8g_DrawStr(&u8g,72,42+FONT_HEIGHT,twoLines[0].c_str());
    u8g_DrawStr(&u8g,72,52+FONT_HEIGHT,twoLines[1].c_str());
  }

  } else {
    //print that weather is not available
    u8g_SetFont(&u8g, u8g_font_7x14);
    u8g_DrawStr(&u8g,70,34 + 3,"Weather");
    u8g_DrawStr(&u8g,70,50 + 3,"Data N/A");
    u8g_SetFont(&u8g, u8g_font_6x12);
  }
}

void updateSystem(){
  long current = millis();
  if(current - prevMillis >= clockUpdateInterval){
    prevMillis = current;

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
    batteryPercentage = ((batteryVoltage - 3)/1.2)*100;
    //make sure we never display a negative percentage
    if(batteryPercentage < 0){
      batteryPercentage = 0;
    }

    if(loading!=0){
      loading--;
    }

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
            //todo show a notifcation and vibrate to show timer is done
            createAlert("Timer Finished!");
          }
        }
      }
    }

    Serial.println("==============================================");
    if(isConnected){
      connectedTime++;
      Serial.print("Connected for ");
      Serial.print(connectedTime);
      Serial.println(" seconds.");
    } else {
      Serial.print("Connected Status: ");
      Serial.println(isConnected);
      connectedTime = 0;
    }
    Serial.print("Number of Notifications: ");
    Serial.println(notificationIndex);
    Serial.print("Time: ");
    Serial.print(clockArray[0]);
    Serial.print(":");
    Serial.print(clockArray[1]);
    Serial.print(":");
    Serial.print(clockArray[2]);
    Serial.print("    Date: ");
    Serial.print(dateArray[0]);
    Serial.print("/");
    Serial.print(dateArray[1]);
    Serial.print("/");
    Serial.println(dateArray[2]);

    // ram should never change as we are never dynamically allocating things
    //after tests this holes true, with this sketch i have a constant 3800 bytes (ish) for libs and oother stuff
    Serial.print("Free RAM:");
    Serial.println(FreeRam());
    Serial.println("==============================================");

    //send a message to the phone
    HWSERIAL.print("Testing123");

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
   //change math below to reflect the voltage divider change
  return (reads/100) * (3.3 / 1024);
}

void alert(){
  //buzzes with vibration motor
  //activates transistor that connects the buzzer with 3.3v
  // due to the nature of transistors there is 0.7v drop
  //bringing the voltage to 2.6v - which is safe for the motors
    digitalWrite(VIBRATE_PIN,HIGH);
    delay(500);
    digitalWrite(VIBRATE_PIN,LOW);
}

void handleInput(){
  int  vector = getConfirmedInputVector();
    if(vector!=lastVector){
      if (vector == UP_DOWN){
        Serial.println("Dual click detected!");
        handleDualClick();
      } else if (vector == UP_ONLY){
        handleUpInput();
      } else if(vector == DOWN_ONLY){
        handleDownInput();
      } else if(vector == OK_ONLY){
        handleOkInput();
      } else if(vector == ALL_THREE){
        Serial.println("Return to menu Combo");
        pageIndex = HOME_PAGE; // take us back to the home page
        widgetSelector = settingValue[0];// and our fav widget
      }
      prevButtonPressed = millis();
    }
      if(vector == NONE_OF_THEM){
        if(((millis() - prevButtonPressed) > INPUT_TIME_OUT) && (prevButtonPressed != 0)){
          Serial.println("Time out input");
          prevButtonPressed = 0;
          pageIndex = HOME_PAGE; // take us back to the home page
          widgetSelector = settingValue[0]; //and our fav widget
        }
      }
    lastVector = vector;
}

void handleDualClick(){
    pageIndex = HOME_PAGE;
}

void handleUpInput(){
  Serial.println("Up Click Detected");
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
  } else {
    Serial.println("Unknown Page.");
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
     menuSelector = size;
  }
}

void menuDown(){
  menuSelector--;
  if(menuSelector < 0){
     menuSelector = 0;
  }
  //plus y
  if((menuSelector >= 3)){
    Y_OFFSET += MENU_ITEM_HEIGHT;
  }
}

void handleDownInput(){
  Serial.println("Down Click Detected");
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
  } else {
    Serial.println("Unknown Page.");
  }
}

void handleOkInput(){
  Serial.println("OK Click Detected");
  if(pageIndex == HOME_PAGE){
    if(widgetSelector == 3){
      pageIndex = NOTIFICATION_MENU;
    } else if(widgetSelector == 4){
      pageIndex = TIMER;
    } else if(widgetSelector == 5){
      pageIndex = SETTINGS;
    } else if(widgetSelector == 6){
        resetBTModule();
    }
    Y_OFFSET = 0;
  } else if(pageIndex == NOTIFICATION_MENU){
    if(menuSelector != notificationIndex){//last one is the back item
      pageIndex = NOTIFICATION_BIG;
    } else {
      //remove the notification
      menuSelector = 0;//rest the selector
      pageIndex = HOME_PAGE;// go back to list of notifications
    }
  } else if(pageIndex == NOTIFICATION_BIG){
    shouldRemove = true;
    Y_OFFSET = 0;
    lineCount = 0;//reset number of lines
    currentLine = 0;// reset currentLine back to zero
    pageIndex = NOTIFICATION_MENU;
  } else if(pageIndex == TIMER){
    if(timerIndex==3){
      isRunning = !isRunning; //start/stop timer
    } else if(timerIndex == 4){
      Serial.println("Resetting timer.");
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
        saveSettingToEEPROM(menuSelector);
      }
    }
  } else if(pageIndex == ALERT){
    //memset(alertText,0,sizeof(alertText)); //reset the alertText
    alertText = "";
    pageIndex = lastPage; //go back
  } else {
    Serial.println("Unknown Page.");
  }
}

void saveSettingToEEPROM(int address){
  if(address < EEPROM.length()){
    EEPROM.write(address,settingValue[address]);
  }
}

void readSettingsFromEEPROM(){
  for(int i=0; i < numberOfSettings; i++){
    settingValue[i] = EEPROM.read(i);
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

void drawTriangle(int x, int y, int size, int direction){
  switch (direction) {
    case 1: u8g_DrawTriangle(&u8g,x,y,x+size,y, x+(size/2), y+(size/2)); break; //down
    case 2: u8g_DrawTriangle(&u8g,x,y,x+size,y, x+(size/2), y-(size/2)); break; //up
    case 3: u8g_DrawTriangle(&u8g,x+size,y,x,y-(size/2),x+size,y-size); break; // left
    case 4: u8g_DrawTriangle(&u8g,x + size,y-(size/2),x,y,x,y-size); break; // right
  }

}


void timerApp(){
  // need to a add input for two button presses to get out of this app
  u8g_DrawStr(&u8g, 24,23,"H");
  u8g_DrawStr(&u8g, 54,23,"M");
  u8g_DrawStr(&u8g, 84,23,"S");

  u8g_DrawStr(&u8g, 24,35,String(timerArray[0]).c_str());
  u8g_DrawStr(&u8g, 54,35,String(timerArray[1]).c_str());
  u8g_DrawStr(&u8g, 84,35,String(timerArray[2]).c_str());

  if(locked){
    switch (timerIndex) {
      case 0: drawTriangle(22,10,8,2); drawTriangle(22,38,8,1); break;
      case 1: drawTriangle(52,10,8,2); drawTriangle(52,38,8,1); break;
      case 2: drawTriangle(82,10,8,2); drawTriangle(82,38,8,1); break;
    }
  } else {
    switch (timerIndex) {
      case 0: drawTriangle(22,8,8,1); break;
      case 1: drawTriangle(52,8,8,1);break;
      case 2: drawTriangle(82,8,8,1); break;
      case 3: u8g_DrawFrame(&u8g,28,45,34,13); break; //draw rectangle around this option
      case 4: u8g_DrawFrame(&u8g,68,45,34,13); break; //draw rectangle around this option
    }
  }

  if(isRunning){
    u8g_DrawStr(&u8g, 30,54,"Stop");
  } else {
    u8g_DrawStr(&u8g, 30,54,"Start");
  }

  u8g_DrawStr(&u8g, 70,54,"Reset");


}

void createAlert(String text){
  lastPage = pageIndex;
  alertText = text;
  pageIndex = ALERT;
}

void resetBTModule(){
  HWSERIAL.print("AT"); //disconnect
  delay(100); //should probably remove this
  HWSERIAL.print("AT+RESET"); //then reset
  createAlert("BT Module Reset.");
}

void removeNotification(int pos){
  if ( pos >= notificationIndex + 1 ){
    Serial.println("Can't delete notification.");
  } else {
    for ( int c = pos ; c < (notificationIndex - 1) ; c++ ){
       notifications[c] = notifications[c+1];
    }
    Serial.print("Removed notification at position: ");
    Serial.println(pos);
    //lower the index
    notificationIndex--;
  }
}


void fullNotification(int chosenNotification){
  int charsThatFit = 20;
  int charIndex = 0;
  int lines = 0;

  String text = notifications[chosenNotification].text;

  String linesArray[8]; // (150/charsThatFit) = 7.5 = 8

  if(sizeof(notifications[chosenNotification].text) > charsThatFit){
    for(int i=0; i< 150; i++){
      if(charIndex > charsThatFit){
        linesArray[lines] += text.charAt(i);
        lines++;
        charIndex = 0;
      } else {
        linesArray[lines] += text.charAt(i);
        charIndex++;
      }
    }
    for(int j=0; j < lines + 1; j++){
      u8g_DrawStr(&u8g,0,j * 10 +FONT_HEIGHT + Y_OFFSET, linesArray[j].c_str());
    }
    lineCount = lines;
  } else {
    u8g_DrawStr(&u8g,0,FONT_HEIGHT,text.c_str());
  }
}

void loop(void) {
  u8g_FirstPage(&u8g);
  do {
    if(loading !=0){
      u8g_DrawXBMP(&u8g,55,12,21,24,LOGO);
      u8g_DrawStr(&u8g,42,55,"Loading...");
    } else {
      switch(pageIndex){
        case 0: drawClock(clockArray[0],clockArray[1],clockArray[2]); break;
        case 1: showNotifications(); break;
        case 2: fullNotification(menuSelector); break;
        case 4: timerApp(); break;
        case 5: settingsPage(); break; //read the newst settings
        case 6: alertPage(); break;
      }
    }
  } while( u8g_NextPage(&u8g) );
    handleInput();
    while(HWSERIAL.available()){
    message[messageIndex] = char(HWSERIAL.read());//store string from serial command
    messageIndex++;
    if(messageIndex >= 99){
      //this message is too big something went wrong flush the message out the system and break the loop
      Serial.println("Error message overflow, flushing buffer and discarding message.");
      messageIndex = 0;
      memset(message, 0, sizeof(message)); //resetting array
      HWSERIAL.flush();
      break;
    }
    delay(1);
  }
  if(!HWSERIAL.available()){
    if(messageIndex > 0){
      Serial.print("Message: ");
      for(int i=0; i < messageIndex; i++){
        Serial.print(message[i]);
      }
      Serial.println();


      if(startsWith(message,"OK",2)){
          if(startsWith(message,"OK+C",4)){
            isConnected = true;
            Serial.println("Connected!");
          }
          if(startsWith(message,"OK+L",4)){
            isConnected = false;
            Serial.println("Disconnected!");
            //reset vars like got updated time and weather here also
          }
          messageIndex = 0;
          memset(message, 0, sizeof(message));
        }
      if(!receiving && startsWith(message,"<",1)){
        receiving = true;
      }else if(receiving && (message=="<n>" || message=="<d>" || message=="<w>")) {
        Serial.println("Message data missing, ignoring.");
        //we never recieved the end tag of a previous message
        //reset vars
        messageIndex = 0;
        memset(message, 0, sizeof(message));
        resetTransmissionVariables();
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
            Serial.println("FinalData is full, but there was more data to add. Discarding data.");
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
    Serial.print("Received: ");
    Serial.println(finalData);
    if(startsWith(finalData,"<n>",3)){
      if(notificationIndex < (notificationMax - 1)){ // 0-7 == 8
        getNotification(finalData,finalDataIndex);
      } else {
        Serial.println("Max notifications Hit.");
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

void resetTransmissionVariables(){
  finalDataIndex = 0; //reset final data
  memset(finalData, 0, sizeof(finalData)); // clears array - (When add this code to the OS we need to add the memsets to the resetTransmission() func)
  readyToProcess = false;
}

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
  Serial.print("Day: ");
  Serial.println(weatherDay);
  Serial.print("Temperature: ");
  Serial.println(weatherTemperature);
  Serial.print("Forecast: ");
  Serial.println(weatherForecast);
  weatherData = true;
}

int FreeRam() {
  char top;
  #ifdef __arm__
    return &top - reinterpret_cast<char*>(sbrk(0));
  #else  // __arm__
    return __brkval ? &top - __brkval : &top - &__bss_end;
  #endif  // __arm__
}

void showNotifications(){
  if(shouldRemove){
    //remove the notification once read
    removeNotification(menuSelector);
    shouldRemove = false;
    menuSelector = 0;
  }
  if(notificationIndex == 0){
    u8g_DrawStr(&u8g,30,32,"No Messages.");
  }
  showMenu(notificationIndex, notificationMenuItem,true);
}

void settingsPage(){
  //load settings from eeprom then display
  //need a data struct for the setting text, and its value
  //enable editing of these settings
  bool showCursor = true;
  if(locked){
    showCursor = false;
  }
  showMenu(numberOfSettings,settingsMenuItem,showCursor);
}

void notificationMenuItem(int position){
  u8g_DrawStr(&u8g,x+3,y+Y_OFFSET+FONT_HEIGHT,notifications[position].title);
}

void settingsMenuItem(int position){
  u8g_DrawStr(&u8g,x+3,y+Y_OFFSET+FONT_HEIGHT,settingKey[position].c_str());
  u8g_DrawStr(&u8g,x+3 + 104,y+Y_OFFSET+FONT_HEIGHT,String(settingValue[position]).c_str());
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
  Serial.print("Notification title: ");
  Serial.println(notifications[notificationIndex].title);
  Serial.print("Notification text: ");
  Serial.println(notifications[notificationIndex].text);
  notificationIndex++;
}



void getTimeFromDevice(char message[], int len){
  //sample data
  //<d>24 04 2016 17:44:46
  Serial.print("Date data: ");
  Serial.println(message);
  char buf[4];//max 2 chars
  int charIndex = 0;
  int dateIndex = 0;
  int clockLoopIndex = 0;
     bool gotDate = false;
     for(int i = 3; i< len;i++){ // i =3 skips first 3 chars
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
     if(!((tm.Hour == clockArray[0] && tm.Minute == clockArray[1] && dateArray[1] == tm.Day && dateArray[2] == tm.Month && dateArray[3] == tm.Year))){
        setClockTime(clockArray[0],clockArray[1],clockArray[2],dateArray[0],dateArray[1],dateArray[2]);
        Serial.println(F("Setting the clock!"));
     } else {
        //if it's correct we do not have to set the RTC and we just keep using the RTC's time
        Serial.println(F("Clock is correct already!"));
        gotUpdatedTime = true;
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


void setClockTime(int hours,int minutes,int seconds, int days, int months, int years){

  Serial.print("Hours: ");
  Serial.println(hours);

  tm.Hour = hours;
  tm.Minute = minutes;
  tm.Second = seconds;
  tm.Day = days;
  tm.Month = months;
  //This is correct
  tm.Year = CalendarYrToTm(years);
  t = makeTime(tm);
  if(RTC.set(t) == 0) { // Success
    setTime(t);
    Serial.println(F("Writing time to RTC was successfull!"));
    gotUpdatedTime= true;
  } else {
    Serial.println(F("Writing to clock failed!"));
  }
}
