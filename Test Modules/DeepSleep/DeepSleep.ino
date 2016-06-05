
#include <Snooze.h>

SnoozeBlock config;

int OK_BUTTON = 16;

void setup() {
  pinMode(LED_BUILTIN,OUTPUT);
  Serial.begin(9600);
  config.pinMode(OK_BUTTON, TSI, touchRead(OK_BUTTON) + 250);
}

void loop() {
  int whatPin = Snooze.deepSleep(config);
  if(whatPin == 37){ // 37 is the TSI detected number
    delay(1000);
    digitalWrite(LED_BUILTIN,HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN,LOW);
  }
}
