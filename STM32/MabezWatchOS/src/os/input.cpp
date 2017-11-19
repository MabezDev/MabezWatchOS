#include "inc/input.h"
#include <Arduino.h>



void handleInput()
{
    short vector = getConfirmedInputVector();
    if (vector != lastVector)
    {
        // TODO: once we've clicked a new button if we keep holding it we should keep handling that event, i.e keep increasing a number if we hold the button - implement this
        if (vector == UP_DOWN)
        {
            Serial.println(F("Dual click detected!"));
            handleDualClick();
        }
        else if (vector == UP_ONLY)
        {
            handleUpInput();
        }
        else if (vector == DOWN_ONLY)
        {
            handleDownInput();
        }
        else if (vector == OK_ONLY)
        {
            handleOkInput();
        }
        else if (vector == ALL_THREE)
        {
            Serial.println(F("Return to menu Combo"));
            pageIndex = HOME_PAGE;            // take us back to the home page
            widgetSelector = settingValue[0]; // and our fav widget
        }
        prevButtonPressed = millis();
        idle = false;
    }
    if (vector == NONE_OF_THEM)
    {
        if (((millis() - prevButtonPressed) > INPUT_TIME_OUT) && (prevButtonPressed != 0))
        {
            Serial.println(F("Time out input"));
            prevButtonPressed = 0;
            pageIndex = HOME_PAGE;            // take us back to the home page
            widgetSelector = settingValue[0]; //and our fav widget
            Y_OFFSET = 0;                     // reset the Y_OFFSET so if we come off a page with an offset it doesnt get bugged
            currentLine = 0;
            idle = true;
        }
    }
    lastVector = vector;
}

void handleDualClick()
{
    // currently just return to home
    pageIndex = HOME_PAGE;
}

void handleUpInput()
{
    Serial.println(F("Up Click Detected"));
    if (pageIndex == HOME_PAGE)
    {
        widgetSelector++;
        if (widgetSelector > numberOfWidgets)
        {
            widgetSelector = 0;
        }
    }
    else if (pageIndex == NOTIFICATION_MENU)
    {
        menuUp(notificationIndex);
    }
    else if (pageIndex == NOTIFICATION_BIG)
    {
        if ((lineCount - currentLine) >= 6)
        {
            //this scrolls down
            Y_OFFSET -= FONT_HEIGHT;
            currentLine++;
        }
    }
    else if (pageIndex == TIMER)
    {
        if (locked)
        {
            timerArray[timerIndex]++;
        }
        else
        {
            timerIndex++;
            if (timerIndex > 4)
            {
                timerIndex = 0;
            }
        }
    }
    else if (pageIndex == SETTINGS)
    {
        //check if were locked first (changing value)
        if (locked)
        {
            settingValue[menuSelector]++;
            if (settingValue[menuSelector] > settingValueMax[menuSelector])
            {
                settingValue[menuSelector] = settingValueMax[menuSelector];
                ;
            }
        }
        else
        {
            menuUp(numberOfSettings);
        }
    }
    else if (pageIndex == ALARM_PAGE)
    {
        if (locked)
        {
            alarms[alarmToggle].alarmTime[alarmIndex]++;
            if (alarms[alarmToggle].alarmTime[alarmIndex] > alarmMaxValues[alarmIndex])
            {
                alarms[alarmToggle].alarmTime[alarmIndex] = alarmMaxValues[alarmIndex];
            }
        }
        else
        {
            alarmIndex++;
            if (alarmIndex > 4)
            {
                alarmIndex = 0; // 3 is max
            }
        }
    }
    else
    {
        Serial.println(F("Unknown Page."));
    }
}

void menuUp(short size)
{
    menuSelector++;
    //check here if we need scroll up to get the next items on the screen//check here if we nmeed to scroll down to get the next items
    if ((menuSelector >= 4) && (((size + 1) - menuSelector) > 0))
    { //0,1,2,3 = 4 items
        //shift the y down
        Y_OFFSET -= MENU_ITEM_HEIGHT;
    }
    if (menuSelector >= size)
    {
        //menuSelector = 0;
        menuSelector = size;
    }
}

void menuDown()
{
    menuSelector--;
    if (menuSelector < 0)
    {
        menuSelector = 0;
        //menuSelector = notificationIndex + 1;
    }
    //plus y
    if ((menuSelector >= 3))
    {
        Y_OFFSET += MENU_ITEM_HEIGHT;
    }
}

void handleDownInput()
{
    Serial.println(F("Down Click Detected"));
    if (pageIndex == HOME_PAGE)
    {
        widgetSelector--;
        if (widgetSelector < 0)
        {
            widgetSelector = numberOfWidgets;
        }
    }
    else if (pageIndex == NOTIFICATION_MENU)
    {
        menuDown();
    }
    else if (pageIndex == NOTIFICATION_BIG)
    {
        if (currentLine > 0)
        {
            //scrolls back up
            Y_OFFSET += FONT_HEIGHT;
            currentLine--;
        }
    }
    else if (pageIndex == TIMER)
    {
        if (locked)
        {
            timerArray[timerIndex]--;
            if (timerArray[timerIndex] < 0)
            {
                timerArray[timerIndex] = 0;
            }
        }
        else
        {
            timerIndex--;
            if (timerIndex < 0)
            {
                timerIndex = 4;
            }
        }
    }
    else if (pageIndex == SETTINGS)
    {
        if (locked)
        {
            settingValue[menuSelector]--;
            if (settingValue[menuSelector] < settingValueMin[menuSelector])
            {
                settingValue[menuSelector] = settingValueMin[menuSelector];
            }
        }
        else
        {
            menuDown();
        }
    }
    else if (pageIndex == ALARM_PAGE)
    {
        if (locked)
        {
            alarms[alarmToggle].alarmTime[alarmIndex]--;
            if (alarms[alarmToggle].alarmTime[alarmIndex] < 0)
            {
                alarms[alarmToggle].alarmTime[alarmIndex] = 0;
            }
        }
        else
        {
            alarmIndex--;
            if (alarmIndex < 0)
            {
                alarmIndex = 4; // 3 is max
            }
        }
    }
    else
    {
        Serial.println(F("Unknown Page."));
    }
}

void handleOkInput()
{
    Serial.println(F("OK Click Detected"));
    if (pageIndex == HOME_PAGE)
    {
        if (widgetSelector == 3)
        {
            pageIndex = NOTIFICATION_MENU;
        }
        else if (widgetSelector == 4)
        {
            pageIndex = TIMER;
        }
        else if (widgetSelector == 5)
        {
            pageIndex = SETTINGS;
        }
        else if (widgetSelector == 6)
        {
            pageIndex = ALARM_PAGE;
        }
        else if (widgetSelector == 7)
        {
            resetBTModule();
        }
        Y_OFFSET = 0;
    }
    else if (pageIndex == NOTIFICATION_MENU)
    {
        if (menuSelector != notificationIndex)
        { //last one is the back item
            Y_OFFSET = 0;
            pageIndex = NOTIFICATION_BIG;
        }
        else
        {
            menuSelector = 0;      //reset the selector
            pageIndex = HOME_PAGE; // go back to list of notifications
        }
    }
    else if (pageIndex == NOTIFICATION_BIG)
    {
        shouldRemove = true;
        if (menuSelector > 3)
        {
            Y_OFFSET = -1 * (menuSelector - 3) * MENU_ITEM_HEIGHT; //return to place
        }
        else
        {
            Y_OFFSET = 0;
        }
        lineCount = 0;   //reset number of lines
        currentLine = 0; // reset currentLine back to zero
        pageIndex = NOTIFICATION_MENU;
    }
    else if (pageIndex == TIMER)
    {
        if (timerIndex == 3)
        {
            isRunning = !isRunning; //start/stop timer
        }
        else if (timerIndex == 4)
        {
            Serial.println(F("Resetting timer."));
            isRunning = false;
            timerArray[0] = 0;
            timerArray[1] = 0;
            timerArray[2] = 0;
        }
        else
        {
            locked = !locked; //lock or unlock into a digit so we can manipulate it
        }
    }
    else if (pageIndex == SETTINGS)
    {
        if (menuSelector == (numberOfSettings))
        { //thisis the back button
            menuSelector = 0;
            pageIndex = HOME_PAGE;
            //dont reset the widgetIndex to give the illusion we just came from there
        }
        else
        {
            locked = !locked;
            if (!locked)
            {
                saveToEEPROM(menuSelector, settingValue[menuSelector]);
            }
        }
    }
    else if (pageIndex == ALERT)
    {
        memset(alertText, 0, sizeof(alertText)); //reset the alertText
        alertTextLen = 0;                        //reset the index
        pageIndex = lastPage;                    //go back
    }
    else if (pageIndex == ALARM_PAGE)
    {
        //handle alarmInput
        if (alarmIndex == 3)
        {
            alarms[alarmToggle].active = !alarms[alarmToggle].active;
            if (alarms[alarmToggle].active)
            {
                if ((dateArray[0] + alarms[alarmToggle].alarmTime[2]) > dayInMonth[dateArray[1] - 1])
                {
                    setAlarm(alarmToggle, alarms[alarmToggle].alarmTime[0], alarms[alarmToggle].alarmTime[1], ((dateArray[0] + alarms[alarmToggle].alarmTime[2]) - dayInMonth[dateArray[1] - 1]), dateArray[1], dateArray[2]); // (dateArray[0] + alarmTime[2]) == current date plus the days in advance we want to set the
                                                                                                                                                                                                                               //Serial.print("Alarm set for the : ");
                                                                                                                                                                                                                               //Serial.println(((dateArray[0] + alarms[alarmToggle].alarmTime[2]) - dayInMonth[dateArray[1] - 1]));
                }
                else
                {
                    setAlarm(alarmToggle, alarms[alarmToggle].alarmTime[0], alarms[alarmToggle].alarmTime[1], (dateArray[0] + alarms[alarmToggle].alarmTime[2]), dateArray[1], dateArray[2]);
                    //Serial.print("Alarm set for the : ");
                    //Serial.println((dateArray[0] + alarms[alarmToggle].alarmTime[2]));
                }
            }
        }
        else if (alarmIndex == 4)
        {
            if (alarmToggle == 0)
            {
                alarmToggle = 1;
            }
            else
            {
                alarmToggle = 0;
            }
        }
        else
        {
            locked = !locked;
        }
    }
    else
    {
        Serial.println(F("Unknown Page."));
    }
}

short getConfirmedInputVector()
{
    static short lastConfirmedVector = 0;
    static short lastVector = -1;
    static long unsigned int heldVector = 0L;

    // Combine the inputs.
    short rawVector =
        isButtonPressed(OK_BUTTON) << 2 |
        isButtonPressed(DOWN_BUTTON) << 1 |
        isButtonPressed(UP_BUTTON) << 0;

    /*Serial.print("Okay Button: ");
  Serial.println(touchRead(OK_BUTTON));
  Serial.print("Down Button: ");
  Serial.println(touchRead(DOWN_BUTTON));
  Serial.print("Up Button: ");
  Serial.println(touchRead(UP_BUTTON));*/

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
