#include <Arduino.h>




void setup() {
 pinMode(VIBRATE_PIN,OUTPUT);
 Serial.begin(9600);
}

void loop() {
  

}

void alert(){
  //buzzes with vibration motor
  //activates transistor that connects the buzzer with 3.3v
  // due to the nature of transistors there is 0.7v drop
  //bringing the voltage to 2.6v - which is safe for the motors
    digitalWrite(VIBRATE_PIN,HIGH);
    delay(500);
    digitalWrite(VIBRATE_PIN,LOW);
}
