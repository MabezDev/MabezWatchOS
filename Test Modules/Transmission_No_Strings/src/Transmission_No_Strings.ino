#include <Arduino.h>

#define HWSERIAL Serial1


//needed for calculating Free RAM on ARM based MC's
#ifdef __arm__
  extern "C" char* sbrk(int incr);
#else  // __ARM__
  extern char *__brkval;
  extern char __bss_end;
#endif  // __arm__



//serial retrieval vars
char message[100];
char *messagePtr; //this could be local to the receiving loop
char finalData[250];
int finalDataIndex = 0; //index is required as we dunno when we stop
int messageIndex = 0;
//int chunkCount = 0;

//weather vars
boolean weatherData = false;
char weatherDay[4];
char weatherTemperature[4];
char weatherForecast[25];

//time and date vars
boolean gotUpdatedTime = false;
int clockArray[3] = {0,0,0};
int dateArray[4] = {0,0,0,0};


boolean readyToProcess = false;
boolean receiving = false;

long previousMillis = 0;

//connection
boolean isConnected = false;
int connectedTime = 0;

//notification data structure
typedef struct{
  char packageName[15];
  char title[15];
  char text[150];
} Notification;

//notification vars
int notificationIndex = 0;
int PROGMEM notificationMax = 8;
Notification notifications[8];


/*
 * This sketch transforms the current String version to the char[] version (using pointers)
 */

void setup() {
  HWSERIAL.begin(9600);
  Serial.begin(9600);
  messagePtr = &message[0]; // messagePtr = message would also work as message just points to the address of the first element anyway

  //force dc on restart
  HWSERIAL.print("AT");
}

void loop() {
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
        //we never recieved the end tag of a previous message
        //reset vars
        Serial.println("Message data missing, ignoring.");
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
      if(notificationIndex < 7){ // 0-7 == 8
        getNotification(finalData,finalDataIndex);
      } else {
        Serial.println("Max notifications Hit.");
      }
    } else if(startsWith(finalData,"<w>",3)){
      getWeatherData(finalData,finalDataIndex);
      weatherData = true;
    } else if(startsWith(finalData,"<d>",3)){
      getTimeFromDevice(finalData,finalDataIndex);
    }
    resetTransmissionVariables();
  }

  long current = millis();
  if((current - previousMillis) > 1000){
    previousMillis = current;
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
  }

}

void resetTransmissionVariables(){
  finalDataIndex = 0; //reset final data
  memset(finalData, 0, sizeof(finalData)); // clears array - (When add this code to the OS we need to add the memsets to the resetTransmission() func)
  readyToProcess = false;
}

int FreeRam() {
  char top;
  #ifdef __arm__
    return &top - reinterpret_cast<char*>(sbrk(0));
  #else  // __arm__
    return __brkval ? &top - __brkval : &top - &__bss_end;
  #endif  // __arm__
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
     boolean gotDate = false;
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

     /* //uncomment this in real program
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
     */
}



//
bool startsWith(char data[], char charSeq[], int len){
    for(int i=0; i < len; i++){
      if(!(data[i]==charSeq[i])){
        return false;
      }
    }
    return true;
}
