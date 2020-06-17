// Palanduino - Small IoT device to display 2 lines of information off various servers
//
//           Written during Corona Lockdown 2020 by Jack Machiela <jack@pobox.com>
//           Inspiration from /u/cakeanalytics ( https://github.com/BetterWayElectronics/esp8266-corona-tracking-i2c-oled )
//           Loads of programming tips from my son Josh Machiela <howdidthishappen@hesmyson.itaughthimeverythingiknow.com>
//
//           Channel credits: Corona Virus data API provided by Javier Aviles https://github.com/javieraviles
//           
//  Components:
// * 1x Arduino/ESP8266 NodeMCU
// * 1x 1602A LCD Display (2x16 chars). I'm using a 16-pin one - 14 pin versions should work with minor modifications.
// * 1x 360deg Rotary Encoder
//
//  The 1602A LCD Display hooks into the IIC/I2C 1602A 2004 Adapter Plate, which has four pins:
// * IIC(01) GND  - MCU ground
// * IIC(02) VCC  - MCU 5V(VV)
// * IIC(03) VO   - MCU D3
// * IIC(04) RS   - MCU D4
//  NB - some displays come with the IIC/I2C module already connected - if not, get one and solder it on. The screen is so much easier to deal with on the I2C interface.
//
//  The Rotary Encoder:
// * RE(5) GND  - MCU GND
// * RE(4) +    - MCU 5V(VV)
// * RE(3)                                   (SW   - MCU D2)
// * RE(2) DT   - MCU D1
// * RE(1) CLK  - MCU D0
//
//  The NodeMCU (remaining pins), if using external PSU:
// * MCU VV    - PSU +5v
// * MCU G     - PSU GND
//

// Libraries to include:
#include <ESP8266HTTPClient.h>
#include <WiFiManager.h>      // Using the Arduino Library Manager, install "WifiManager by tzapu" - https://github.com/tzapu/WiFiManager

#include <ezTime.h>           // lib at https://github.com/ropg/ezTime, docs at https://awesomeopensource.com/project/ropg/ezTime

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <MD_REncoder.h>      // Majic Designs Rotary Encoder Library    https://github.com/MajicDesigns/MD_REncoder


///////////////////////// Globals


// Define some wibbley wobbley timey wimey stuff
Timezone localTimeZone;
String TimeZoneDB = "Pacific/Auckland";      // See full list here: https://en.wikipedia.org/wiki/List_of_tz_database_time_zones 
String localNtpServer = "nz.pool.ntp.org";           // See full list here: http://support.ntp.org/bin/view/Servers/WebHome
bool showColon = true;


//  setDebug(INFO);  // Outputs NTP updates to serial monitor
//  setServer(localNtpServer);
//  Timezone timeZone;
//  setInterval(30);    //ntp polling interval     - temp, remove when done
//  timeZone.setLocation(localTimeZone);
//  waitForSync();      //Make sure to start with accurate time


// initialise the LCD instance
LiquidCrystal_I2C lcd(0x3F, 16, 2);

//Define strings that will be displayed to the LCD screen
  String lineOne;
  String lineTwo;
  //Define two more strings to see if the message has changed since last screen update
  String prevLineOne="";
  String prevLineTwo="";


//Define some vars for the Rotary Encoder (Channel Selector dial)
  MD_REncoder rotEnc = MD_REncoder(D0, D1);      //RotEnc's DR, CLK pins
  unsigned long long lastRotEncAction = 0;                       // Contains millis since last Rotary Encoder was clicked or rotated


// Some vars to limit the rate of server interrogations (throttle - don't strangle the internet!)
  int interval=1;                                  // each module will have its own interval, but start with 1 second
  unsigned long long lastCheckedMillis;
  unsigned long long currentMillis;

// And some Channel stuff
  int totalChannels=4;             // Total number of available channels
  int currentChannel=1;            // Start on Channel 1









void setup() {                    ///////////////////////////////////////////////////////////////////////////////////////////////// VOID SETUP
  Serial.begin(74880);
  delay(10);      // let the serial connection calm down


  // set up the LCD's number of columns and rows:
  Wire.begin(2,0);      // set up a IIC instance
  lcd.init();           // initialise the LCD display
  lcd.backlight();      // switch on the backlight 

  // Initial welcome message
  // Restrict to 16 characters
  //         [ xxxxxxxxxxxxxxxx ]
    lineOne = "   Palanduino   ";
    lineTwo = "======== v4.0 ==";
                 // v1.0 was initial basic code, one channel (NZ Covid-19 data update)
                 // v1.1 changed the main structure and smoothed it out, ready for multi-channel
                 // v1.2 - Add Rotary Encoder
                 // v2.0 - Changed LCD to use IIC/I2C protocol, saving 3x POTs, 16x Jumper wires, 4x NodeMCU pins
                 // v3.0 - Changed hardcoded WiFi details to WiFi Manager's on-the-fly configuration (safer!)
                 //        Also added clock and NTP libraries
                 // v4.0 - changed NTP library to EZTime
                 //
                 

  // Print the title to the screen
    updateLCD();
    delay(1500); // and give it a couple of seconds
  

  // Initialise the internet link (login to Wi-Fi)
  init_wifi();
  
  setDebug(INFO);   // Outputs NTP updates to serial monitor
  setServer(localNtpServer);
  setInterval(30);    //ntp polling interval     - temp, remove when done
  waitForSync();      //Make sure to start with accurate time
//  Timezone localTimeZone;
  localTimeZone.setLocation(TimeZoneDB);
///////////////////////////////////  localTimeZone.setDefault();   // ??????

//  Timezone timeZone;
//  timeZone.setLocation(localTimeZone);


  //checkNtpStatus();         // temp, remove when done
  Serial.println("[X] setup] UTC: " + UTC.dateTime());
  Serial.println("[X] Local time: " + localTimeZone.dateTime());


  // Initialise the Rotary Encoder
    rotEnc.begin();

}

void loop() {                    ///////////////////////////////////////////////////////////////////////////////////////////////// VOID LOOP
//  Timezone localTimeZone;
//  localTimeZone.setLocation(TimeZoneDB);

//  Serial.println("[Y] Local time: " + localTimeZone.dateTime());

  
  // lastRotEncAction
  
  uint8_t rotEncReading = rotEnc.read();

    if (rotEncReading)
  {

    currentChannel=(currentChannel + (rotEncReading == DIR_CW ? 1 : -1));   // Add or subtract 1 to the current channel
    if (currentChannel > totalChannels) currentChannel = 1;                 // If it goes over the maximum available channel, reset to 1
    if (currentChannel < 1) currentChannel = totalChannels;                 // If it goes under the lowest available channel, reset to highest
    interval = 0;                               //Channel changed - reset straight away
    lastRotEncAction = millis();                // Set last action to current time
    
    Serial.print("    currentChannel = ");
    Serial.println(currentChannel);

  }

  if ((lastRotEncAction + 1800) >= millis())       //if less than 3 seconds have passed since Rotatry encoder was used, display channel titles only
    {

      lineOne = "Select Channel: ";
      lineTwo = "                ";
      
      if (currentChannel == 1) lineTwo = "1: Time & Date  ";                  //
      if (currentChannel == 2) lineTwo = "2: NZ Covid-19  ";                  //Get updated New Zealand Covid-19 data
      if (currentChannel == 3) lineTwo = "3: Channel Three";                  //
      if (currentChannel == 4) lineTwo = "4: Channel Four ";                  //
      
      updateLCD();

    }
  else
    {
      // Check if interval for this channel has been reached
      currentMillis = millis();
      if ((lastCheckedMillis==0) || ((currentMillis - lastCheckedMillis) > (interval*1000)))  {
  
      if (currentChannel == 1) {                           //timeAndDate();                       //
        interval = 1;         // (in seconds) How often should I check this website for updates after the initial check
        // Initialise the SNTP time synchro
        //timeZone.setLocation(localTimeZone);
        Serial.println("A:Local time: " + localTimeZone.dateTime());
        lineOne = localTimeZone.dateTime("  h:i:s A   ");
        lineTwo = localTimeZone.dateTime("D d M, Y");
        Serial.println("B:Local time: " + localTimeZone.dateTime());
      }
      if (currentChannel == 2) channelNZCovid19();                  //Get updated New Zealand Covid-19 data
      if (currentChannel == 3) channelThree();                      //
      if (currentChannel == 4) channelFour();                       //

      lastCheckedMillis=millis();
    
    }
  
  }


  updateLCD();
  events();         // Check if any scheduled ntp events
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// Functions


void init_wifi() {
  // Connect to WiFi access point.
    lineOne = "Connecting to   ";
    lineTwo = "    LAN via WiFi";
    updateLCD();

  // Start WiFi
    WiFiManager wifiManager;
    wifiManager.autoConnect("Palanduino_Install");        // <----------- Create Wifi LAN to install with, if not previously initialised

    delay(1000);

}





void updateLCD() {
//Serial.println("TEMP - lineOne" + lineOne);
//Serial.println("TEMP - length" + lineOne.length());

//  while ( lineOne.length() < 16 ) {                // crashes the arduino for some reason. Needs work.
//    lineOne = lineOne + " ";
//  }
//  while ( lineTwo.length() < 16 ) {
//    lineOne=lineTwo+" ";
//  }
  
  // Check if the data has been updated, otherwise don't bother updating the display
  if ((lineOne != prevLineOne) || (lineTwo != prevLineTwo)) {

    // Print a message to the LCD.
//    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(lineOne);
    lcd.setCursor(0, 1); lcd.print(lineTwo);

    // also print the message over the serial link (if arduino is connected via USB)
    Serial.println("Sent to LCD : ---------------------------------------------------");
    Serial.println("               + -  -  -  -  -  - +");
    Serial.print(  "                 "); Serial.println(lineOne);
    Serial.print(  "                 "); Serial.println(lineTwo);
    Serial.println("               + -  -  -  -  -  - +");

    // Set the previous lines as the current ones
    prevLineOne = lineOne;
    prevLineTwo = lineTwo;
}
  

}

                    //////////////////////////////////////////////////////////////////////////////////////////////////////////// CHANNELS


void timeAndDate() {

  
}





void channelNZCovid19() {
  interval = 300;         // (in seconds) How often should I check this website for updates after the initial check

    lineOne = "NZ Covid-19     ";
    lineTwo = "Updating...     ";
    updateLCD();
    
    delay(1000);

    HTTPClient http;
    //GET directly from the URL (Dont use HTTPS) Modify the JSON Value as required!
    http.begin("http://coronavirus-19-api.herokuapp.com/countries/New%20Zealand");

    int httpCode = http.GET();
    if(httpCode > 0)
    {
      String payload = http.getString();

      // NB. The following bit was (mostly) created using https://arduinojson.org/v6/assistant/ -Jack
        const size_t capacity = JSON_OBJECT_SIZE(12) + 170;
        DynamicJsonDocument doc(capacity);

        deserializeJson(doc, payload);

        const char* country = doc["country"];
        int cases = doc["cases"];
        int todayCases = doc["todayCases"];
        int deaths = doc["deaths"];
        int todayDeaths = doc["todayDeaths"];
        int recovered = doc["recovered"];
        int active = doc["active"];
        int critical = doc["critical"];
        int casesPerOneMillion = doc["casesPerOneMillion"];
        int deathsPerOneMillion = doc["deathsPerOneMillion"];
        long totalTests = doc["totalTests"];
        int testsPerOneMillion = doc["testsPerOneMillion"];

      lineOne = "Cov-19 - Sick:" + String(active) + " ";
      lineTwo = "Dead:" + String(deaths) + " Ok:" + String(recovered)+ " ";

//      lastCheckedMillis=millis();
    }
    else
    {
      lineOne = "Cov-19 - Sick:??";
      lineTwo = "Dead:?? Ok:???? ";

    }
    http.end(); 

//  }

}

void channelThree() {
      interval = 1;         // (in seconds) How often should I check this website for updates after the initial check

      lineOne = "Channel 3       ";
      lineTwo = "Millis:" + String(millis()) + "     ";
}

void channelFour() {
      interval = 1;         // (in seconds) How often should I check this website for updates after the initial check

      lineOne = "Channel 4       ";
      lineTwo = "Millis:" + String(millis()) + "     ";
}
