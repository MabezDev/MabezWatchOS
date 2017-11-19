

#include "inc/system.h"

/*
* System methods
*/

void setAlarm(short alarmType, short hours, short minutes, short date, short month, short year)
{
    short start = alarmType == 0 ? ALARM_ADDRESS : ALARM_ADDRESS + ALARM_DATA_LENGTH;
    //  if(alarmType == 0){
    //    RTC.setAlarm(ALM1_MATCH_DATE, minutes, hours, date); // minutes // hours // date (28th of the month)
    //    Serial.println("ALARM1 Set!");
    //    start = ALARM_ADDRESS;
    //  } else {
    //    RTC.setAlarm(ALM2_MATCH_DATE, minutes, hours, date); // minutes // hours // date (28th of the month)
    //    Serial.println("ALARM2 Set!");
    //    start = ALARM_ADDRESS + 4;
    //  }

    /*
   * As our RTC not longer has alarm functionality we need to write it in software, this should be pretty simple as we already wrote the values to eeprom
   * instead of checking if the interrupt has gone off we should just check current time/date with the alarms ones, make sure not to miss it though!
   */
    Serial.println("Setting alarm for: ");
    Serial.print("Time: ");
    Serial.print(hours);
    Serial.print(":");
    Serial.print(minutes);
    Serial.println(":0");
    Serial.print("Date: ");
    Serial.print(date);
    Serial.print("/");
    Serial.print(month);
    Serial.print("/");
    Serial.print(year);
    Serial.println();

    uint32_t time;
    alarmTm.Hour = hours;
    alarmTm.Minute = minutes;
    alarmTm.Day = date;
    alarmTm.Month = month;
    alarmTm.Second = 0;
    alarmTm.Year = year; // - 1970
    time = makeTime(alarmTm);
    alarms[alarmType].time = time;

    Serial.print("Alarm makeTime: ");
    Serial.println(alarms[alarmType].time);
    Serial.print("Starting write at address: ");
    Serial.println(start);

    saveToEEPROM(start, true); //set alarm active
    start++;
    EEPROMWritelong(start, time); // need to write long
}

void createAlert(char const *text, short len, short vibrationTime)
{
    if (len < 20)
    {
        lastPage = pageIndex;
        alertTextLen = len;
        for (short i = 0; i < len; i++)
        {
            alertText[i] = text[i];
        }
        // set the timestamp
        alertRecieved[0] = clockArray[0];
        alertRecieved[1] = clockArray[1];
        vibrate(vibrationTime);
        pageIndex = ALERT;
    }
    else
    {
        Serial.println(F("Not Creating Alert, text to big!"));
    }
}

void vibrate(short vibrationTime)
{
    alertVibrationCount = (vibrationTime)*2; // double it as we toggle vibrate twice a second
}

void drawTriangle(short x, short y, short size, short direction)
{
    // some triangle are miss-shapen need to fix
    switch (direction)
    {
    case 1:
        display.drawTriangle(x, y, x + size, y, x + (size / 2), y + (size / 2), WHITE); /* u8g_DrawTriangle(&u8g,x,y,x+size,y, x+(size/2), y+(size/2)); */
        break;                                                                          //down
    case 2:
        display.drawTriangle(x, y, x + size, y, x + (size / 2), y - (size / 2), WHITE); /* u8g_DrawTriangle(&u8g,x,y,x+size,y, x+(size/2), y-(size/2)); */
        break;                                                                          //up
    case 3:
        display.drawTriangle(x + size, y, x, y - (size / 2), x + size, y - size, WHITE); /* u8g_DrawTriangle(&u8g,x+size,y,x,y-(size/2),x+size,y-size); */
        break;                                                                           // left
    case 4:
        display.drawTriangle(x + size, y - (size / 2), x, y, x, y - size, WHITE); /* u8g_DrawTriangle(&u8g,x + size,y-(size/2),x,y,x,y-size); */
        break;                                                                    // right
    }
}

void breakTime(uint32_t timeInput, struct TimeElements &tm)
{
    // Break the given time_t into time components
    // This is a more compact version of the C library localtime function
    // Note that year is offset from 1970 !!!

    uint8_t year;
    uint8_t month, monthLength;
    uint32_t time;
    unsigned long days;

    time = (uint32_t)timeInput;
    tm.Second = time % 60;
    time /= 60; // Now it is minutes
    tm.Minute = time % 60;
    time /= 60; // Now it is hours
    tm.Hour = time % 24;
    time /= 24;                     // Now it is days
    tm.Wday = ((time + 4) % 7) + 1; // Sunday is day 1

    year = 0;
    days = 0;
    while ((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= time)
    {
        year++;
    }
    tm.Year = year; // Year is offset from 1970

    days -= LEAP_YEAR(year) ? 366 : 365;
    time -= days; // Now it is days in this year, starting at 0

    days = 0;
    month = 0;
    monthLength = 0;
    for (month = 0; month < 12; month++)
    {
        if (month == 1)
        { // February
            if (LEAP_YEAR(year))
            {
                monthLength = 29;
            }
            else
            {
                monthLength = 28;
            }
        }
        else
        {
            monthLength = dayInMonth[month];
        }

        if (time >= monthLength)
        {
            time -= monthLength;
        }
        else
        {
            break;
        }
    }
    tm.Month = month + 1; // Jan is month 1
    tm.Day = time + 1;    // Day of month
}

uint32_t makeTime(struct TimeElements &tm)
{
    // Assemble time elements into "unix" format
    // Note year argument is offset from 1970

    int i;
    uint32_t seconds;

    // Seconds from 1970 till 1 jan 00:00:00 of the given year
    seconds = tm.Year * (SECS_PER_DAY * 365);
    for (i = 0; i < tm.Year; i++)
    {
        if (LEAP_YEAR(i))
        {
            seconds += SECS_PER_DAY; // Add extra days for leap years
        }
    }

    // Add days for this year, months start from 1
    for (i = 1; i < tm.Month; i++)
    {
        if ((i == 2) && LEAP_YEAR(tm.Year))
        {
            seconds += SECS_PER_DAY * 29;
        }
        else
        {
            seconds += SECS_PER_DAY * dayInMonth[i - 1]; // MonthDay array starts from 0
        }
    }
    seconds += (tm.Day - 1) * SECS_PER_DAY;
    seconds += tm.Hour * SECS_PER_HOUR;
    seconds += tm.Minute * SECS_PER_MIN;
    seconds += tm.Second;
    return (uint32_t)seconds;
}

void setClockTime(short hours, short minutes, short seconds, short days, short months, short years)
{
    tm.Hour = hours;
    tm.Minute = minutes;
    tm.Second = seconds;
    tm.Day = days;
    tm.Month = months;
    tm.Year = years - 1970; //offset
    Serial.print(F("Time: "));
    Serial.print(clockArray[0]);
    Serial.print(F(":"));
    Serial.print(clockArray[1]);
    Serial.print(F(":"));
    Serial.print(clockArray[2]);
    Serial.print(F("    Date: "));
    Serial.print(dateArray[0]);
    Serial.print(F("/"));
    Serial.print(dateArray[1]);
    Serial.print(F("/"));
    Serial.println(dateArray[2]);
    rt.setTime(makeTime(tm));
}

void resetBTModule()
{
    HWSERIAL.print("AT");       //disconnect
    delay(100);                 //need else the module won't see the commands as two separate ones
    HWSERIAL.print("AT+RESET"); //then reset
    createAlert("BT Module Reset.", 16, 0);
}

void removeNotificationById(int id)
{
    Serial.print("App requesting deletion of notification with ID: ");
    Serial.println(id);
    for (int i = 0; i < notificationIndex; i++)
    {
        if (notifications[i].id == id)
        {
            Serial.println("Found notification to delete. Removing now.");
            removeNotification(i);
            break; // don't remove multiple
        }
    }
}

void removeNotification(short pos)
{
    if (pos >= notificationIndex + 1)
    {
        Serial.println(F("Can't delete notification."));
    }
    else
    {
        //need to zero out the array or stray chars will overlap with notifications
        memset(notifications[pos].title, 0, sizeof(notifications[pos].title));
        memset(notifications[pos].packageName, 0, sizeof(notifications[pos].packageName));
        //removeTextFromNotification(&notifications[pos]);
        notifications[pos].textPointer = 0;
        for (short c = pos; c < (notificationIndex - 1); c++)
        {
            notifications[c] = notifications[c + 1];
        }
        Serial.print(F("Removed notification at index: "));
        Serial.println(pos);
        //lower the index
        notificationIndex--;
    }
}

//void removeTextFromNotification(Notification *notification){
//  Serial.print("Trying to remove text from a notification of type ");
//  bool found = false;
//  Serial.println(notification->textType);
//  char *arrIndexPtr = (char*)(types[notification->textType]); // find the begining of the respective array, i.e SMALL,NORMAL,LARGE
//  for(int i=0; i < textIndexes[notification->textType];i++){ // look through all valid elements
//    if((notification->textPointer - arrIndexPtr) == 0){ // more 'safe way' of comparing pointers to avoid compiler optimisations
//      Serial.print("Found the text to be wiped at index ");
//      Serial.print(i);
//      Serial.print(" in array of type ");
//      Serial.println(notification->textType);
//      found = true;
//      for ( short c = i ; c < (textIndexes[notification->textType] - 1) ; c++ ){
//        // move each block into the index before it, basically Array[c] = Array[c+1], but done soley using memory modifying methods
//         memcpy((char*)(types[notification->textType]) + (c * MSG_SIZE[notification->textType]),(char*)(types[notification->textType]) + ((c+1) *  MSG_SIZE[notification->textType]), MSG_SIZE[notification->textType]);
//      }
//      textIndexes[notification->textType]--; // remeber to decrease the index once we have removed it
//    }
//    arrIndexPtr += MSG_SIZE[notification->textType]; // if we haven't found our pointer, move the next elemnt by moving our pointer along
//  }
//  if(!found){
//    Serial.print("Failed to find the pointer for text : ");
//    Serial.println(notification->textPointer);
//  }
//}

//void removeNotification(short pos){
//  if ( pos >= notificationIndex + 1 ){
//    Serial.println(F("Can't delete notification."));
//  } else {
//    //need to zero out the array or stray chars will overlap with notifications
//    memset(notifications[pos].textPointer,0,sizeof(notifications[pos].textPointer));
//    memset(notifications[pos].title,0,sizeof(notifications[pos].title));
//    memset(notifications[pos].packageName,0,sizeof(notifications[pos].packageName));
//    for ( short c = pos ; c < (notificationIndex - 1) ; c++ ){
//       notifications[c] = notifications[c+1];
//    }
//    Serial.print(F("Removed notification at position: "));
//    Serial.println(pos);
//    //lower the index
//    notificationIndex--;
//  }
//}

float getBatteryVoltage()
{   // depreciated soon, will be using MAX17043
    /*
   * WARNING: Add voltage divider to bring batt voltage below 3.3v at all times! Do this before pluggin in the Batt or will destroy the Pin in a best case scenario
   * and will destroy the teensy in a worst case.
   */
    float reads = 0;
    for (short i = 0; i < 100; i++)
    {
        reads += analogRead(BATT_READ);
    }
    // R1 = 2000, R2 = 3300
    // Vin = (Vout * (R1 + R2)) / R2
    return ((reads / 100) * (3.3 / 1024) * (3300 + 2000)) / (3300);
}

/*
* Utility methods
*/

void setText(char *c, char *textPtr, short len)
{
    int i = 0;
    while (i < len)
    {
        // set the actual array value to the next value in the setText String
        *(textPtr++) = *(c++);
        i++;
    }
}

void intTo2Chars(short number)
{
    memset(numberBuffer, 0, sizeof(numberBuffer));
    if (number < 10)
    {
        //not working atm
        numberBuffer[0] = '0';
        numberBuffer[1] = char(number + 48);
    }
    else
    {
        itoa(number, numberBuffer, 10);
    }
}

bool startsWith(char data[], char charSeq[], short len)
{
    for (short i = 0; i < len; i++)
    {
        if (!(data[i] == charSeq[i]))
        {
            return false;
        }
    }
    return true;
}

bool contains(char data[], char character, short lenOfData)
{
    for (short i = 0; i < lenOfData; i++)
    {
        if (data[i] == character)
        {
            return true;
        }
    }
    return false;
}

// short FreeRam()
// {
//     // char top;
//     // #ifdef __arm__
//     //   return &top - reinterpret_cast<char*>(sbrk(0));
//     // #else  // __arm__
//     //   return __brkval ? &top - __brkval : &top - &__bss_end;
//     // #endif  // __arm__
//     return 0;
// }
