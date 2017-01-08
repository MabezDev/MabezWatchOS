#include <itoa.h>

#define HWSERIAL Serial2 //change back to Serial2 when we use bluetooth // PA2 & PA3


const short MAX_DATA_LENGTH = 500; // sum off all bytes of the notification struct with 50 bytes left for message tags i.e <n>
char payload[300]; // serial read buffer for data segments(payloads)
char data[MAX_DATA_LENGTH];//data set buffer
short dataIndex = 0; //index is required as we dunno when we stop
bool transmissionSuccess = false;
bool receiving = false;
short totalChecksum = 0;
short expectedChecksum = 0;
short payloadChecksum = 0;
char type;

//weather

bool weatherData = false;
char weatherDay[4];
char weatherTemperature[4];
char weatherForecast[25];
short timeWeGotWeather[2] = {0,0};

// time/date
bool gotUpdatedTime = false;
short clockArray[3] = {0,0,0}; // HH:MM:SS
short dateArray[4] = {0,0,0,0}; // DD/MM/YYYY, last is Day of Week


// notification stuff
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

const short MSG_SIZE[3] = {25,200,750};
short textIndexes[3] = {0,0,0};

char SmallText[50][25];
char NormalText[25][200];
char LargeText[5][750];

void *types[3] = {SmallText,NormalText,LargeText}; // store a pointer of each matrix, later to be casted to a char *

//notification vars
short notificationIndex = 0;
const short notificationMax = 30; // can increase this or increase the text size as we have 20kb RAM on the STM32

Notification notifications[notificationMax]; 

/*
 *    ToDo:
 *  - Add a time out, if we dont recieve the data withing a timeframe then discard the packet or send fail or something
 */

void setup() {
  Serial.begin(9600);  //for debug
  HWSERIAL.begin(9600);
}

//<*>n55 - this will expect a notification of length 55, the length og th packet below
//<t>Test Title<t>Test text that will be displayed here.*

//large test
//<*>n763
//Contrary to popular belief, Lorem Ipsum is not simply random text. It has roots in a piece of classical Latin literature from 45 BC, making it over 2000 years old. Richard McClintock, a Latin professor at Hampden-Sydney College in Virginia, looked up one of the more obscure Latin words, consectetur, from a Lorem Ipsum passage, and going through the cites of the word in classical literature, discovered the undoubtable source. Lorem Ipsum comes from sections 1.10.32 and 1.10.33 of "de Finibus Bonorum et Malorum" (The Extremes of Good and Evil) by Cicero, written in 45 BC. This book is a treatise on the theory of ethics, very popular during the Renaissance. The first line of Lorem Ipsum, "Lorem ipsum dolor sit amet..", comes from a line in section 1.10.32.

void loop() {
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
      delay(5); // put back to 1 delay when back in the OS
  }  
  if(!HWSERIAL.available()){
    if(payloadIndex > 0){ // we have recived something
      Serial.print(F("Message: "));
      Serial.println(payload);
      
      if(startsWith(payload,"<*>",3)){
        if(!receiving){
          Serial.println("Found a new packet initializer.");
          type = payload[3]; // 4th char will always be the same type
          expectedChecksum = getCheckSum(payload,payloadIndex,true);
          Serial.print("expectedChecksum: ");
          Serial.println(expectedChecksum);
          receiving = true;
          HWSERIAL.print("<ACK>"); // send acknowledge packet, then app will send the contents to the watch
        } else {
          Serial.println("Recived a new packet init when we weren't expecting one. Resetting all transmission variabels for new packet.");
          completeReset(true);  // tell the app we weren't expecting a packet, so restart completely
        }
      } else if(startsWith(payload,"<!>",3)){
        Serial.println("<!> detected! Reseeting transmission vars.");
        completeReset(false);
      } else if(startsWith(payload,"<+>",3)){
        payloadChecksum = getCheckSum(payload,payloadIndex,false);
        Serial.print("Found new payload with checksum: ");
        Serial.println(payloadChecksum);
        HWSERIAL.print("<ACK>");
      } else {
        if(receiving){
          // check if payload is correct
          Serial.print("payloadIndex : ");
          Serial.print(payloadIndex);
          Serial.print(", payloadChecksum : ");
          Serial.println(payloadChecksum);
          if(payloadIndex == payloadChecksum){
            // now we just add the text to the data till we reach the payloadCount length
            if(dataIndex + payloadIndex < MAX_DATA_LENGTH){
              for(int j = 0; j < payloadIndex; j++){ // add the payload to the final data
                data[dataIndex] = payload[j];
                dataIndex++;
              }
            } else {
              Serial.println("Error! Final data is full!");
            }
            totalChecksum += payloadIndex;
            // send OK
            HWSERIAL.print("<OK>");
          } else {
            HWSERIAL.println("<FAIL>");
          }
          Serial.print("totalChecksum : ");
          Serial.print(totalChecksum);
          Serial.print(", expectedChecksum : ");
          Serial.println(expectedChecksum);
          if(totalChecksum == (expectedChecksum - 1)){
            Serial.println("End of stream, send to function now");
            totalChecksum = 0;
            receiving  = false;
            transmissionSuccess = true;
          }
          
        }
      }
      
    }
  }

  if(transmissionSuccess){
    switch(type){
      case 'n':
        Serial.println("Notification data processed!");
        getNotification(data, dataIndex);
        break;
      case 'w':
        Serial.println("Weather data processed!");
        getWeatherData(data,dataIndex);
        break;
      case 'd':
        Serial.println("Date/Time data processed!");
        getDateTime(data,dataIndex);
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

}

short getCheckSum(char initPayload[], short pIndex, bool startPacket){
      uint8_t startPos = startPacket ? 4 : 3;
      uint8_t numChars = pIndex - startPos;
      char payloadCountChars[numChars];
      for(int i = 0; i < numChars; i++){
        payloadCountChars[i] = initPayload[i+startPos];
      }
      return atoi(payloadCountChars);
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
        //notifications[notificationIndex].textType = determineType(notifications[notificationIndex].textLength);
        //addTextToNotification(&notifications[notificationIndex],notPtr,notifications[notificationIndex].textLength);
        break;
      }
    }
    notPtr++;
    textCount++; // used to calculate the final text size by subtracting from the length of the packet
  }
  
//  //finally get the timestamp of whenwe recieved the notification
//  notifications[notificationIndex].dateReceived[0] = clockArray[0];
//  notifications[notificationIndex].dateReceived[1] = clockArray[1];
//
  Serial.print(F("Notification title: "));
  Serial.println(notifications[notificationIndex].title);
//  Serial.print(F("Notification text: "));
//  Serial.println(notifications[notificationIndex].textPointer);
  Serial.print("Text length: ");
  Serial.println(notifications[notificationIndex].textLength);

  // dont forget to increment the index
  notificationIndex++;
  Serial.print("Number of notifications: ");
  Serial.println(notificationIndex);
}

void getDateTime(char message[], short len){
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

//  Serial.print("Date: ");
//  for(int q = 0; q < 3; q++){
//    Serial.print(dateArray[q]);
//    Serial.print("/");
//  }
//  Serial.println();
//
//  Serial.print("Time: ");
//  for(int t = 0; t < 3; t++){
//    Serial.print(clockArray[t]);
//    Serial.print(":");
//  }
//  Serial.println();
  
     
//     //Read from the RTC
//     breakTime(rt.getTime(),tm);
//     //Compare to time from Device(only minutes and hours doesn't have to be perfect)
//     if(!((tm.Hour == clockArray[0] && tm.Minute == clockArray[1] && dateArray[0] == tm.Day && dateArray[1] == tm.Month && dateArray[2] == tm.Year))){
//        setClockTime(clockArray[0],clockArray[1],clockArray[2],dateArray[0],dateArray[1],dateArray[2]);
//        Serial.println(F("Setting the clock!"));
//     } else {
//        //if it's correct we do not have to set the RTC and we just keep using the RTC's time
//        Serial.println(F("Clock is correct already!"));
//        gotUpdatedTime = true;
//     }

}




bool startsWith(char data[], char charSeq[], short len){
    for(short i=0; i < len; i++){
      if(!(data[i]==charSeq[i])){
        return false;
      }
    }
    return true;
}










