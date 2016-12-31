short sIndex = 0;
short nIndex = 0;
short lIndex = 0;

const short SMALL = 0;
const short NORMAL = 1;
const short LARGE = 2;

char SmallText[50][25];
char NormalText[25][200];
char LargeText[5][750];
//char **types[3] = {SmallText,NormalText,LargeText};
//String test[12];
//String *ty[1] = {test};



struct Notification {
//  char packageName[15];
//  char title[15];
  String title;
  short dateReceived[2];
  short textLength;
  char *textPointer; //points to a char array containing the text
  short textType; // used to find or remove in the correct array
};

Notification notifications[80];
int notificationIndex = 0;



void setup() {
  Serial.begin(9600);
  while(!Serial.isConnected());

  newNotification("Hello",5,SMALL);
  newNotification("Hello this is evidently longer than 25 characters and quite clearly needs to go in a normal notification",104,NORMAL);
  newNotification("Lorem Ipsum is simply dummy text of the printing and typesetting industry. Lorem Ipsum has been the industry's standard dummy text ever since the 1500s, when an unknown printer took a galley of type and scrambled it to make a type specimen book. It has survived not only five centuries, but also the leap into electronic typesetting, remaining essentially unchanged. It was popularised in the 1960s with the release of Letraset sheets containing Lorem Ipsum passages, and more recently with desktop publishing software like Aldus PageMaker including versions of Lorem Ipsum.",574,LARGE);
  
  
  printNotificationTexts();

  removeNotification(1);

  printNotificationTexts();
  
  
}

void newNotification(char *text, short len ,short type){
  notifications[notificationIndex].title = "Test title";
  notifications[notificationIndex].textType = type;
  addTextToNotification(&notifications[notificationIndex],text,len);
  notificationIndex++;
}

void removeNotification(short pos){
  if ( pos >= notificationIndex + 1 ){
    Serial.println(F("Can't delete notification."));
  } else {
    //need to zero out the array or stray chars will overlap with notifications
    //memset(notifications[pos].title,0,sizeof(notifications[pos].title));
    //memset(notifications[pos].packageName,0,sizeof(notifications[pos].packageName));
    removeTextFromNotification(&notifications[pos]);
    for ( short c = pos ; c < (notificationIndex - 1) ; c++ ){
       notifications[c] = notifications[c+1];
    }
    Serial.print(F("Removed notification at position: "));
    Serial.println(pos);
    //lower the index
    notificationIndex--;
  }
}

void printNotificationTexts(){
  Serial.println("Notification Texts: ");
  for(int i=0; i < notificationIndex; i++){
    Serial.print(i+1);
    Serial.print("). ");
    Serial.println(notifications[i].textPointer);
  }
  
}


void removeTextFromNotification(Notification *notification){
  switch(notification->textType){
    case 0:
        //find our notification to remove using out pointer
        for(int i=0; i < sIndex;i++){
          if(notification->textPointer == SmallText[i]){
            for ( short c = i ; c < (sIndex - 1) ; c++ ){
               strcpy(SmallText[c], SmallText[c+1]);
            }
            sIndex--;
          }
        }
        break;
     case 1:
        //find our notification to remove using out pointer
        for(int i=0; i < nIndex;i++){
          if(notification->textPointer == NormalText[i]){
            for ( short c = i ; c < (nIndex - 1) ; c++ ){
               strcpy(NormalText[c], NormalText[c+1]);
            }
            nIndex--;
          }
        }
        break;
     case 2:
        for(int i=0; i < lIndex;i++){
          if(notification->textPointer == LargeText[i]){
            for ( short c = i ; c < (lIndex - 1) ; c++ ){
               strcpy(LargeText[c], LargeText[c+1]);
            }
            lIndex--;
          }
        }
  }
}

void removeNotification(short pos, char *arr, short len){
    //need to zero out the array or stray chars will overlap with notifications
    arr+= pos; // move pointer to pos
    for ( short c = pos ; c < (len - 1) ; c++ ){
       //arr[c] = arr[c+1];
       arr = arr++;
    }
}

// TYPE, address of the notification to change, text to set , length of text to set
void addTextToNotification(Notification *notification, char *textToSet, short len){
  switch(notification->textType){
    case 0:
        Serial.println("SMALL");
        setText(textToSet,SmallText[sIndex],len);
        notification->textPointer = SmallText[sIndex];
        notification->textLength = len;
        sIndex++;
        break;
    case 1:
        Serial.println("NORMAL");
        setText(textToSet,NormalText[nIndex],len);
        notification->textPointer = NormalText[nIndex];
        notification->textLength = len;
        nIndex++;
        break;
    case 2:
        Serial.println("LARGE");
        setText(textToSet,LargeText[lIndex],len);
        notification->textPointer = LargeText[lIndex];
        notification->textLength = len;
        lIndex++;
        break;
  }
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
