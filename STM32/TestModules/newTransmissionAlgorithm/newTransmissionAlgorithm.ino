#include <itoa.h>

#define HWSERIAL Serial //change back to Serial2 when we use bluetooth // PA2 & PA3


const short MAX_DATA_LENGTH = 500; // sum off all bytes of the notification struct with 50 bytes left for message tags i.e <n>
char payload[100]; // serial read buffer for data segments(payloads)
char data[MAX_DATA_LENGTH];//data set buffer
short dataIndex = 0; //index is required as we dunno when we stop
bool transmissionSuccess = false;
bool receiving = false;
short checkSum = 0;

void setup() {
  Serial.begin(9600);  //for debug
  HWSERIAL.begin(9600);

}

//<*>n54 - this will expect a notification of length 54, the length og th packet below
//<t>Test Title<t>Test text that will be displayed here.

void loop() {
  short payloadIndex = 0;
  while(HWSERIAL.available()){
      payload[payloadIndex] = char(HWSERIAL.read());//store char from serial command
      payloadIndex++;
      if(payloadIndex > 99){
        Serial.println(F("Error message overflow, flushing buffer and discarding message."));
        memset(payload, 0, sizeof(payload)); //resetting array
        break;
      }
      delay(1);
  }  
  if(!HWSERIAL.available()){
    if(payloadIndex > 0){ // we have recived something
      Serial.print(F("Message: "));
      for(short i=0; i < payloadIndex; i++){
        Serial.print(payload[i]);
      }
      Serial.println();


      if(startsWith(payload,"<*>",3)){
        Serial.println("Found a new packet initializer.");
        char type = payload[3]; // 4th char will always be the same type
        Serial.print("Type: ");
        Serial.println(type);
        uint8_t numChars = payloadIndex - 4;
        char checkSumChars[numChars];
        for(int i = 0; i < numChars; i++){
          checkSumChars[i] = payload[i+4];
        }
        checkSum = atoi(checkSumChars);
        Serial.print("Checksum char length: ");
        Serial.println(checkSum);
        receiving = true;
        HWSERIAL.print("<*>ACK"); // the app will now send the contents to the watch
      } else {
        if(receiving){
          // now we just add the text to the data till we reach the checksum length
          if(dataIndex + payloadIndex < MAX_DATA_LENGTH){
             for(int j = 0; j < payloadIndex; j++){ // add the payload to the final data
              data[dataIndex] = payload[j];
              dataIndex++;
             }
          } else {
            Serial.println("Error! Final data is full!");
          }
          if(dataIndex >= checkSum){
            Serial.println("End of message, final contents: ");
            Serial.println(data);
            receiving  = false;
          }
        }
      }
      
    }
  }

  //reset payload for next block of text
  memset(payload, 0, sizeof(payload));

}



bool startsWith(char data[], char charSeq[], short len){
    for(short i=0; i < len; i++){
      if(!(data[i]==charSeq[i])){
        return false;
      }
    }
    return true;
}










