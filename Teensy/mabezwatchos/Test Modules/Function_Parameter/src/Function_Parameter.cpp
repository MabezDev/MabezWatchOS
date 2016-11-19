#include <Arduino.h>





void setup(){
  Serial.begin(9600);
}

void showFunction(void f(int)){
  f(1);
}



void demoFunction(int i){
  Serial.println("We called this.");
}

void demoFunction2(int i){
  Serial.println("We called that.");
}


void loop(){
  showFunction(demoFunction);
  delay(1000);
}
