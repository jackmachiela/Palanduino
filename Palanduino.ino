// Palanduino - Small IoT device to display 2 lines of information off various servers
//
//           Written during Corona Lockdown 2020 by Jack Machiela <jack@pobox.com> (c) 2020
//           Inspiration from /u/cakeanalytics ( https://github.com/BetterWayElectronics/esp8266-corona-tracking-i2c-oled )
//           Loads of programming tips from my son Josh <howdidthishappen@hesmyson.itaughthimeverythingiknow.com>
//
//           Channel credits: Corona Virus data API provided by Javier Aviles https://github.com/javieraviles/covidAPI
//                            Weather Data provided by https://openweathermap.org/
//                                                                                                                                                 UPDATE: PALANDUINO IS NOT CURRENTLY WORKING DUE TO OUTDATED API'S
//
//  Components:
// * 1x Arduino/ESP8266 NodeMCU, or 1x Mini D1 (pinouts will work with (and have been tested with) either arduino compatible unit.
// * 1x 1602A LCD Display (2x16 chars). I'm using a 16-pin one - 14 pin versions should work with minor modifications.
// * 1x IIC/I2C 1602A 2004 Adapter Plate for the 1602A screen.
// * 1x 360deg Rotary Encoder
// * Breadboard, cables, etc.
//
//  The 1602A LCD Display hooks into the IIC/I2C 1602A 2004 Adapter Plate, which has four pins:
// * IIC(01) GND    - MCU G      - MD1 G
// * IIC(02) VCC    - MCU 5V(VV) - MD1 5V
// * IIC(03) SDA/VO - MCU D4     - MD1 D4
// * IIC(04) SCL/RS - MCU D3     - MD1 D3
//  NB - some displays come with the IIC/I2C module already connected - if not, get one and solder it on. The screen is
//       so much easier to deal with on the I2C interface.
//
//  The Rotary Encoder:
// * RE(1) CLK  - MCU D1     - MD1 D1
// * RE(2) DT   - MCU D0     - MD1 D0
// * RE(3) SW   - MCU D2     - MD1 D2             // Button functionality not yet implemented
// * RE(4) +    - MCU 5V(VV) - MD1 5V
// * RE(5) GND  - MCU G      - MD1 G
//
//  The Mini D1 (remaining pins), if using external PSU:
// * MD1 VV    - MD1 5V    - PSU +5v
// * MD1 G     - MD1 G     - PSU GND
//

// Libraries to include:
#include <WiFiManager.h>          // Library "WifiManager by tablatronix" - Tested at v2.0.3-alpha (aka "WifiManager by tzapu" - lib at https://github.com/tzapu/WiFiManager)
#include <ezTime.h>               // Library "EZTime by Rop Gonggrijp" - Tested at v0.8.3 (lib & docs at https://github.com/ropg/ezTime, docs at https://awesomeopensource.com/project/ropg/ezTime
#include <TM1637Display.h>        // Library "TM1637 by Avishay Orpaz" - Tested at v1.2.0
#include <ESP8266HTTPClient.h>   // Once the connection to the internet is made, this library talks to actual servers. Make sure "Board" is set to your ESP8266 board (NodeMCU, Mini D1, or whatever you're using)
#include <Wire.h>                // Built-in library, to use the I2C/IIC protocol to address the 1602A LCD display
#include <LiquidCrystal_I2C.h>   // Library "LiquidCrystal I2C by Marco Schwartz" - Tested at v1.1.2 (lib at https://github.com/johnrickman/LiquidCrystal_I2C - possibly outdated)
                                 //    Lib used to talk to the actual 1602A LCD display.
                                 //    TO DO: Library is outdated. Update to "LCD_I2C" by Blackhack.
#include <ArduinoJson.h>         // Library "ArduinoJson by Benoit Blanchon" - Tested at v6.18.4 (lib & docs at https://arduinojson.org/?utm_source=meta&utm_medium=library.properties)
                                 //    Lib used to talk to servers and parse their JSON data
#include <MD_REncoder.h>         // Library "MD_REncoder by marco_c 8136821@gmail.com" - Tested at v1.0.1 (aka "Majic Designs Rotary Encoder" - lib & docs at https://github.com/MajicDesigns/MD_REncoder)

///////////////////////// Globals


// Define some wibbley wobbley timey wimey stuff
Timezone localTimeZone;
String TimeZoneDB = "Pacific/Auckland";              // See full list here: https://en.wikipedia.org/wiki/List_of_tz_database_time_zones 
String localNtpServer = "nz.pool.ntp.org";           // See full list here: http://support.ntp.org/bin/view/Servers/WebHome
bool showColon = true;



//open weather map api key                                   // Get a free account and set up an API key on: https://home.openweathermap.org/api_keys
String weatherApiKey = "d6d87c4134612c3994b6b29278cf23d7";   // DO NOT PUBLISH THE KEY!!! (this is not actually my key, so don't bother)
//the city you want the weather for
String weatherLocation = "Mangatainoka, NZ";                 // Make sure you have a valid city code - get it at http://www.openweathermap.org as well


// initialise the LCD instance
LiquidCrystal_I2C lcd(0x3F, 16, 2);

//Define strings that will be displayed to the LCD screen
  String lineOne;
  String lineTwo;
  //Define two more strings to see if the message has changed since last screen update
  String prevLineOne="";
  String prevLineTwo="";


//Define some vars for the Rotary Encoder (Channel Selector dial)
  MD_REncoder rotEnc = MD_REncoder(D0, D1);                      // RotEnc's DT, CLK pins
  unsigned long long lastRotEncAction = 0;                       // Contains millis since last Rotary Encoder was clicked or rotated


// Some vars to limit the rate of server interrogations (throttle - don't strangle the internet!)
  int interval=1;                                                // each module will have its own interval, but start with 1 second
  unsigned long long lastCheckedMillis;
  unsigned long long currentMillis;

// And some Channel stuff
  int totalChannels=5;                                           // Total number of available channels
  int currentChannel=1;                                          // Start on Channel 1


void setup() {                    ///////////////////////////////////////////////////////////////////////////////////////////////// VOID SETUP
  Serial.begin(74880);
  delay(10);      // let the serial connection calm down


  // set up the LCD's number of columns and rows:
  Wire.begin(2,0);                          // set up a IIC instance
  lcd.init();                               // initialise the LCD display
  lcd.backlight();                          // switch on the backlight 

  // Initial welcome message
  // Restrict to 16 characters
  //         [ xxxxxxxxxxxxxxxx ]
    lineOne = "   Palanduino";
    lineTwo = "================";
                 // v1.0 was initial basic code, one channel (NZ Covid-19 data update)
                 // v1.1 changed the main structure and smoothed it out, ready for multi-channel
                 // v1.2 - Add Rotary Encoder
                 // v2.0 - Changed LCD to use IIC/I2C protocol, saving 3x POTs, 16x Jumper wires, 4x NodeMCU pins
                 // v3.0 - Changed hardcoded WiFi details to WiFi Manager's on-the-fly configuration (safer!)
                 //        Also added clock and NTP libraries
                 // v4.0 - changed NTP library to EZTime
                 // v5.0 - Added channel 3 - weather forecast


  // Print the title to the screen
    updateLCD();
    delay(1500);                        // and give it a moment
  

  // Initialise the internet link (login to Wi-Fi)
  init_wifi();
  
  setDebug(INFO);                                           // Outputs NTP updates to serial monitor
  setServer(localNtpServer);                                // Go easy on the international NTP server and use a localised one instead
  setInterval(30);                                          // NTP polling interval     - temp, remove when everything works
  waitForSync();                                            // Make sure to start with accurate time
  localTimeZone.setLocation(TimeZoneDB);                    // Set the default time to the local timezone

  // Initialise the Rotary Encoder
    rotEnc.begin();

}

void loop() {                    ///////////////////////////////////////////////////////////////////////////////////////////////// VOID LOOP
  
  uint8_t rotEncReading = rotEnc.read();
  if (rotEncReading)  {

    currentChannel=(currentChannel + (rotEncReading == DIR_CW ? 1 : -1));   // Add or subtract 1 to the current channel
    if (currentChannel > totalChannels) currentChannel = 1;                 // If it goes over the maximum available channel, reset to 1
    if (currentChannel < 1) currentChannel = totalChannels;                 // If it goes under the lowest available channel, reset to highest
    interval = 0;                                                           //Channel changed - reset straight away
    lastRotEncAction = millis();                                            // Set last action to current time
    
    Serial.print("Current Channel = ");
    Serial.println(currentChannel);

  }

  if ((lastRotEncAction + 1800) >= millis()) {              //if less than 3 seconds have passed since Rotatry encoder was used, display channel titles only

    lineOne = "Select Channel:";
    lineTwo = "";

    if (currentChannel == 1) lineTwo = "1:Time & Date";                     //Get current date & time based on NZ timezone
    if (currentChannel == 2) lineTwo = "2:NZ Covid-19";                     //Get latest New Zealand Covid-19 data
    if (currentChannel == 3) lineTwo = "3:Weather Report";                  //Get Wellington NZ weather forecast
    if (currentChannel == 4) lineTwo = "4:Channel Four";                    // (dummy channel)
    if (currentChannel == 5) lineTwo = "5:Channel Five";                    // (dummy channel)
      
    updateLCD();

  }
  else {
    // Check if interval for this channel has been reached
    currentMillis = millis();
    if ((lastCheckedMillis==0) || ((currentMillis - lastCheckedMillis) > (interval*1000)))  {
  
      if (currentChannel == 1) channelTimeAndDate();                //Display current Timezone's time & date
      if (currentChannel == 2) channelNZCovid19();                  //Get updated New Zealand Covid-19 data
      if (currentChannel == 3) channelWeather();                    //Weather Report for Wellington, NZ [from OpenWeatherMap]
      if (currentChannel == 4) channelFour();                       // (dummy channel)
      if (currentChannel == 5) channelFive();                       // (dummy channel)

      lastCheckedMillis=millis();
    
    }
  
  }


  updateLCD();
  events();                          // Check if any scheduled ntp events
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// Other Functions

void init_wifi() {
  // Connect to WiFi access point.
  lineOne = "Connecting to";
  lineTwo = "    LAN via WiFi";
  updateLCD();

  // Start WiFi
  WiFiManager wifiManager;
  wifiManager.autoConnect("Palanduino_Install");        // <----------- Create Wifi LAN to install with, if not previously initialised

  delay(1000);

}




void updateLCD() {

  while ( lineOne.length() < 16 ) lineOne.concat(" ");     //Pad the strings to 16 chars long, to clear out previous messages
  while ( lineTwo.length() < 16 ) lineTwo.concat(" ");     //Pad the strings to 16 chars long, to clear out previous messages
  
  // Check if the data has been updated, otherwise don't bother updating the display
  if ((lineOne != prevLineOne) || (lineTwo != prevLineTwo)) {

  // Print a message to the LCD.
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


void channelTimeAndDate() {

  interval = 1;         // (in seconds) How often should I check this website for updates after the initial check

  showColon = !showColon;
  if (showColon) lineOne = localTimeZone.dateTime(" h:i:s A");           // flash one second with dots showing
  else lineOne = localTimeZone.dateTime(" h i s A");                     // flash one second with dots not showing
  
  lineTwo = localTimeZone.dateTime("D d M, Y");

}


void channelNZCovid19() {
  interval = 300;         // (in seconds) How often should I check this website for updates after the initial check

  lineOne = "NZ Covid-19";
  lineTwo = "Updating...";
  updateLCD();
    
  delay(1000);

  HTTPClient http;
  //GET directly from the URL (Dont use HTTPS) Modify the JSON Value as required!
  http.begin("http://coronavirus-19-api.herokuapp.com/countries/New%20Zealand");

  int httpCode = http.GET();
  if(httpCode > 0)
    {
      String payload = http.getString();

      // NB. The following bit was (mostly) created using https://arduinojson.org/v6/assistant/
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

      lineOne = "Covid: Sick:" + String(active);
      lineTwo = "Dead:" + String(deaths) + " Ok:" + String(recovered);

    }
    else
    {
      lineOne = "Covid: Sick:??";
      lineTwo = "Dead:?? Ok:????";

    }
    http.end(); 

}



void channelWeather() {
  interval = 900;         // (in seconds) How often should I check this website for updates after the initial check

    lineOne = weatherLocation + ",";
    lineTwo = "Weather update";
    updateLCD();
    
    delay(1000);
    HTTPClient http;
    //GET directly from the URL (Dont use HTTPS) Modify the JSON Value as required!
    http.begin("http://api.openweathermap.org/data/2.5/weather?q=" + weatherLocation + "&units=metric&appid=" + weatherApiKey);

    int httpCode = http.GET();
 
    if(httpCode > 0)
    {
      String payload = http.getString();

      // NB. The following bit was (mostly) created using https://arduinojson.org/v6/assistant/ -Jack
      const size_t capacity = JSON_ARRAY_SIZE(3) + 2*JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + 3*JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(12) + 340;
      DynamicJsonDocument doc(capacity);


      deserializeJson(doc, payload);

      float coord_lon = doc["coord"]["lon"]; // -0.13
      float coord_lat = doc["coord"]["lat"]; // 51.51

      JsonArray weather = doc["weather"];

      JsonObject weather_0 = weather[0];
      int weather_0_id = weather_0["id"]; // 301
      const char* weather_0_main = weather_0["main"]; // "Drizzle"
      const char* weather_0_description = weather_0["description"]; // "drizzle"
      const char* weather_0_icon = weather_0["icon"]; // "09n"

      JsonObject weather_1 = weather[1];
      int weather_1_id = weather_1["id"]; // 701
      const char* weather_1_main = weather_1["main"]; // "Mist"
      const char* weather_1_description = weather_1["description"]; // "mist"
      const char* weather_1_icon = weather_1["icon"]; // "50n"

      JsonObject weather_2 = weather[2];
      int weather_2_id = weather_2["id"]; // 741
      const char* weather_2_main = weather_2["main"]; // "Fog"
      const char* weather_2_description = weather_2["description"]; // "fog"
      const char* weather_2_icon = weather_2["icon"]; // "50n"

      const char* base = doc["base"]; // "stations"

      JsonObject main = doc["main"];
      float main_temp = main["temp"]; // 281.87
      int main_pressure = main["pressure"]; // 1032
      int main_humidity = main["humidity"]; // 100
      float main_temp_min = main["temp_min"]; // 281.15
      float main_temp_max = main["temp_max"]; // 283.15

      int visibility = doc["visibility"]; // 2900

      float wind_speed = doc["wind"]["speed"]; // 1.5

      int clouds_all = doc["clouds"]["all"]; // 90

      long dt = doc["dt"]; // 1483820400

      JsonObject sys = doc["sys"];
      int sys_type = sys["type"]; // 1
      int sys_id = sys["id"]; // 5091
      float sys_message = sys["message"]; // 0.0226
      const char* sys_country = sys["country"]; // "GB"
      long sys_sunrise = sys["sunrise"]; // 1483776245
      long sys_sunset = sys["sunset"]; // 1483805443

      long id = doc["id"]; // 2643743
      const char* name = doc["name"]; // "London"
      int cod = doc["cod"]; // 200

      lineOne = String(weather_0_main) + ", " + String(weather_1_description);
      lineTwo = "Temp:" + String((int(main_temp-0.5))+0.5) + ((char)223) + "C";

    }
    else
    {
      lineOne = "No Weather Data:";
      lineTwo = "Couldn't connect";

    }
    http.end(); 

}

void channelFour() {
      interval = 1;         // (in seconds) How often should I check this website for updates after the initial check

      lineOne = "Channel 4";
      lineTwo = "Millis:" + String(millis());
}

void channelFive() {
      interval = 1;         // (in seconds) How often should I check this website for updates after the initial check

      lineOne = "Channel 5";
      lineTwo = "Millis:" + String(millis());
}
