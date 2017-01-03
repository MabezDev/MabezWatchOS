const short SMALL = 0;
const short NORMAL = 1;
const short LARGE = 2;

const short MSG_SIZE[3] = {25,200,750};
short textIndexes[3] = {0,0,0};

char SmallText[50][25];
char NormalText[25][200];
char LargeText[5][750];

void *types[3] = {SmallText,NormalText,LargeText}; // store a pointer of each matrix, later to be casted to a char *

typedef struct {
  char packageName[15];
  //char title[15];
  String title;
  short dateReceived[2];
  short textLength;
  /*
   * Add these two vars below to the Notification struct in the OS
   */
  char *textPointer; //points to a char array containing the text, replaces the raw text
  short textType; // used to find or remove in the correct array
} Notification;

Notification notifications[80];
int notificationIndex = 0;



void setup() {
  Serial.begin(9600);
  while(!Serial.isConnected());
  
  newNotification("Hello",5,SMALL);
  newNotification("Hellooooo222",12,SMALL);
  newNotification("Hellooooo333",12,SMALL);
  newNotification("Hellooooo444",12,SMALL);
  newNotification("Hello this is evidently longer than 25 characters and quite clearly needs to go in a normal notification",104,NORMAL);
  newNotification("Whassup this is evidently longer than 25 characters and quite clearly needs to go in a normal notification",104,NORMAL);
  newNotification("olleH this is evidently longer than 25 characters and quite clearly needs to go in a normal notification",104,NORMAL);
  newNotification("Lorem Ipsum is simply dummy text of the printing and typesetting industry. Lorem Ipsum has been the industry's standard dummy text ever since the 1500s, when an unknown printer took a galley of type and scrambled it to make a type specimen book. It has survived not only five centuries, but also the leap into electronic typesetting, remaining essentially unchanged. It was popularised in the 1960s with the release of Letraset sheets containing Lorem Ipsum passages, and more recently with desktop publishing software like Aldus PageMaker including versions of Lorem Ipsum.",574,LARGE);
  newNotification("Lorem DIFF1 is simply dummy text of the printing and typesetting industry. Lorem Ipsum has been the industry's standard dummy text ever since the 1500s, when an unknown printer took a galley of type and scrambled it to make a type specimen book. It has survived not only five centuries, but also the leap into electronic typesetting, remaining essentially unchanged. It was popularised in the 1960s with the release of Letraset sheets containing Lorem Ipsum passages, and more recently with desktop publishing software like Aldus PageMaker including versions of Lorem Ipsum.",574,LARGE);
  newNotification("Lorem DIFF2 is simply dummy text of the printing and typesetting industry. Lorem Ipsum has been the industry's standard dummy text ever since the 1500s, when an unknown printer took a galley of type and scrambled it to make a type specimen book. It has survived not only five centuries, but also the leap into electronic typesetting, remaining essentially unchanged. It was popularised in the 1960s with the release of Letraset sheets containing Lorem Ipsum passages, and more recently with desktop publishing software like Aldus PageMaker including versions of Lorem Ipsum.",574,LARGE);
  
  
  printNotificationTexts();
  //printSmallArray();
  Serial.println();

  removeNotification(2);

  Serial.println();

  //printSmallArray();
  printNotificationTexts();
  
  
}

void addTextToNotification(Notification *notification, char *textToSet, short len){
  // gives us the location to start writing our new text
  char *textPointer = (char*)(types[notification->textType]); //pointer to the first element in the respective array, i.e Small, Normal Large
  /*
   * Move the pointer along an element at a time using the MSG_SIZE array to get the size of an element based on type, 
   * then times that by the index we want to add the text to
   */
  textPointer += (MSG_SIZE[notification->textType] * textIndexes[notification->textType]);
  // write the text to the new found location
  setText(textToSet, textPointer, len);
  // store a reference to the textpointer for later removal
  notification->textPointer = textPointer;
  notification->textLength = len;

//  Serial.print("Index for type ");
//  Serial.print(notification->textType);
//  Serial.print(" is ");
//  Serial.println(textIndexes[notification->textType]);
  
  textIndexes[notification->textType]++; //increment the respective index
  
}

void removeTextFromNotification(Notification *notification){
  char *arrIndexPtr = (char*)(types[notification->textType]); // find the begining of the respective array, i.e SMALL,NORMAL,LARGE
  for(int i=0; i < textIndexes[notification->textType];i++){ // look through all valid elements
    if((notification->textPointer - arrIndexPtr) == 0){ // more 'safe way' of comparing pointers to avoid compiler optimisations
      Serial.print("Found the text to be wiped at index ");
      Serial.print(i);
      Serial.print(" in array of type ");
      Serial.println(notification->textType);
      for ( short c = i ; c < (textIndexes[notification->textType] - 1) ; c++ ){
        // move each block into the index before it, basically Array[c] = Array[c+1], but done soley using memory modifying methods
         memcpy((char*)(types[notification->textType]) + (c * MSG_SIZE[notification->textType]),(char*)(types[notification->textType]) + ((c+1) *  MSG_SIZE[notification->textType]), MSG_SIZE[notification->textType]);
      }
      textIndexes[notification->textType]--; // remeber to decrease the index once we have removed it
    }
    arrIndexPtr += MSG_SIZE[notification->textType]; // if we haven't found our pointer, move the next elemnt by moving our pointer along
  }
}

void newNotification(char *text, short len ,short type){
  // this code can go in getNotification in the OS, should work fine provided we add our new funcs and vars
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
    for ( short c = pos; c < (notificationIndex - 1) ; c++ ){
//       Serial.print(notifications[c].textPointer);
//       Serial.print(" now equals ");
//       Serial.println(notifications[c+1].textPointer);
       notifications[c] = notifications[c+1];
    }
    Serial.print(F("Removed notification at index: "));
    Serial.println(pos);
    //lower the index
    notificationIndex--;
  }
}

void printNotificationTexts(){
  Serial.println("Notification Texts: ");
  for(int i=0; i < notificationIndex; i++){
    Serial.print(i);
    Serial.print("). ");
    Serial.println(notifications[i].textPointer);
  }
  
}

void printSmallArray(){
  Serial.print("SmallText (index = ");
  Serial.print(textIndexes[0]);
  Serial.println(") :");
  for(int i=0; i < textIndexes[0]; i++){
    Serial.print(i);
    Serial.print("). ");
    Serial.println(SmallText[i]);
  }
}


//void removeTextFromNotification(Notification *notification){
//  switch(notification->textType){
//    case 0:
//        //find our notification to remove using out pointer
//        for(int i=0; i < sIndex;i++){
//          if(notification->textPointer == SmallText[i]){
//            for ( short c = i ; c < (sIndex - 1) ; c++ ){
//               strcpy(SmallText[c], SmallText[c+1]);
//            }
//            sIndex--;
//          }
//        }
//        break;
//     case 1:
//        //find our notification to remove using out pointer
//        for(int i=0; i < nIndex;i++){
//          if(notification->textPointer == NormalText[i]){
//            for ( short c = i ; c < (nIndex - 1) ; c++ ){
//               strcpy(NormalText[c], NormalText[c+1]);
//            }
//            nIndex--;
//          }
//        }
//        break;
//     case 2:
//        for(int i=0; i < lIndex;i++){
//          if(notification->textPointer == LargeText[i]){
//            for ( short c = i ; c < (lIndex - 1) ; c++ ){
//               strcpy(LargeText[c], LargeText[c+1]);
//            }
//            lIndex--;
//          }
//        }
//  }
//}

//void removeNotification(short pos, char *arr, short len){
//    //need to zero out the array or stray chars will overlap with notifications
//    arr+= pos; // move pointer to pos
//    for ( short c = pos ; c < (len - 1) ; c++ ){
//       //arr[c] = arr[c+1];
//       arr = arr++;
//    }
//}

//// TYPE, address of the notification to change, text to set , length of text to set
//void addTextToNotification(Notification *notification, char *textToSet, short len){
//  switch(notification->textType){
//    case 0:
//        Serial.println("SMALL");
//        setText(textToSet,SmallText[sIndex],len);
//        notification->textPointer = SmallText[sIndex];
//        notification->textLength = len;
//        sIndex++;
//        break;
//    case 1:
//        Serial.println("NORMAL");
//        setText(textToSet,NormalText[nIndex],len);
//        notification->textPointer = NormalText[nIndex];
//        notification->textLength = len;
//        nIndex++;
//        break;
//    case 2:
//        Serial.println("LARGE");
//        setText(textToSet,LargeText[lIndex],len);
//        notification->textPointer = LargeText[lIndex];
//        notification->textLength = len;
//        lIndex++;
//        break;
//  }
//}

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
