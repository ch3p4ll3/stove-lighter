# stove-lighter
Automate the ignition of a pellet stove with an esp32

## How to install
Install [this](https://github.com/Martin-Laclaustra/CronAlarms) library in yours Arduino IDE, edit the sketch by entering the ssid and password of your wifi and edit `gmtOffset_sec` to set yours timezone

## How to use
Connect, with a browser, to the IP address of the esp (print its IP at each boot), select the days and time in which you want to turn on the stove and press Submit. To reset, send the form without selecting anything, to view the active crons go to: http://<<esp_ip>>/ontime

## Features
*  Takes UTC time from the internet (can set your time zone by modifying the `gmtOffset_sec` variable)
*  It keeps the cron times in memory so in case the power goes out you don't have to reconfigure it
*  Possibility to turn the stove on or off manually

Please note: the software is still in beta version
