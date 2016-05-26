# MabezWatchOS

This is the firmware the Smart watch I am creating runs on. This code was created for use with A Teensy LC (3.x is also compatible).

## Notifications
  
  - Receive and break up the serial data into title, text, package name
  - Hold multiple Notifications in RAM
  - Display a whole Message (the text)
  - remove notifications
  
## Weather Data
  - Receive and break up the serial data into Day, Temperature and Forecast.
  - Provides a time that the data was received so we know if the data is accurate or not.
  
## Date/Time
  - Provides a nice Interface to see the time, like a classic clock face and a digital clock
  - Wouldn't be a watch without this functionality
  - Checks the RTC has the proper time everytime we connect to the app

## Other functionality
  - Timer, simple timer to time hours seconds and minutes with an alert to shop when the timer is done
  - Settings, store basic settings on the on board EEPROM
  - Can reset the BT module if it fails to connect after a while.


