#include <u8g_i2c.h>
#include <Arduino.h>
#include <i2c_t3.h>
#include <Time.h>
#include <DS1302RTC.h>

#define HWSERIAL Serial1
u8g_t u8g;
DS1302RTC RTC(12,11,13);

/*
 * 
 * SERIAL COMMUNICATIONS TILL WORKS ALTHOUGH NEED TO SWITCH FROM SOFT SER
 * need to rewrite display using layout u8g_Draw... function as this is the only way the display will work
 * As i do not have a knowledge to correctly implement this arm code into the lib
 * 
 * update -- started rewrite of code need to implment proper ways to get char array etc, might have a power problem need to charge batt/ use the new ones
 * REST NEEDS TO BE TESTED
 * 
 * 
 */

boolean button_ok = false;
boolean lastb_ok = false;
boolean button_up = false;
boolean lastb_up = false;
boolean button_down = false;
boolean lastb_down = false;

String message = "";

char command[256];  
char commandBuffer[128];  
int commandBufferSize = 0;

int clockRadius = 32;
int clockArray[3] = {0,0,0};

tmElements_t tm;
time_t t;

boolean gotUpdatedTime = false;
boolean hasNotifications = false;

int notificationIndex = 0;
int pageIndex = 0;
int menuSelector = 0;

const int x = 6;
int y = 0; // needed cuz the font starts fromt he top now 

typedef struct{
  String packageName;
  String title;
  String text;
} Notification;

Notification notifications[10]; //current max of 10 notifications

int clockUpdateInterval = 1000;
long prevMillis = 1;

const int OK_BUTTON = 5;
const int DOWN_BUTTON = 3;
const int UP_BUTTON = 4;
const int CLOCK_PAGE = 0;
const int NOTIFICATION_MENU = 1;
const int NOTIFICATION_BIG = 2;
const int MAX_PAGES = 2;
const int MENU_ITEM_HEIGHT = 16;

int Y_OFFSET = 0;
//need to add this to the y for all DrawStr functions
const int FONT_HEIGHT = 12;

void u8g_prepare(void) {
  u8g_InitComFn(&u8g, &u8g_dev_sh1106_128x64_2x_i2c, u8g_com_hw_i2c_fn);
  u8g_SetFont(&u8g, u8g_font_6x12);
}

/*
 * RESEARCH:
 * Might need to change HC-06 baud to something different than 9600 as it may be sending reste signals to the arduino (use soft serial?)
 * UPDATE:
 * THIS FIXED IT, working perfectly. Now need to got back to working on app to filter notifications.
 * 
 * TO DO:
 * generate a structure for representing notifications
 */


void drawClock(int hour, int minute,int second){
  u8g_DrawFrame(&u8g,28,0,72,64);
  u8g_DrawLine(&u8g,64,0,64,5);//top notch
  u8g_DrawStr(&u8g,59,5+FONT_HEIGHT,"12");
  u8g_DrawLine(&u8g,64,64,64,59);//bottom notch
  u8g_DrawLine(&u8g,28,32,33,32);//left notch
  u8g_DrawLine(&u8g,99,32,94,32);//right notch

  float hours = ((hour * 30)* (PI/180));
  int x2 = 64 + (sin(hours) * (clockRadius/2));
  int y2 = 32 - (cos(hours) * (clockRadius/2));
  u8g_DrawLine(&u8g,64,32,x2,y2); //hour hand

  float minutes = ((minute * 6) * (PI/180));
  int xx2 = 64 + (sin(minutes) * (clockRadius/1.2));
  int yy2 = 32 - (cos(minutes) * (clockRadius/1.2));
  u8g_DrawLine(&u8g,64,32,xx2,yy2);//minute hand

  float seconds = ((second * 6) * (PI/180));
  int xxx2 = 64 + (sin(seconds) * (clockRadius/1.2));
  int yyy2 = 32 - (cos(seconds) * (clockRadius/1.2));
  u8g_DrawLine(&u8g,64,32,xxx2,yyy2);//second hand
  
  u8g_DrawStr(&u8g,120,8+FONT_HEIGHT,intToChar(notificationIndex));
}

void updateClock(){
  long current = millis();
  if(current - prevMillis >= clockUpdateInterval){
    prevMillis = current;
    RTC.read(tm);
    clockArray[0] = tm.Hour;
    clockArray[1] = tm.Minute;
    clockArray[2] = tm.Second;
  }
}

void setup(void) {

  // flip screen, if required
  //u8g_setRot180();
  Serial.begin(9600);
  HWSERIAL.begin(9600);
  #if defined(ARDUINO)
    pinMode(13, OUTPUT);           
    digitalWrite(13, HIGH);  
  #endif
  pinMode(OK_BUTTON,INPUT);
  pinMode(DOWN_BUTTON,INPUT);
  pinMode(UP_BUTTON,INPUT);
  RTC.haltRTC(false);
  RTC.writeEN(false);
  u8g_prepare();
  
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
        Serial.println("Scrolling Down 1 Item!");
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
        //remove the notification
        menuSelector = 0;//rest the selector
        pageIndex = CLOCK_PAGE;// go back to list of notifications
      }
    }
    lastb_ok = button_ok;
  }
   
}

//need to implment this but its too much to bother with right now
/*void fullNotification(int chosenNotification){
  int charsThatFit = 20;
  int charIndex = 0;
  int lines = 0;
  String textInput = notifications[chosenNotification].text;
  char text[textInput.length()];
  textInput.toCharArray(text,textInput.length()); 
 
  if(text.length() > charsThatFit){
    for(int i=0; i< text.length(); i++){
      if(charIndex > charsThatFit){
        lines++;
        charIndex = 0;
        u8g_DrawStr(&u8g,0,lines * 10,text[i]);//print the char we miss
      } else {
        u8g_DrawStr(&u8g,0,lines * 10,text[i]);//print each char till it wont fit
        charIndex++;
      }
    }
    //print line number here
  } else {
    u8g_DrawStr(&u8g,0,0,text);
  }
  
  // need to work oput what fits on each line
  //store ther line number so we know when to stop scrolling
  //format it nicely
  //work out how to delete the notification 
}*/

void handleNotificationInput(){
  button_ok = digitalRead(OK_BUTTON);
  
  if (button_ok != lastb_ok) {
    if (button_ok == HIGH) {
      pageIndex = 1;
    }
    lastb_ok = button_ok;
  }
}

void loop(void) {
  u8g_FirstPage(&u8g);  
  do {
    switch(pageIndex){
      case 0: drawClock(clockArray[0],clockArray[1],clockArray[2]); break;
      case 1: showNotifications(); break;
      //case 2: fullNotification(menuSelector); break;
    }
  } while( u8g_NextPage(&u8g) );
    switch(pageIndex){
      case 0: clockInput(); break;
      case 1: handleMenuInput(); break;
      //case 2: handleNotificationInput(); break;
    }
    
    while(HWSERIAL.available())
    {
      byte incoming = HWSERIAL.read();
      Serial.println("Recived: "+(char) incoming);
      message+=char(incoming);//store string from serial command
      delay(1);
    }
    if(!HWSERIAL.available())
    {
    if(message!=""){
        Serial.println("Message: "+message);
        /*
         * Once we have the message we can take its tag i.e time or a notification etc and deal with it appropriatly from here.
         */
        if(message.startsWith("<n>")){
          if(notificationIndex > 10){
            Serial.println(F("Recieved 10 messages!"));
          }else{
            getNotification(message);
          }
        }else if(message.startsWith("<d>")){
          if(!gotUpdatedTime){
            getTimeFromDevice(message);   
          }
        }
      } else {
        //we have no connection to phone use the time from the RTC
         updateClock();
      }
      //Reset the message variable
      message="";
  }
}

void showNotifications(){
  for(int i=0; i < notificationIndex + 1;i++){
    int startY = 0;//introducing the new u8glib the text is offsetted wrong
    if(i==menuSelector){
        u8g_DrawStr(&u8g,0,y+Y_OFFSET+FONT_HEIGHT,">");
    }
    if(i!=notificationIndex){
     u8g_DrawStr(&u8g,x+3,y+Y_OFFSET+FONT_HEIGHT,stringToChar(notifications[i].title));
    } else {
      u8g_DrawStr(&u8g,x + 3,y + Y_OFFSET+FONT_HEIGHT, "Back");
    }
    y += MENU_ITEM_HEIGHT;
    u8g_DrawFrame(&u8g,x,startY,128,y +Y_OFFSET);
  }
  y = 0;
}

char* stringToChar(String s){
  char buf[s.length()];
  for(int i = 0; i < s.length();i++){
    buf[i] = (char) s.charAt(i);
  }
  return buf;
}

char* intToChar(int i){
  String s = String(i);
  char buf[s.length()];
  for(int i = 0; i < s.length();i++){
    buf[i] = (char) s.charAt(i);
  }
  return buf;
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
  char* temp[3];
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
  //add the text tot he current notification
  notifications[notificationIndex].packageName = temp[0];
  notifications[notificationIndex].title =  temp[1];
  notifications[notificationIndex].text = temp[2];
  Serial.println("Notification title: "+notifications[notificationIndex].title);
  Serial.println("Notification text: "+notifications[notificationIndex].text);
  notificationIndex++;
}


void getTimeFromDevice(String message){
  message.remove(0,3);
  String dateNtime[2] = {"",""};
  int spaceIndex = 0;
     boolean after = true;
     for(int i = 0; i< message.length();i++){
      if(after){
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












