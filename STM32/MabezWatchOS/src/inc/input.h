#ifndef __M_INPUT__
#define __M_INPUT__

#include "inc/globals.h"
#include "inc/pin_defs.h"
#include "inc/eeprom.h"
#include "inc/system.h"

//need to use 4,2,1 as no combination of any of the numbers makes the same number, where as 1,2,3 1+2 = 3 so there is no individual state.
#define UP_ONLY 4
#define OK_ONLY 2
#define DOWN_ONLY 1
#define UP_OK (UP_ONLY | OK_ONLY)
#define UP_DOWN (UP_ONLY | DOWN_ONLY)
#define DOWN_OK (OK_ONLY | DOWN_ONLY)
#define ALL_THREE (UP_ONLY | OK_ONLY | DOWN_ONLY)
#define NONE_OF_THEM 0

#define CONFIRMATION_TIME 0 //80 change back when we go back to capactive //length in time the button has to be pressed for it to be a valid press
#define INPUT_TIME_OUT 60000 //60 seconds
#define TOUCH_THRESHOLD 1200 // value will change depending on the capacitance of the material

#define isButtonPressed(pin) (digitalRead(pin) == LOW) //this was old with buttons
//#define isButtonPressed(pin) (touchRead(pin) > TOUCH_THRESHOLD)

//input vars
bool button_ok = false;
bool lastb_ok = false;
bool button_up = false;
bool lastb_up = false;
bool button_down = false;
bool lastb_down = false;

/*
* Input handling methods
*/

void handleInput();
void handleDualClick();
void handleUpInput();
void menuUp(short size);
void menuDown();
void handleDownInput();
void handleOkInput();
short getConfirmedInputVector();

#endif