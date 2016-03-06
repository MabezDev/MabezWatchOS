#include "U8glib.h"
#include <Time.h>
#include <DS1302RTC.h>


U8GLIB_SH1106_128X64 u8g(U8G_I2C_OPT_DEV_0|U8G_I2C_OPT_FAST); // Dev 0, Fast I2C / TWI
DS1302RTC RTC(12,11,13);


String message;
String toDisplay;
int clockRadius = 32;
int clockArray[3] = {0,0,0};
tmElements_t tm;
time_t t;
boolean gotUpdatedTime = false;
int notificationIndex = -1;
int itemsIndex = 0;

typedef struct{
  String packageName;
  String title;
  String text;
} Notification;

Notification notifications[10]; //current max of 10 notifications

int clockUpdateInterval = 1000;
long prevMillis = 0;

void u8g_prepare(void) {
  u8g.setFont(u8g_font_6x10);
  u8g.setFontRefHeightExtendedText();
  u8g.setDefaultForegroundColor();
  u8g.setFontPosTop();
}



void draw() {
  u8g_prepare();
  drawClock(clockArray[0],clockArray[1],clockArray[2]);
}

void drawClock(int hour, int minute,int second){
  u8g.drawFrame(28,0,72,64);
  u8g.drawLine(64,0,64,5);//top notch
  u8g.drawStr(59,5,"12");
  u8g.drawLine(64,64,64,59);//bottom notch
  u8g.drawLine(28,32,33,32);//left notch
  u8g.drawLine(99,32,94,32);//right notch

  float hours = ((hour * 30)* (PI/180));
  int x2 = 64 + (sin(hours) * (clockRadius/2));
  int y2 = 32 - (cos(hours) * (clockRadius/2));
  u8g.drawLine(64,32,x2,y2); //hour hand

  float minutes = ((minute * 6) * (PI/180));
  int xx2 = 64 + (sin(minutes) * (clockRadius/1.2));
  int yy2 = 32 - (cos(minutes) * (clockRadius/1.2));
  u8g.drawLine(64,32,xx2,yy2);//minute hand

  float seconds = ((second * 6) * (PI/180));
  int xxx2 = 64 + (sin(seconds) * (clockRadius/1.2));
  int yyy2 = 32 - (cos(seconds) * (clockRadius/1.2));
  u8g.drawLine(64,32,xxx2,yyy2);//second hand
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
  //u8g.setRot180();
  Serial.begin(9600);
  #if defined(ARDUINO)
    pinMode(13, OUTPUT);           
    digitalWrite(13, HIGH);  
  #endif
  toDisplay ="";
  RTC.haltRTC(false);
  RTC.writeEN(false);
  
}

void loop(void) {
  
  // picture loop  
  u8g.firstPage();  
  do {
    //all display stuff is done in this loop
    draw();
  } while( u8g.nextPage() );
  //logic is done here
    while(Serial.available())
    {
      message+=char(Serial.read());//store string from serial command
      delay(1);// very important
    }

    if(!Serial.available())
    {
    if(message!=""){
      Serial.println("Message: "+message);
      /*
       * Once we have the message we can take its tag i.e time or a notification etc and deal with it appropriatly from here.
       */
      if(message.startsWith("<n>")){
        notificationIndex++;
        Serial.println("Notification Received.");
        getNotification(message);
        delay(500);  
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

void getNotification(String notificationItem){
  //split the <i>
  notificationItem.remove(0,3);
  String temp[3];
  int index = 0;
  for(int i=0; i < notificationItem.length();i++){
    char c = notificationItem.charAt(i);
    if(c=='<'){
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
}

void printNotifications(){
  u8g.setPrintPos(64,32);
    for(int i=0; i < sizeof(notifications);i++){
      Serial.println("A notification was stored.");
 }
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
        Serial.println("Setting the clock!");
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
    Serial.println("Writing time to RTC was successfull!");
    gotUpdatedTime= true;
  } else {
    Serial.println("Writing to clock failed!");
  }
}












