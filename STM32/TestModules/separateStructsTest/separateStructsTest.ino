


struct Notification {
  char packageName[15];
  char title[15];
  short dateReceived[2];
  short textLength;
  virtual short getType();
};

struct SmallNotification : Notification {
  short getType(){  return 0; };
  char text[25];
};

struct NormalNotification : Notification {
  char text[150];
  short getType(){  return 1; };
};

struct LargeNotification : Notification {
  short getType(){  return 2; };
  char text[500];
};

//SmallNotification sArray[50];
//NormalNotification nArray[25];
//LargeNotification lArray[5];

Notification *nArray[20]; //points to the notifications

void setup() {
  Serial.begin(9600);
  while(!Serial.isConnected());
  // check what type of not we want, create it and add to the Array
  // in this instance a small notification
  SmallNotification newNotification;
  char text[] = "123456789012345678901234512345678901234567890";
  setText(text,newNotification.text,sizeof(text));
  nArray[0] = &newNotification;
  int type = nArray[0]->getType();
  if(type==0){
    SmallNotification *e = static_cast<SmallNotification*>(nArray[0]);
    Serial.println("Small");
    Serial.println(e->text);
  }else if(type==1){
    NormalNotification *e = static_cast<NormalNotification*>(nArray[0]);
    Serial.println("Normal");
    Serial.println(e->text);
  } else if(type==2){
    LargeNotification *e = static_cast<LargeNotification*>(nArray[0]);
    Serial.println("Large");
    Serial.println(e->text);
  }
}

int determineType(void* n){
  Notification *e = static_cast<Notification*>(n);
  return e->getType();
}

void setText(char* c, char* textPtr, short len){
  int i = 0;
  while(i < len){
    // set the actual array value to the next value in the setText String
    *(textPtr++) = *(c++);
    i++;
  }
}

void loop() {
  // put your main code here, to run repeatedly:

}
