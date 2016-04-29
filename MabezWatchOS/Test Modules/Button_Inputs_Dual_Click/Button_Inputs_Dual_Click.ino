
#define CONFIRMATION_TIME 80 //length in time the button has to be pressed for it to be a valid press


//pin numbers
#define UP_BUTTON 3
#define OK_BUTTON 4
#define DOWN_BUTTON 5

//need to use 4,2,1 as no combination of any of the numbers makes the same number, where as 1,2,3 1+2 = 3 so there is no individual state.
#define UP_ONLY  4
#define OK_ONLY  2
#define DOWN_ONLY  1
#define UP_OK  (UP_ONLY|OK_ONLY)
#define UP_DOWN  (UP_ONLY|DOWN_ONLY)
#define DOWN_OK  (OK_ONLY|DOWN_ONLY)
#define ALL_THREE (UP_ONLY|OK_ONLY|DOWN_ONLY)
#define NONE_OF_THEM  0

#define INPUT_TIME_OUT 15000 //15 seconds

// This macro returns TRUE if a button is pressed.
// It assumes that the given pin number is in INPUT_PULLUP mode,
// and the button closes the contact between the given pin and ground.
//
#define isButtonPressed(pin)  (digitalRead(pin) == LOW)

int lastVector = 0;
long prevButtonPressed = 0;


// Get the total combination of button presses at the current instant.
// This function performs two important features.
//
// (1) combine all three buttons' pressed/unpressed status into one number
//     called an input vector
//
// (2) don't accept a change in the input vector until a certain confirmation
//     time has elapsed; this makes it easier for the user to go from zero
//     buttons pressed to two buttons pressed even if they don't get pressed
//     at the exact same instant


void setup() {
  pinMode(UP_BUTTON, INPUT_PULLUP);
  pinMode(OK_BUTTON, INPUT_PULLUP);
  pinMode(DOWN_BUTTON, INPUT_PULLUP);
}

void loop() {
    int  vector = getConfirmedInputVector();
    if(vector!=lastVector){
      if (vector == UP_DOWN){ 
        Serial.println("Dual click detected!");
      } else if (vector == UP_ONLY){
        Serial.println("Up Click Detected");
      } else if(vector == DOWN_ONLY){
        Serial.println("Down Click Detected");
      } else if(vector == OK_ONLY){
        Serial.println("OK Click Detected");
      } else if(vector == ALL_THREE){
        Serial.println("Return to menu Combo");
      }
      prevButtonPressed = millis();
    }
    if(vector == NONE_OF_THEM){
      if(((millis() - prevButtonPressed) > INPUT_TIME_OUT) && (prevButtonPressed != 0)){
        Serial.println("Time out input");
        prevButtonPressed = 0;
      }
    }
    lastVector = vector;
}

int getConfirmedInputVector()
{
  static int lastConfirmedVector = 0;
  static int lastVector = -1;
  static long unsigned int heldVector = 0L;

  // Combine the inputs.
  int rawVector =
    isButtonPressed(OK_BUTTON) << 2 |
    isButtonPressed(DOWN_BUTTON) << 1 |
    isButtonPressed(UP_BUTTON) << 0;

  // On a change in vector, don't return the new one!
  if (rawVector != lastVector)
  {
    heldVector = millis();
    lastVector = rawVector;
    return lastConfirmedVector;
  }

  // We only update the confirmed vector after it has
  // been held steady for long enough to rule out any
  // accidental/sloppy half-presses or electric bounces.
  //
  long unsigned heldTime = (millis() - heldVector);
  if (heldTime >= CONFIRMATION_TIME)
  {
    lastConfirmedVector = rawVector;
  }

  return lastConfirmedVector;
}






