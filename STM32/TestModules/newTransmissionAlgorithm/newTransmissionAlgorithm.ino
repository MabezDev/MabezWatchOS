#include <itoa.h>

#define HWSERIAL Serial2 //change back to Serial2 when we use bluetooth // PA2 & PA3


const short MAX_DATA_LENGTH = 500; // sum off all bytes of the notification struct with 50 bytes left for message tags i.e <n>
char payload[300]; // serial read buffer for data segments(payloads)
char data[MAX_DATA_LENGTH];//data set buffer
short dataIndex = 0; //index is required as we dunno when we stop
bool transmissionSuccess = false;
bool receiving = false;
short checkSum = 0;
char type;

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
        Serial.println("Notification data processed!");
        getNotification(data, dataIndex);
        break;
      case 'w':
        Serial.println("Weather data processed!");
        break;
      case 'd':
        Serial.println("Date/Time data processed!");
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
void completeReset(bool sendFail){
  memset(data, 0, sizeof(data)); // wipe final data ready for next notification
  dataIndex = 0; // and reset the index
  receiving  = false;
  transmissionSuccess = false;
  if(sendFail){
    HWSERIAL.print("<FAIL>");
  }
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


boolean isInterval(char* ptr1){
  char* ptr = ptr1;
  Serial.print("Received: ");
  Serial.print(*ptr);
  Serial.print(" ptr++ = ");
  Serial.println(*(ptr++));
  if(*(ptr) == '<' && *(ptr + 1) == 'i' && *(ptr + 2) == '>'){
    return true;
  }
  return false;
}




bool startsWith(char data[], char charSeq[], short len){
    for(short i=0; i < len; i++){
      if(!(data[i]==charSeq[i])){
        return false;
      }
    }
    return true;
}










