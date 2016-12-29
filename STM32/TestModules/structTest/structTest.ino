
//class Notification{
//  public:
//      char packageName[15];
//      char title[15];
//      short dateReceived[2];
//      short textLength;
//      virtual char* getText(){
//        return text;
//      }
//      virtual void setText(char* c){
//        char *textPtr = text;
//        int i = 0;
//        while(*c != '\0' && i < sizeof(text)){
//          *(textPtr++) = *(c++);
//          i++;
//        }
//        Serial.println(i);
//      }
//  private:
//    char text[1];
//};
//
//class SmallNotification : public Notification{
//  public:
//      char text[50];
//};
//
//class LargeNotification : public Notification{
//  public:
//      char text[60];
//};


class Notification{
  public:
      char packageName[15];
      char title[15];
      short dateReceived[2];
      short textLength;
      virtual void setText(char* c);
      virtual char* getText();
};

class SmallNotification : public Notification{
  public:
      char text[50];
      virtual char* getText(){
        return text;
      }
      virtual void setText(char* c){
        char *textPtr = text;
        while(*c != '\0'){
          *(textPtr++) = *(c++);
        }
      }
};

class LargeNotification : public Notification{
  public:
      char text[60];
      virtual char* getText(){
        return text;
      }
      virtual void setText(char* c){
        char *textPtr = text;
        int i = 0;
        while(*c != '\0' && i < sizeof(text)){
          *(textPtr++) = *(c++);
          i++;
        }
        text[i] = '\0';
      }
};

Notification *nArray[12];

void setup() {
  Serial.begin(9600);
  while(!Serial.isConnected());
  
  nArray[0] = new SmallNotification();
  nArray[1] = new LargeNotification();
  nArray[0]->setText("Hello this is message that is Really Short");
  nArray[1]->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero. Sed cursus ante dapibus diam. Sed nisi. Nulla quis sem at nibh elementum imperdiet. Duis sagittis ipsum. Praesent mauris. Fusce nec tellus sed augue semper porta. Mauris massa. Vestibulum lacinia arcu eget nulla. Class aptent taciti sociosqu ad litora torquent per conubia nostra, per inceptos himenaeos. Curabitur sodales ligula in libero. Sed dignissim lacinia nunc. Curabitur tortor. Pellentesque nibh. Aenean quam. In scelerisque sem at dolor. Maecenas mattis. Sed convallis tristique sem. Proin ut ligula vel nunc egestas porttitor. Morbi lectus risus, iaculis vel, suscipit quis, luctus non, massa. Fusce ac turpis quis ligula lacinia aliquet. Mauris ipsum. Nulla metus metus, ullamcorper vel, tincidunt sed, euismod in, nibh. Quisque volutpat condimentum velit. Class aptent taciti sociosqu ad litora torquent per conubia nostra, per inceptos himenaeos. Nam nec ante. Sed lacinia, urna non tincidunt mattis, tortor neque adipiscing diam, a cursus ipsum ante quis turpis. Nulla facilisi. Ut fringilla. Suspendisse potenti. Nunc feugiat mi a tellus consequat imperdiet. Vestibulum sapien. Proin quam. Etiam ultrices. Suspendisse in justo eu magna luctus suscipit. Sed lectus. Integer euismod lacus luctus magna. Quisque cursus, metus vitae pharetra auctor, sem massa mattis sem, at interdum magna augue eget diam. Vestibulum ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia Curae; Morbi lacinia molestie dui. Praesent blandit dolor. Sed non quam. In vel mi sit amet augue congue elementum. Morbi in ipsum sit amet pede facilisis laoreet. Donec lacus nunc, viverra nec.");
  Serial.println(nArray[0]->getText());
  Serial.println(nArray[1]->getText());
  
}

void loop() {
  // put your main code here, to run repeatedly:

}
