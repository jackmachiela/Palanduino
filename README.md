# Palanduino

This project creates a small IoT device that displays data off an internet service. The rotary encoder allows you to "dial" in to any number of channels; Youtube followers, Facebook Likes, current weather, whatever.

Channel 1 is basic time/date displays
Channel 2 shows the current Covid-19 data for my country (New Zealand),
Channel 3 and 4 currently show a dummy millis() counter.

The hardward I'm using, a cheap Chinese NodeMCU Arduino ESP8266 unit, has lots of potential feature I want to implement here.

Things to implement:
 - More channels
 - Add a button to the rotary switch
 - Flashing colon for time channel
 - Add an alarm system if important info is published on non-active channels
 - Warning system for previous step, via flashing warning on LCD, or separate flashing LED

Full hardward documentation (parts used and pinouts) is contained within the code.
