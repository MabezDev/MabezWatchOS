#include <Arduino.h>


#define HWSERIAL Serial1


void setup(){
  Serial.begin(9600);
  while(!Serial){
    ;
  }

  Serial.println("Ready.");

  HWSERIAL.begin(9600);
}

void loop(){
  if(HWSERIAL.available()){
    Serial.write(HWSERIAL.read());
  }
  if(Serial.available()){
    HWSERIAL.write(Serial.read());
  }
}
