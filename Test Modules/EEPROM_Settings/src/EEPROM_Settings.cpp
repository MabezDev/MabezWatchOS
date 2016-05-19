
#include <Arduino.h>
#include <EEPROM.h>

/*
* Teensy Has 0-127 bytes of eeprom
*/


void setup(){
    Serial.begin(9600);
    Serial.println("Init...");
    //EEPROM.write(5,10); - comenting this to save EEPROM write cycles write(ADDRESS,VALUE);
}

void loop(){
  for(int i=0; i < EEPROM.length(); i++){
    Serial.print("Address: ");
    Serial.print(i);
    Serial.print(" Value: ");
    Serial.println(EEPROM.read(i));
  }

  delay(5000);
}
