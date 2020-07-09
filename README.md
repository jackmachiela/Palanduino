# Palanduino

## (Arduino version of a Palantir)

![Palanduino, unboxed](https://raw.githubusercontent.com/jackmachiela/Palanduino/master/Palanduino%20Github%20Display.jpg)

This project creates a small IoT device that displays data off an internet service. The rotary encoder allows you to "dial" in to any number of channels; Youtube followers, Facebook Likes, current weather, whatever.

 - Channel 1 is basic time/date displays, adjusted for local timezones & daylight savings etc,
 - Channel 2 shows the current Covid-19 data for my country (New Zealand),
 - Channel 3 is a basic Weather Report from openweathermaps.org,
 - Channel 4 will be a weather forecast for the next 24 hours,
 - Channel 4 and 5 currently show a dummy millis() counter.

The hardware I'm using is a cheap Chinese NodeMCU Arduino ESP8266 unit. The code works also on a Mini D1 - the pinouts are compatible (I've tested with both).

Things to implement:

 - More channels
 - Add a button to the rotary switch
 - Add an alarm system if important info is published on non-active channels
 - Warning system for previous step, via flashing warning on LCD, or separate flashing LED
 - Implement a Reset button if it crashes, or hangs on some server connects
 - Change String to Char - less buggy

Full hardware documentation (parts used and pinouts) is contained within the code.
