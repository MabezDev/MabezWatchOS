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
          completeReset();  // tell the app we weren't expecting a packet, so restart completely
        }
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
              Serial.println();
              Serial.println("End of message, final contents: ");
              Serial.println(data);
              Serial.println();
              receiving  = false;
              transmissionSuccess = true;
            } else {
              // failed the checkSum, tell the watch to resend
              Serial.println("Checksum failed, asking App for resend.");
              completeReset();
            }
          } else if(dataIndex > checkSum){
            // something has gone wrong
            Serial.println("We received more data than we were expecting, asking App for resend.");
            completeReset();
          }
        }
      }
      
    }
  }

  if(transmissionSuccess){
    switch(type){
      case 'n':
        Serial.println("Notification data processed!");
        getNotification(data);
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

//void resetAndResend(){
//  HWSERIAL.print("<FAIL>"); // watch will resend
//  memset(data, 0, sizeof(data)); // wipe final data ready for next notification
//  dataIndex = 0; // and reset the index
//}

void completeReset(){
  memset(data, 0, sizeof(data)); // wipe final data ready for next notification
  dataIndex = 0; // and reset the index
  receiving  = false;
  transmissionSuccess = false;
  HWSERIAL.print("<FAIL>");
}


void getNotification(char text[]){
  Serial.print("Text in notification: ");
  Serial.println(text);
}







bool startsWith(char data[], char charSeq[], short len){
    for(short i=0; i < len; i++){
      if(!(data[i]==charSeq[i])){
        return false;
      }
    }
    return true;
}










