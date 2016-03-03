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

int clockUpdateInterval = 10000;
long prevMillis = 0;

void u8g_prepare(void) {
  u8g.setFont(u8g_font_6x10);
  u8g.setFontRefHeightExtendedText();
  u8g.setDefaultForegroundColor();
  u8g.setFontPosTop();
}



void draw(String text) {
  u8g_prepare();
  u8g.setPrintPos(0, 20); 
  u8g.print(text);
  //updateClock();
  drawClock(clockArray[0],clockArray[1],clockArray[2]);
  /*u8g.setPrintPos(0, 0); 
  u8g.print(clockArray[0]);
  u8g.setPrintPos(0,20);
  u8g.print(clockArray[1]);*/
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
    
  while(Serial.available())
  {//while there is data available on the serial monitor
    message+=char(Serial.read());//store string from serial command
    delay(1);// very important else it cunts the message
  }
  if(!Serial.available())
  {
    if(message!=""){
      Serial.println("Message: "+message);
      if(!gotUpdatedTime){
        String dateNtime[2] = {"",""};
        int spaceIndex = 0;
           boolean after = true;
           for(int i = 0; i< message.length();i++){//NEED TO DEBUG THIS HOLE ALGORITHM TO GET THE RIGHT TIME
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
           RTC.read(tm);
           if(!(tm.Hour == clockArray[0] && tm.Minute== clockArray[1])){
              setClockTime(clockArray[0],clockArray[1]);
              Serial.println("Setting the clock!");
              //for(int z=0; z< sizeof(clockArray);z++){
               // Serial.println(clockArray[z]);
              //}
           } else {
            //rtc works fine
           }
           message="";
           
      }
    } else {
      //we have no connection to phone use the time from the RTC
       updateClock();
    }
  }
  draw(toDisplay);
  } while( u8g.nextPage() );
  
  // increase the state
  
  
  // rebuild the picture after some delay
  //delay(150)
}

void setClockTime(int hours,int minutes){
  tm.Hour = hours;
  tm.Minute = minutes;
  t = makeTime(tm);
  if(RTC.set(t) == 0) { // Success
    setTime(t);
    Serial.println("Writing time to RTC was successfull!");
    gotUpdatedTime= true;
  } else {
    Serial.println("Writing to clock failed!");
  }
}












