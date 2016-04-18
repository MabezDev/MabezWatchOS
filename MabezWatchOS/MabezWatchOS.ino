#include <u8g_i2c.h>
#include <i2c_t3.h>
#include <Time.h>
#include <DS1302RTC.h>
#define HWSERIAL Serial1

//needed for calculating Free RAM on ARM based MC's
#ifdef __arm__
  extern "C" char* sbrk(int incr);
#else  // __ARM__
  extern char *__brkval;
  extern char __bss_end;
#endif  // __arm__

//u8g lib object without the wrapper due to lack of support of the OLED
u8g_t u8g;
//RTC object
DS1302RTC RTC(12,11,13);// 12 is reset/chip select, 11 is data, 13 is clock

/*
 * This has now been fully updated to the teensy LC, with an improved transmission algorithm. 
 * 
 * Update: 
 *  -Made UI Look much nicer.
 *  -Fixed notification sizes mean we can't SRAM overflow, leaves about 1k of SRAM for dynamic allocations i.e message or finalData
 *  
 *  Buglist:
 *  -Sending to many messages will overload the buffer and we will run out of SRAM, this needs to be controlled on the watch side of things
 *  -Need to remove all mentions of string and change to char array so save even more RAM
 *  - if a program fails to send the <f> tag correctly, we will run out of ram as we have already allocated RAM for notifcations
 *
 *  Todo: 
 *    -Use touch capacitive sensors instead of switches to simplify PCB and I think it will be nicer to use.
 *    -Maybe add a micro sd card (might be over kill) or eeprom to store notification text on, just leave the titles in memory as we cant store many in memeory atm
 *  6 is just enough atm hopefully
 *    -Maybe use teensy 3.2, more powerful, with 64k RAM, and most importantly has a RTC built in
 *    -add month and year 
 */

//input vars
boolean button_ok = false;
boolean lastb_ok = false;
boolean button_up = false;
boolean lastb_up = false;
boolean button_down = false;
boolean lastb_down = false;

//serial retrieval vars
String message = "";
String finalData = "";
int chunkCount = 0;
boolean readyToProcess = false;
boolean receiving = false;

//date contants
String PROGMEM months[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

//weather vars
boolean weatherData = false;
String weatherDay = "Sun";
String weatherTemperature = "24 C";
String weatherForecast = "Showers";


//time and date constants
tmElements_t tm;
time_t t;
int PROGMEM clockRadius = 32;
const int PROGMEM clockUpdateInterval = 1000;

//time and date vars
boolean gotUpdatedTime = false;
int clockArray[3] = {0,0,0};
String dateDay = "1";
int dateMonth = 0;
long prevMillis = 1;

//notification data structure
typedef struct{
  char packageName[15];
  char title[15];
  char text[150];
} Notification;

//notification vars
int notificationIndex = 0;
int PROGMEM notificationMax = 10;
Notification notifications[8];

boolean shouldRemove = false;

//pin constants
const int PROGMEM OK_BUTTON = 5;
const int PROGMEM DOWN_BUTTON = 3;
const int PROGMEM UP_BUTTON = 4;
const int PROGMEM BATT_READ = A6;
const int PROGMEM VIBRATE_PIN = 10;

//navigation constants
const int PROGMEM CLOCK_PAGE = 0;
const int PROGMEM NOTIFICATION_MENU = 1;
const int PROGMEM NOTIFICATION_BIG = 2;
const int PROGMEM MAX_PAGES = 2;
const int PROGMEM MENU_ITEM_HEIGHT = 16;
const int PROGMEM FONT_HEIGHT = 12; //need to add this to the y for all DrawStr functions  

//navigation vars
int pageIndex = 0;
int menuSelector = 0;

const int x = 6;
int y = 0; 
int Y_OFFSET = 0;

int lineCount = 0;
int currentLine = 0;

//batt monitoring
float batteryVoltage = 0;

//connection
boolean isConnected = false;


//icons

const byte PROGMEM BLUETOOTH_CONNECTED[] = { 
   0x31, 0x52, 0x94, 0x58, 0x38, 0x38, 0x58, 0x94, 0x52, 0x31
};

void setup(void) {
  Serial.begin(9600);
  HWSERIAL.begin(9600);
  pinMode(OK_BUTTON,INPUT);
  pinMode(DOWN_BUTTON,INPUT);
  pinMode(UP_BUTTON,INPUT);
  pinMode(BATT_READ,INPUT);
  pinMode(VIBRATE_PIN,OUTPUT);
  RTC.haltRTC(false);
  RTC.writeEN(false);
  u8g_prepare();

  //setup batt read pin
  analogReference(DEFAULT);
  analogReadResolution(10);// 2^10 = 1024
  analogReadAveraging(32);//smoothing

  /*
   * Could at HWSERIAL.print("AT"); to cut the connection so we have to reconnect if the watch crashes
   */
   HWSERIAL.print("AT");
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


  float hours = ((hour * 30)* (PI/180));
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

  //notification indicator
  String toString = String(notificationIndex);
  u8g_DrawStr(&u8g,122,3+FONT_HEIGHT,toString.c_str());
  u8g_DrawCircle(&u8g,126,11,10,U8G_DRAW_ALL);

  

  if(weatherData){
    //change fonts
    u8g_SetFont(&u8g, u8g_font_7x14);
    u8g_DrawStr(&u8g,68,5+FONT_HEIGHT,weatherDay.c_str());
    u8g_DrawStr(&u8g,68,20+FONT_HEIGHT,weatherTemperature.c_str());

    u8g_SetFont(&u8g, u8g_font_6x12);
    u8g_DrawStr(&u8g,68,30+FONT_HEIGHT,weatherForecast.c_str());
  } else {
    //display date from RTC
    u8g_SetFont(&u8g, u8g_font_7x14);
    u8g_DrawStr(&u8g,75,10+FONT_HEIGHT,dateDay.c_str());
    u8g_DrawStr(&u8g,75,24+FONT_HEIGHT,months[dateMonth - 1].c_str());
    u8g_SetFont(&u8g, u8g_font_6x12);
  }

  if(isConnected){
    u8g_DrawXBMP(&u8g,100,3,8,10,BLUETOOTH_CONNECTED);
  } else {
    u8g_DrawStr(&u8g,100,15,"NC");
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
    if(tm.Day< 10){
      dateDay = "0" + String(tm.Day);
    } else {
      dateDay = String(tm.Day);
    }
    dateMonth = tm.Month;

    // update battery stuff
    batteryVoltage = getBatteryVoltage();
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
    delay(500);
    digitalWrite(VIBRATE_PIN,HIGH);
    delay(500);
    digitalWrite(VIBRATE_PIN,LOW);
}

void handleMenuInput(){
  button_down = digitalRead(DOWN_BUTTON);
  button_up = digitalRead(UP_BUTTON);
  button_ok = digitalRead(OK_BUTTON);
  
  if (button_up != lastb_up) {
    if (button_up == HIGH) {
      menuSelector++;
      //check here if we need scroll up to get the next items on the screen//check here if we nmeed to scroll down to get the next items
      if((menuSelector >= 4) && (((notificationIndex + 1) - menuSelector) > 0)){//0,1,2,3 = 4 items
        //shift the y down
        Y_OFFSET -= MENU_ITEM_HEIGHT;
      }
      if(menuSelector >= notificationIndex){
         menuSelector = notificationIndex; 
      }
      
    }
    lastb_up = button_up;
  }
  
  if (button_down != lastb_down) {
    if (button_down == HIGH) {
      menuSelector--;
      if(menuSelector < 0){
         menuSelector = 0; 
      }
      //plus y
      if((menuSelector >= 3)){
        Y_OFFSET += MENU_ITEM_HEIGHT;
      }
      
    }
    lastb_down = button_down;
  } 
  
  if (button_ok != lastb_ok) {
    if (button_ok == HIGH) {
      if(menuSelector != notificationIndex){//last one is the back item
        pageIndex = NOTIFICATION_BIG;
      } else {
        menuSelector = 0;//reset the selector
        Y_OFFSET = 0;//reset any scrolling done
        pageIndex = CLOCK_PAGE;// go back to list of notifications
        
      }
    }
    lastb_ok = button_ok;
  }
   
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

void handleNotificationInput(){
  button_ok = digitalRead(OK_BUTTON);
  button_down = digitalRead(DOWN_BUTTON);
  button_up = digitalRead(UP_BUTTON);
  
  if (button_ok != lastb_ok) {
    if (button_ok == HIGH) {
      //sets flag for removal of this notification
      shouldRemove = true;
      Y_OFFSET = 0;
      lineCount = 0;//reset number of lines
      currentLine = 0;// reset currentLine back to zero
      pageIndex = NOTIFICATION_MENU;
    }
    lastb_ok = button_ok;
  }

  if (button_up != lastb_up) {
    if (button_up == HIGH) {
      if( (lineCount - currentLine) >= 6){
        //this scrolls down
        Y_OFFSET -= FONT_HEIGHT;
        currentLine++;
      }
    }
    lastb_up = button_up;
  }
  
  if (button_down != lastb_down) {
    if (button_down == HIGH) {
      if(currentLine > 0){
        //scrolls back up
        Y_OFFSET += FONT_HEIGHT;
        currentLine--;
      }
    }
    lastb_down = button_down;
  } 
}

void loop(void) {
  u8g_FirstPage(&u8g);  
  do {
    switch(pageIndex){
      case 0: drawClock(clockArray[0],clockArray[1],clockArray[2]); break;
      case 1: showNotifications(); break;
      case 2: fullNotification(menuSelector); break;
    }
  } while( u8g_NextPage(&u8g) );
    switch(pageIndex){
      case 0: clockInput(); break;
      case 1: handleMenuInput(); break;
      case 2: handleNotificationInput(); break;
    }
    while(HWSERIAL.available())
    {
      byte incoming = HWSERIAL.read();
      message+=char(incoming);//store string from serial command
      delay(1);
    }
    if(!HWSERIAL.available()){
      if(message!=""){
        Serial.print("Message: ");
        Serial.println(message);
        if(message=="OK+CONN"){
          isConnected = true;
          Serial.println(F("BLUETOOTH IS CONNECTED."));
          message="";
        } else if(message=="OK+LOST") {
          isConnected = false;
          Serial.println(F("BLUETOOTH IS NOT CONNECTED."));
          message="";
        }
        if(!receiving && message.startsWith("<")){
          receiving = true;
        } else if(receiving && (message=="<n>" || message=="<d>" || message=="<w>")) {
          Serial.println("Message data missing, ignoring.");
          //we never recieved the end tag of a previous message
          //reset vars
          receiving = false;
          resetTransmissionData();
        }
        if(message!="<f>"){// need to check if another start tag is used aswell as a message could miss the <f> tag
          if(message.startsWith("<i>")){
            message.remove(0,3);
            chunkCount++;
            finalData+=message;
          } else {
            finalData+=message;
          }
          
        } else {
          receiving = false;
          readyToProcess = true;
        }
    } else {
       //we are not recieving from the bluetooth module
       updateSystem();
    }
    //Reset the message variable
    message="";
  }
  
  if(readyToProcess){
    Serial.println("Received: "+finalData);
    if(finalData.startsWith("<n>")){
          if(!(notificationIndex >= notificationMax)){
            getNotification(finalData);
          } else {
            Serial.println("Hit max notifications, need to pop off some");
            resetTransmissionData();
          }
          alert();
    }else if(finalData.startsWith("<d>")){  
      if(!gotUpdatedTime){
        getTimeFromDevice(finalData); 
      }
    } else if(finalData.startsWith("<w>")){
      getWeatherData(finalData);
      weatherData = true;
    } else {
      Serial.println("Received data with unknown tag");
    }
    //reset vars
    resetTransmissionData();
    Serial.print("Free RAM:");
    Serial.println(FreeRam());
  }
}

void getWeatherData(String finalData){
  Serial.print("Weather data: ");
  Serial.println(finalData);
  finalData.remove(0,3); //remove the tag
  String temp[3];
  int index = 0;
  for(int i=0; i < finalData.length();i++){
    char c = finalData.charAt(i);
    if(c=='<'){
      //split the t and more the next item
      finalData.remove(i,2);
      index++;
    } else {
      temp[index]+= c;
    } 
  }
  weatherDay = temp[0];
  weatherTemperature = temp[1] + " C";
  weatherForecast = temp[2];
  Serial.println("Day: "+weatherDay);
  Serial.println("Temperature: "+weatherTemperature);
  Serial.println("Forecast: "+weatherForecast);
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
  for(int i=0; i < notificationIndex + 1;i++){
    int startY = 0;
    if(i==menuSelector){
        u8g_DrawStr(&u8g,0,y+Y_OFFSET+FONT_HEIGHT,">");
    }
    if(i!=notificationIndex){
      u8g_DrawStr(&u8g,x+3,y+Y_OFFSET+FONT_HEIGHT,notifications[i].title);
    } else {
      u8g_DrawStr(&u8g,x + 3,y + Y_OFFSET+FONT_HEIGHT, "Back");
    }
    y += MENU_ITEM_HEIGHT;
    u8g_DrawFrame(&u8g,x,startY,128,y +Y_OFFSET);
  }
  y = 0;
}

void clockInput(){
  button_ok = digitalRead(OK_BUTTON);
  
  if (button_ok != lastb_ok) {
    if (button_ok == HIGH) {
      pageIndex++;
      if(pageIndex > MAX_PAGES){
         pageIndex = 0; 
      }
      Y_OFFSET = 0;
    }
    lastb_ok = button_ok;
  } 
  
}


void getNotification(String notificationItem){
  //split the <n>
  notificationItem.remove(0,3);
  String temp[3];
  int index = 0;
  for(int i=0; i < notificationItem.length();i++){
    char c = notificationItem.charAt(i);
    if(c=='<'){
      //split the i and more the next item
      notificationItem.remove(i,2);
      index++;
    } else {
      temp[index]+= c;
    } 
  }
  temp[0].substring(0,15);//make sure it fits
  temp[1].substring(0,15);//make sure it fits
  for(int j=0; j < 15;j++){
    notifications[notificationIndex].packageName[j] = temp[0].charAt(j);
    notifications[notificationIndex].title[j] = temp[1].charAt(j);
  }
  temp[2].substring(0,150);//make sure it fits
  for(int k=0; k < 150;k++){
     notifications[notificationIndex].text[k] = temp[2].charAt(k);
  }
  Serial.print("Notification title: ");
  Serial.println(notifications[notificationIndex].title);
  Serial.print("Notification text: ");
  Serial.println(notifications[notificationIndex].text);
  notificationIndex++;
}

void resetTransmissionData(){
  //reset used variables
  readyToProcess = false;
  chunkCount = 0;
  finalData = "";
}


void getTimeFromDevice(String message){
  Serial.print("Date data: ");
  Serial.println(message);
  message.remove(0,3);
  String dateNtime[2] = {"",""};
  int spaceIndex = 0;
     boolean after = true;
     for(int i = 0; i< message.length();i++){
      if(after){
        //todo: need to get date data here, first format the date coming from the smartphone so that the month in a int
       if(message.charAt(i)==' '){
          spaceIndex++;
          if(spaceIndex >= 3){
            after = false;
          } 
       } else {
        dateNtime[0] += message.charAt(i);
       }
      } else {
        dateNtime[1] += message.charAt(i);
      }
      
     }
     int clockIndex = 0;
     String temp = "";
     for(int j=0; j < dateNtime[1].length();j++){
        if(dateNtime[1].charAt(j)==':'){
          clockIndex++;
          temp="";
        } else {
          temp += dateNtime[1].charAt(j);
          clockArray[clockIndex] = temp.toInt();
        }
     }
     //Read from the RTC
     RTC.read(tm);
     //Compare to time from Device(only minutes and hours doesn't have to be perfect)
     if(!(tm.Hour == clockArray[0] && tm.Minute== clockArray[1])){
        setClockTime(clockArray[0],clockArray[1],clockArray[2]);
        Serial.println(F("Setting the clock!"));
     } else {
        //if it's correct we do not have to set the RTC and we just keep using the RTC's time
        Serial.println(F("Clock is correct already!"));
        gotUpdatedTime = true;
     }
}


void setClockTime(int hours,int minutes,int seconds){// no need for seconds
  tm.Hour = hours;
  tm.Minute = minutes;
  tm.Second = seconds;
  t = makeTime(tm);
  if(RTC.set(t) == 0) { // Success
    setTime(t);
    Serial.println(F("Writing time to RTC was successfull!"));
    gotUpdatedTime= true;
  } else {
    Serial.println(F("Writing to clock failed!"));
  }
}


