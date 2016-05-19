#include <Arduino.h>

bool touch = false;
bool wasTouched = false;
int timesTouched = 0;


void setup() {

}

void loop() {
    touch = touchRead(15) > 1000;
    if(touch != wasTouched){
      if(touch){ //check touch
        Serial.print("Touched! Total:");
        timesTouched++;
        Serial.println(timesTouched);
      }
    }
    wasTouched = touch;
}
