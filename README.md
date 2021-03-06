# MabezWatchOS

This is the firmware the Smart watch I am creating runs on. This code was originally created for use with an Arduino Nano but has evolved and adapted to the Teensy LC (3.x is also compatible with a few register address changes) and now the STM32 ARM 32bit microcontroller.

## The Operating system

### Notifications
  - Receive and break up the serial data into title, text, package name
  - Hold multiple Notifications in RAM
  - Remove notifications
  - Display a whole Message (the text)
  - Control the flow of notifications (I.e if we run out of space we tell the App to hold the notifications in a queue ready for when we have space to receive more.)
  
### Weather Data
  - Receive and break up the serial data into Day, Temperature and Forecast.
  - Provides a time that the data was received so we know if the data is accurate or not.
  
### Date/Time
  - Provides a nice Interface to see the time, like a classic clock face and a digital clock
  - Wouldn't be a watch without this functionality
  - Checks the RTC has the proper time everytime we connect to the app
  - Timer, simple timer to time hours seconds and minutes with an alert to shop when the timer is done
  - Alarm, utilizes the 2 alarms available on the DS3231 RTC with easy to use interface.
  
### Input and gestures
  - Dual click (UP & DOWN together) to go back
  - Tri Click (ALL THREE together) to go to the home page with the favourite widget selected from the settings
  

### Other functionality
  - Settings, store basic settings on the on board EEPROM
  - Can reset the BT module if it fails to connect after a while.
  - Smart shutdown, instead of a clunky off toggle switch, we drop the watch into hibernation mode and can be woken by pressing the OK button at anytime.
  - Crash handler, usuing the built in watchdog timer we 'kick' the dog every cycle, if the dog does not get kicked it resets the watch (Disabled when Shutdown).
  - Status bar showing connection, battery and notification information.


## The Hardware
  - 1.3 inch (128x32) monochrome OLED (Black & White)
  - Capacitive touch buttons, instead of clunky tactile switches.
  - DS3231 RTC with alarm and temperature(not implmented currently) functionality
  - HM-11 Bluetooth 4.0 low energy transciever
  - TP4056 single cell lithium charger board
  - Single cell 240mAh lithium ion battery
  - Teensy LC (48Mhz) (Soon to be replaced with STM due to difficulty getting the bootloader from PJRC Store)
  
### Power Specifications
  - Draws 24mA on average when operating normally.
  - In Shutdown mode it draws 550uA
  - Power on time: Over 8 hours
  - Shutdown mode timer: 18 days
  - Quick charge under an Hour

