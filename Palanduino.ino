// Palanduino - Small IoT device to display 2 lines of information off various servers
//
//           Written during Corona Lockdown 2020 by Jack Machiela <jack@pobox.com>
//           Inspiration from /u/cakeanalytics ( https://github.com/BetterWayElectronics/esp8266-corona-tracking-i2c-oled )
//
//           Channel credits: Corona Virus data API provided by Javier Aviles https://github.com/javieraviles
//           
//  Components:
// * 1x Arduino/ESP8266 NodeMCU
// * 1x 1602A LCD Display (2x16 chars). I'm using a 16-pin one - 14 pin versions should work with minor modifications.
// * 3x 10k Ohm Potentiometers
// * 1x 360deg Rotary Encoder
//
//  The 1602A LCD Display:
// * LCD(01) VSS      - MCU ground
// * LCD(02) VCC/VDD  - Wiper Pot(1) - other two Pot(1) pins to MCU 5V(VV) & (unused)   - BRIGHTNESS
// * LCD(03) VO       - Wiper Pot(2) - other two Pot(2) pins to MCU 5V(VV) & MCU GND    - CONTRAST
// * LCD(04) RS       - MCU D2
// * LCD(05) R/W      - MCU GND
// * LCD(06) Enable   - MCU D3
// * LCD(07) D0       - (unused)                      //  The Rotary Encoder:
// * LCD(08) D1       - (unused)                      // * RE(1) CLK  - MCU D4
// * LCD(09) D2       - (unused)                      // * RE(2) DT   - MCU D0
// * LCD(10) D3       - (unused)                      // * RE(3) SW   - MCU D1
// * LCD(11) D4       - MCU D5                        // * RE(4) +    - MCU 3V
// * LCD(12) D5       - MCU D6                        // * RE(5) GND  - MCU GND
// * LCD(13) D6       - MCU D7
// * LCD(14) D7       - MCU D8
// * LCD(15) A        - Wiper Pot(3) - other Pot(3) pins to MCU 5V(VV) & (unused)   - BACKLIGHT
// * LCD(16) K        - MCU GND
//
// NB - The three Pots may need to be experimented with so they turn the right way (CCW = lower, CW = higher intensities).
//      If necessary, swap +(/-) terminals to other (non-swiper) side.
//
//  The NodeMCU (remaining pins), if using external PSU:
// * MCU VV    - +5v
// * MCU G     - GND
//
// NB - NEVER connect external PSU at the same time as powering through USB connector, as nuclear detonation may occur.
//



// Libraries to include:
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <LiquidCrystal.h>
#include <ArduinoJson.h>

/////////////////////////  WiFi Details - remove before publishing online

#define R_SSID       "ABCDEF"       // Wifi network name
#define PASS         "PASSWD"          // Wifi Password

const char* ssid     = R_SSID;

///////////////////////// Globals

// Create an ESP8266 WiFiClient class to connect to the various servers.
WiFiClient client;

// initialise the library with the numbers of the interface pins
const int RS = D2, EN = D3, d4 = D5, d5 = D6, d6 = D7, d7 = D8;
LiquidCrystal lcd(RS, EN, d4, d5, d6, d7);

//Define strings that will be displayed to the LCD screen
String lineOne;
String lineTwo;

// Some vars to limit the rate of server interrogations (throttle - don't strangle the internet!)
int interval;                                  // each module will have its own interval
unsigned long long lastCheckedMillis;
unsigned long long currentMillis;

void setup() {                    ///////////////////////////////////////////////////////////////////////////////////////////////// VOID SETUP
  Serial.begin(115200);
  delay(10);

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);

  // Initial welcome message
  // Restrict to 16 characters
  //       [ xxxxxxxxxxxxxxxx ]
  lineOne = "   Palanduino   ";
  lineTwo = "======== v1.0 ==";

  // Print the data to the screen
  updateLCD();

  // Initialise the internet link (login to Wi-Fi)
  init_wifi();

  //Make sure the first module gets to check something straight away
 // lastCheckedMillis = (-100000);   // To ensure the first module gets to GET DATA straight away

}


void loop() {                    ///////////////////////////////////////////////////////////////////////////////////////////////// VOID LOOP

  // Print the data to the screen
  updateLCD();

  //First channel - get updated New Zealand Covid-19 data
  channel_covid19();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// INITs

void init_wifi() {
  // Connect to WiFi access point.
  Serial.println(); Serial.println();
  Serial.print("Connecting to ");
  Serial.println(R_SSID);

  lineOne = "LAN:";  lineOne += ssid;
  lineTwo = ".";
  
  WiFi.begin(R_SSID, PASS);
  int counter=10;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    updateLCD();
    lineTwo += ".";
    
    counter--;
    if(counter==0){
      // reset me
        lineOne = "LAN login failed";
        lineTwo = "Resetting...";
        updateLCD();
        delay(2000);
        ESP.reset();
    }
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.println("IP address: "); Serial.println(WiFi.localIP());

  lineOne = "LAN: ";
  lineOne += ssid;

  lineTwo = "IP: ";
  lineTwo += WiFi.localIP().toString();

  updateLCD();

}

void updateLCD() {
  // Print a message to the LCD.
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(lineOne);
  lcd.setCursor(0, 1); lcd.print(lineTwo);

  Serial.println("Sent to LCD : ---------------------------------------------------");
  Serial.println("               + -  -  -  -  -  - +");
  Serial.print(  "                 "); Serial.println(lineOne);
  Serial.print(  "                 "); Serial.println(lineTwo);
  Serial.println("               + -  -  -  -  -  - +");

  delay(2000);

}

                    //////////////////////////////////////////////////////////////////////////////////////////////////////////// CHANNELS


void channel_covid19() {
  interval = 30;         // (in seconds)
  currentMillis = millis();

  if ((lastCheckedMillis==0) || ((currentMillis - lastCheckedMillis) > (interval*1000)))  {

    lineOne = "NZ Covid-19";
    lineTwo = "Updating...";
    updateLCD();

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

      lineOne = "Cov-19 Sick:" + String(cases);
      lineTwo = "Dead:" + String(deaths) + " Ok:" + String(recovered);

      lastCheckedMillis=millis();
    }
    http.end(); 

  }

}
