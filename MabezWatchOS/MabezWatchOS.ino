#include <u8g_i2c.h>
#include <Arduino.h>
#include <i2c_t3.h>
#include <Time.h>
#include <DS1302RTC.h>

#define HWSERIAL Serial1
u8g_t u8g;
DS1302RTC RTC(12,11,13);

/*
 * This has now been fully updated to the teensy 
 * Todo: 
 * Change transmission algorithm to catch failed sendings and to break up big messages to avoid packet loss
 */

boolean button_ok = false;
boolean lastb_ok = false;
boolean button_up = false;
boolean lastb_up = false;
boolean button_down = false;
boolean lastb_down = false;

String message = "";
String finalData = "";
int chunkCount = 0;
boolean readyToProcess = false;

int clockRadius = 32;
int clockArray[3] = {0,0,0};

tmElements_t tm;
time_t t;

boolean gotUpdatedTime = false;
boolean hasNotifications = false;

int notificationIndex = 0;
int pageIndex = 0;
int menuSelector = 0;

int lineCount = 0;
int currentLine = 0;

const int x = 6;
int y = 0; 

typedef struct{
  String packageName;
  String title;
  String text;
} Notification;

extern unsigned long _estack; 

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

const int FONT_HEIGHT = 12; //need to add this to the y for all DrawStr functions  

void u8g_prepare(void) {
  u8g_InitComFn(&u8g, &u8g_dev_sh1106_128x64_2x_i2c, u8g_com_hw_i2c_fn);
  u8g_SetFont(&u8g, u8g_font_6x12);
}

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

  String toString = String(notificationIndex);
  char number[toString.length()];
  stringToChar(toString,number);
  u8g_DrawStr(&u8g,120,8+FONT_HEIGHT,number);
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
        //todo: remove the notification once read
        menuSelector = 0;//rest the selector
        Y_OFFSET = 0;//reset any scrolling done
        pageIndex = CLOCK_PAGE;// go back to list of notifications
        
      }
    }
    lastb_ok = button_ok;
  }
   
}

//need to implment this but its too much to bother with right now
void fullNotification(int chosenNotification){
  int charsThatFit = 20;
  int charIndex = 0;
  int lines = 0;
 
  char text[notifications[chosenNotification].text.length()];
  stringToChar(notifications[chosenNotification].text,text);

  String linesArray[(notifications[chosenNotification].text.length()/charsThatFit)+2];
 
  if(notifications[chosenNotification].text.length() > charsThatFit){
    for(int i=0; i< notifications[chosenNotification].text.length(); i++){
      if(charIndex > charsThatFit){
        linesArray[lines] += text[i];
        lines++;
        charIndex = 0;
      } else {
        linesArray[lines] += text[i];
        charIndex++;
      }
    }
    for(int j=0; j < lines + 1; j++){
      char lineToDraw[32] = {' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
      ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '};//should make this 32 to fill a whole row
      stringToChar(linesArray[j],lineToDraw);
      u8g_DrawStr(&u8g,0,j * 10 +FONT_HEIGHT + Y_OFFSET,lineToDraw);
    }
    lineCount = lines;
  } else {
    u8g_DrawStr(&u8g,0,FONT_HEIGHT,text);
  }
}

void handleNotificationInput(){
  button_ok = digitalRead(OK_BUTTON);
  button_down = digitalRead(DOWN_BUTTON);
  button_up = digitalRead(UP_BUTTON);
  
  if (button_ok != lastb_ok) {
    if (button_ok == HIGH) {
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
    if(!HWSERIAL.available())
    {
    if(message!=""){
        if(message!="<f>"){
          if(message.startsWith("<i>")){
            message.remove(0,3);
            chunkCount++;
            finalData+=message;
          } else {
            finalData+=message;
          }
          
        } else {
          readyToProcess = true;
        }
      } else {
        //we have no connection to phone use the time from the RTC
         updateClock();
      }
      //Reset the message variable
      message="";
  }

  /*
   * Once we have the message we can take its tag i.e time or a notification etc and deal with it appropriatly from here.
   */
  if(readyToProcess){
    // add a clause here to reset all variables if data data start is not recognized.
    if(finalData.startsWith("<n>")){
          getNotification(finalData);
    }else if(finalData.startsWith("<d>")){
      if(!gotUpdatedTime){
        getTimeFromDevice(message);   
      }
    } else {
      Serial.println("Received data with unknown tag");
      //reset used variables
      readyToProcess = false;
      chunkCount = 0;
      finalData = "";
    }
  }
}

uint32_t FreeRam() { // for Teensy 3.0
    uint32_t heapTop;
    void* foo = malloc(1);
    heapTop = (uint32_t) foo;
    free(foo);
    return (unsigned long)&_estack - heapTop;
}

void showNotifications(){
  for(int i=0; i < notificationIndex + 1;i++){
    int startY = 0;
    if(i==menuSelector){
        u8g_DrawStr(&u8g,0,y+Y_OFFSET+FONT_HEIGHT,">");
    }
    if(i!=notificationIndex){
      char title[notifications[i].title.length()];
      stringToChar(notifications[i].title,title);
      u8g_DrawStr(&u8g,x+3,y+Y_OFFSET+FONT_HEIGHT,title);//string to char only printing the first character, need to fix
    } else {
      u8g_DrawStr(&u8g,x + 3,y + Y_OFFSET+FONT_HEIGHT, "Back");
    }
    y += MENU_ITEM_HEIGHT;
    u8g_DrawFrame(&u8g,x,startY,128,y +Y_OFFSET);
  }
  y = 0;
}

void stringToChar(String s, char* buf){
  for(int i = 0; i < s.length();i++){
    buf[i] = (char) s.charAt(i);
  }
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
  //add the text tot he current notification
  notifications[notificationIndex].packageName = temp[0];
  notifications[notificationIndex].title = temp[1];
  notifications[notificationIndex].text = temp[2];
  Serial.println("Notification title: "+notifications[notificationIndex].title);
  Serial.println("Notification text: "+notifications[notificationIndex].text);
  notificationIndex++;

  //reset used variables
  readyToProcess = false;
  chunkCount = 0;
  finalData = "";
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












