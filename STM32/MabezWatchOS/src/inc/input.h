
//input vars
bool button_ok = false;
bool lastb_ok = false;
bool button_up = false;
bool lastb_up = false;
bool button_down = false;
bool lastb_down = false;

#define CONFIRMATION_TIME 0 //80 change back when we go back to capactive //length in time the button has to be pressed for it to be a valid press
#define INPUT_TIME_OUT 60000 //60 seconds
#define TOUCH_THRESHOLD 1200 // value will change depending on the capacitance of the material