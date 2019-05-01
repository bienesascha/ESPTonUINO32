//==================================================================
// TonUINO V3.2.0 of ESP32 Basis
// Original V2.0: T. Voss, Erweitert V3.0: C. Ulbrich
//------------------------------------------------------------------
// the DIY jukebox (not only) for kids
//
// Take an Arduino, an MP3 module, an RFID reader, a micro SD card,
// some cables and stuff and an old (or new) bookshelf speaker... 
// and you have the TonUINO!
//==================================================================

// Used libraries
#include <WiFi.h>
#include <WebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "TonUINO_html.h"
#include "TonUINO.h"
#include <Arduino.h>
#include "DFRobotDFPlayerMini.h"
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include <JC_Button.h>
#include <math.h>
#include <FastLED.h>
#include <Preferences.h>
#include <Update.h>

// EEPROM Storage *NVS*
Preferences preferences;
int timeout = 20;

//========================================================================
// Only variables for testing
int l = 0;
bool headphoneIn = 0 ;
int success = 0;
int success2 = 0;

bool debug = true; //false // Set to "true" to get debug information via the serial port.

//Variables for saving settings
//========================================================================

unsigned long last_color = 0xFFFFFF;
unsigned int last_Volume;
unsigned int last_max_Volume;

//========================================================================
//WS2812b settings
#define DATA_PIN 2      // signal pin 
#define NUM_LEDS 7      // number of LEDs in your strip
#define BRIGHTNESS 32   // brightness  (max 254) 
#define LED_TYPE WS2811 // I know we are using ws2812, but it's ok!
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

//============Sunrise Variables===========================================
DEFINE_GRADIENT_PALETTE( sunrise_gp ) {
  0,     0,  0,  0,             // black
  128,   240,  0,  0,           // red
  224,   240, 240,  0,          // yellow
  255,   128, 128, 240
}; //very bright blue
static uint16_t heatIndex = 0;  // start out at 0

//========================================================================
//NTP Variables
WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

//Variable for reading the HTML response
String TimerOFF = "00:00";
String TimerON = "00:00";
uint8_t TMR_OFF_HH, TMR_OFF_MM, TMR_ON_HH, TMR_ON_MM;
int TMR_OFF_REP = 0;
int TMR_ON_REP = 0;
unsigned int max_Volume = 40;
unsigned int akt_Volume = 10;
bool TMP_OFFTIME = true;
bool TMP_ONTIME = true;
bool WakeUpLight = false;
bool SleepLight = false;
bool startSR = false;

//Set Pins for RC522 Module
const int resetPin = 22; // Reset pin
const int ssPin = 21;    // Slave select pin

//Created object for communication with the module
MFRC522 mfrc522 = MFRC522(ssPin, resetPin); // Create instance

//=======================Functions Declaration==============================
nfcTagObject myCard;
bool knownCard = false;
uint16_t numTracksInFolder;
uint16_t track;
uint64_t chipid;

//====================Timer Declaration=====================================
hw_timer_t * timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile uint32_t isrCounter = 0;
volatile boolean isrRead = true;

void IRAM_ATTR onTimer() {
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  isrCounter++;
  isrRead = false;
  portEXIT_CRITICAL_ISR(&timerMux);
  // Give a semaphore that we can check in the loop
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
  // It is safe to use digitalRead/Write here if you want to toggle an output
}

// implement a notification class,
// its member methods will get called
//
class Mp3Notify {
  public:
    static void OnError(uint16_t errorCode) {
      // see DfMp3_Error for code meaning
      Serial.println();
      Serial.print("Com Error ");
      Serial.println(errorCode);
    }
    static void OnPlayFinished(uint16_t track) {
      Serial.print("Track finished");
      Serial.println(track);
      delay(100);
      nextTrack();
    }
    static void OnCardOnline(uint16_t code) {
      Serial.println(F("SD card online "));
    }
    static void OnCardInserted(uint16_t code) {
      Serial.println(F("SD card ready "));
    }
    static void OnCardRemoved(uint16_t code) {
      Serial.println(F("SD card removed "));
    }
};

HardwareSerial mySoftwareSerial(2);
DFRobotDFPlayerMini myDFPlayer;

// Unfortunately, the module can not play a queue
static void nextTrack() {
  if (knownCard == false)
    // When a new card is learned, the end of a track should not be processed
    return;

  if (myCard.mode == 1) {
    Serial.println(F("Radio play mode is active -> save power"));
    myDFPlayer.sleep();
  }
  if (myCard.mode == 2) {
    if (track != numTracksInFolder) {
      track = track + 1;
      myDFPlayer.playFolder(myCard.folder, track);
      Serial.print(F("Album mode is active -> next track: "));
      Serial.print(track);
    } else
      myDFPlayer.sleep();
  }
  if (myCard.mode == 3) {
    track = random(1, numTracksInFolder + 1);
    Serial.print(F("Party mode is active -> play random track: "));
    Serial.println(track);
    myDFPlayer.playFolder(myCard.folder, track);
  }
  if (myCard.mode == 4) {
    Serial.println(F("Single mode active -> save power"));
    myDFPlayer.sleep();
  }
  if (myCard.mode == 5) {
    if (track != numTracksInFolder) {
      track = track + 1;
      Serial.print(F("Audiobook mode is active -> next track and save progress"));
      Serial.println(track);
      myDFPlayer.playFolder(myCard.folder, track);
      // Save progress in the EEPROM
      EEPROM.write(myCard.folder, track);
    } else
      myDFPlayer.sleep();
    // Reset progress
    EEPROM.write(myCard.folder, 1);
  }
  delay(500);
}

static void previousTrack() {
  if (myCard.mode == 1) {
    Serial.println(F("Radio play mode is active -> play track from the beginning"));
    myDFPlayer.playFolder(myCard.folder, track);
  }
  if (myCard.mode == 2) {
    Serial.println(F("Album mode is active -> Previous track"));
    if (track != 1) {
      track = track - 1;
    }
    myDFPlayer.playFolder(myCard.folder, track);
  }
  if (myCard.mode == 3) {
    Serial.println(F("Party Modus is active -> Play track from the beginning"));
    myDFPlayer.playFolder(myCard.folder, track);
  }
  if (myCard.mode == 4) {
    Serial.println(F("Single mode active -> Play track from the beginning"));
    myDFPlayer.playFolder(myCard.folder, track);
  }
  if (myCard.mode == 5) {
    Serial.println(F("Audiobook mode is active -> Next track and save progress"));
    if (track != 1) {
      track = track - 1;
    }
    myDFPlayer.playFolder(myCard.folder, track);
    // Save progress in the EEPROM
    EEPROM.write(myCard.folder, track);
  }
}

MFRC522::MIFARE_Key key;
bool successRead;
byte sector = 1;
byte blockAddr = 4;
byte trailerBlock = 7;
MFRC522::StatusCode status;

#define buttonPause 25
#define buttonUp 26
#define buttonDown 27
#define busyPin 4
#define headphonePin 32
#define dfpMute 33

#define LONG_PRESS 1000

Button pauseButton(buttonPause);
Button upButton(buttonUp, 100);
Button downButton(buttonDown, 100);
bool ignorePauseButton = false;
bool ignoreUpButton = false;
bool ignoreDownButton = false;

uint8_t numberOfCards = 0;

bool isPlaying() {
  return !digitalRead(busyPin);
}

//==========================================================================================
// Function to evaluate the answers of the HTML page
void handleRestart() {
  // Restart ESP
  ESP.restart();
}

void handleUpdate() {
  server.send(200, "text/html", UpdatePage());
}

void handleSetup() {
  server.send ( 200, "text/html", SetupPage());
  if (server.args() > 0 ) { // Arguments were received
    for ( uint8_t i = 0; i < server.args(); i++ ) {

      Serial.print("The server received the following: "); // Display the argument
      Serial.print(server.argName(i)); // Display the argument
      Serial.print("=");
      Serial.println(server.arg(i));

      if (server.argName(i) == "ssid" ) {
        Serial.print("saved SSID : '");
        Serial.println(server.arg(i));
        preferences.putString("SSID", server.arg(i));

      }

      else if (server.argName(i) == "pw") {
        Serial.print("saved PW : '");
        Serial.println(server.arg(i));
        preferences.putString("Password", server.arg(i));

      } else if (server.argName(i) == "hostname") {
        Serial.print("saved Hostname: ");
        Serial.println(server.arg(i));
        preferences.putString("Hostname", server.arg(i));

      }
    }
  }
}

void handleRoot() {
  server.send ( 200, "text/html", getPage() );
  if (server.args() > 0 ) { // Arguments were received
    for ( uint8_t i = 0; i < server.args(); i++ ) {

      Serial.print("The server received the following: "); // Display the argument
      Serial.print(server.argName(i)); // Display the argument
      Serial.print("=");
      Serial.println(server.arg(i));

      if (server.argName(i) == "appt-time-off" ) {
        TimerOFF = server.arg(i);
        char charBuf[TimerOFF.length() + 1];
        TimerOFF.toCharArray(charBuf, TimerOFF.length() + 1);
        TMR_OFF_HH = atof(strtok(charBuf, ":"));
        TMR_OFF_MM = atof(strtok(NULL, ":"));
        Serial.print("The time read in for TimerOFF: ");
        Serial.print(TMR_OFF_HH);
        Serial.print(":");
        Serial.println(TMR_OFF_MM);
        TMR_OFF_REP = 0;
        TMP_OFFTIME = false;
      }

      else if (server.argName(i) == "cb_tmr_off") {
        TMR_OFF_REP = 1;
        Serial.println("A repeat has been set for the TimerOFF");
      }

      else if (server.argName(i) == "appt-time-on") {
        TimerON = server.arg(i);
        char charBuf[TimerON.length() + 1];
        TimerON.toCharArray(charBuf, TimerON.length() + 1);
        TMR_ON_HH = atof(strtok(charBuf, ":"));
        TMR_ON_MM = atof(strtok(NULL, ":"));
        Serial.print("The time read in for TimerON: ");
        Serial.print(TMR_ON_HH);
        Serial.print(":");
        Serial.println(TMR_ON_MM);
        TMR_ON_REP = 0;
        TMP_ONTIME = false;
      }

      else if (server.argName(i) == "cb_tmr_on") {
        TMR_ON_REP = 1;
        Serial.println("A repetition has been set for the TimerON");
      }

      else if (server.argName(i) == "akt_volume") {
        myDFPlayer.volume(server.arg(i).toInt());
        akt_Volume = myDFPlayer.readVolume();
        Serial.println("The current volume level has been changed");
      }
      else if (server.argName(i) == "max_volume") {
        max_Volume = server.arg(i).toInt();
        Serial.println("The maximum volume level has been changed");
      }
      else if (server.argName(i) == "LED_color") {
        Serial.println("The color of the LEDs is changed: " + server.arg(i));
        String Color = server.arg(i);
        char *ptr;
        char charBuf[Color.length() + 1];
        Color.toCharArray(charBuf, Color.length() + 1);
        unsigned long col = strtol(charBuf, &ptr, 16);
        fill_solid(leds, NUM_LEDS, col); // Change color of all LEDs
        FastLED.show();
      }
      else if (server.argName(i) == "LED_bri") {
        Serial.println("The brightness of the LEDs is changed ");
        Serial.println("brightness = " + server.arg(i));
        FastLED.setBrightness(server.arg(i).toInt());
        FastLED.show();
      }
      else if (server.argName(i) == "cb_SleepLight_on") {
        //CODE HERE
        if (server.arg(i).toInt() == 1) {
          SleepLight = true;
        }
      }
      else if (server.argName(i) == "cb_SleepLight_off") {
        if (server.arg(i).toInt() == 0) {
          SleepLight = false;
        }
      }
      else if (server.argName(i) == "cb_WakeUpLight_on") {
        if (server.arg(i).toInt() == 1) {
          WakeUpLight = true;
        }

      }
      else if (server.argName(i) == "cb_WakeUpLight_off") {
        if (server.arg(i).toInt() == 0) {
          WakeUpLight = false;
        }
      }

    }
  }
}

//===================================================================================
// From here, actions which are triggered by pressing a button on the HTML page
void handlePrev() {
  Serial.println("handlePrev");
  myDFPlayer.previous();
  server.send(200, "text/html", getPage());
}

void handlePlay() {
  Serial.println("handlePlay");
  myDFPlayer.start();
  server.send(200, "text/html", getPage());
}

void handlePause() {
  Serial.println("handlePause");
  myDFPlayer.pause();
  server.send(200, "text/html", getPage());
}

void handleNext() {
  Serial.println("handleNext");
  myDFPlayer.next();
  server.send(200, "text/html", getPage());
}

void handleVol_up() {
  Serial.println("handleVol+");
  myDFPlayer.volumeUp();
  akt_Volume = myDFPlayer.readVolume();
  server.send(200, "text/html", getPage());
}

void handleVol_down() {
  Serial.println("handleVol-");
  myDFPlayer.volumeDown();
  akt_Volume = myDFPlayer.readVolume();
  server.send(200, "text/html", getPage());
}

void handleEQ_NORM() {
  Serial.println("handleEQ_Norm");
  myDFPlayer.EQ(DFPLAYER_EQ_NORMAL);
  server.send(200, "text/html", getPage());
}

void handleEQ_POP() {
  Serial.println("handleEQ_POP");
  myDFPlayer.EQ(DFPLAYER_EQ_POP);
  server.send(200, "text/html", getPage());
}

void handleEQ_ROCK() {
  Serial.println("handleEQ_ROCK");
  myDFPlayer.EQ(DFPLAYER_EQ_ROCK);
  server.send(200, "text/html", getPage());
}

void handleEQ_CLASSIC() {
  Serial.println("handleEQ_CLASSIC");
  myDFPlayer.EQ(DFPLAYER_EQ_CLASSIC);
  server.send(200, "text/html", getPage());
}

void handleEQ_BASS() {
  Serial.println("handleEQ_BASE");
  myDFPlayer.EQ(DFPLAYER_EQ_BASS);
  server.send(200, "text/html", getPage());
}

void handleEQ_JAZZ() {
  Serial.println("handleEQ_JAZZ");
  myDFPlayer.EQ(DFPLAYER_EQ_JAZZ);
  server.send(200, "text/html", getPage());
}

//==========================================================================================
// Function for the Sunrise simulation
void sunrise() {

  if (debug) Serial.println("sunrise() is running");
  CRGBPalette256 sunrisePal = sunrise_gp;
  CRGB color = ColorFromPalette(sunrisePal, heatIndex);
  // fill the entire strip with the current color
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
  heatIndex++;
  if (heatIndex == 255) {
    heatIndex = 0;
    startSR = false;
  }
}

//==========================================================================================
// Function to evaluate the on / off timer
void TimeCompare() {

  //if(debug) Serial.println("TimeCompare() is executed");
  timeClient.update();
  int NTP_HH = timeClient.getHours();
  int NTP_MM = timeClient.getMinutes();
  //Serial.println("The current NTP time:" + String(NTP_HH) +":"+ String(NTP_MM) );

  if ((TMR_OFF_MM == NTP_MM) and (TMP_OFFTIME == false) and (TMR_OFF_HH == NTP_HH)) {
    // Sleep Timer
    myDFPlayer.pause();
    delay(100);
    myDFPlayer.playMp3Folder(902); //Play goodbye
    //myDFPlayer.outputDevice(DFPLAYER_DEVICE_SLEEP);
    delay(10000);
    while (isPlaying())
      myDFPlayer.stop();
    if (TMR_OFF_REP == 0) {
      TMP_OFFTIME = true;
    }
    Serial.println("Playback was stopped by the OFF timer.");
  }

  else if ((TMR_ON_MM == NTP_MM) and (TMP_ONTIME == false) and (TMR_ON_HH == NTP_HH)) {
    myDFPlayer.playMp3Folder(903); // Play welcome
    //myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);
    if (TMR_ON_REP == 0) {
      TMP_ONTIME = true;
    }
    if (WakeUpLight == true) {
      startSR = true;
    }
    Serial.println("Playback was started by the ON timer.");
  }
}

//==========================================================================================
// WiFi Connection
//==========================================================================================
int WiFi_RouterNetworkConnect(char* txtSSID, char* txtPassword, char* txtHostname)
{
  int success = 1;

  // connect to WiFi network
  // see https://www.arduino.cc/en/Reference/WiFiBegin
  WiFi.begin(txtSSID, txtPassword);
  WiFi.setHostname(txtHostname);

  // we wait until connection is established
  // or 10 seconds are gone
  int WiFiConnectTimeOut = 0;
  while ((WiFi.status() != WL_CONNECTED) && (WiFiConnectTimeOut < 10))
  {
    delay(1000);
    WiFiConnectTimeOut++;
  }

  // not connected
  if (WiFi.status() != WL_CONNECTED)
  {
    success = -1;
  }

  // print out local address of ESP32 in Router network (LAN)
  Serial.println(WiFi.localIP());
  if (debug) Serial.print("WiFi Connect to AP");
  if (debug) Serial.println(String(success));
  return success;
}

// Disconnect from router network and return 1 (success) or -1 (no success)
int WiFi_RouterNetworkDisconnect()
{
  int success = -1;
  WiFi.disconnect();

  int WiFiConnectTimeOut = 0;
  while ((WiFi.status() == WL_CONNECTED) && (WiFiConnectTimeOut < 10))
  {
    delay(1000);
    WiFiConnectTimeOut++;
  }

  // not connected
  if (WiFi.status() != WL_CONNECTED)
  {
    success = 1;
  }
  Serial.println("Disconnected.");
  return success;
}

// Initialize Soft Access Point with ESP32
// ESP32 establishes its own WiFi network, one can choose the SSID
int WiFi_AccessPointStart(char* AccessPointNetworkSSID)
{
  WiFi.mode(WIFI_AP);
  IPAddress apIP(192, 168, 4, 1);    // Here IP is determined
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("TonUINO");  // Name  des Access Points
  delay(500);
  if (debug)  Serial.println("Starte AP");
  if (debug)  Serial.print("IP Adresse ");      //Output of current IP server
  if (debug)  Serial.println(WiFi.softAPIP());

  //Announcement that an access point is opened
  myDFPlayer.playMp3Folder(901);

  server.on("/", handleSetup);                // INI wifimanager Index Webseite send
  server.on("/restart", []() {                 // INI wifimanager Index Webseite send
    server.send(200, "text/plain", "ESP reset is done");
    handleRestart();
  });

  server.begin();
  if (debug)  Serial.println("HTTP Server started");
  while (1) {
    server.handleClient();                 // Will run endlessly so the WLAN setup can be done
    if (digitalRead(buttonPause) == 0)break; // Cancels the waiting loop as soon as the Play / Pause button has been pressed
  }

  return 1;
}

//==========================================================================================
// Setup
//==========================================================================================
void setup() {
  chipid = ESP.getEfuseMac();
  //======================ISR TIMER====================================================
  // Create semaphore to inform us when the timer has fired
  timerSemaphore = xSemaphoreCreateBinary();

  // Use 1st timer of 4 (counted from zero).
  // Set 80 divider for prescaler (see ESP32 Technical Reference Manual for more
  // info).
  timer = timerBegin(0, 80, true);

  // Attach onTimer function to our timer.
  timerAttachInterrupt(timer, &onTimer, true);

  // Set alarm to call onTimer function every second (value in microseconds).
  // Repeat the alarm (third parameter)
  timerAlarmWrite(timer, 1000000, true);

  //WS2812b Configure
  //===================================================================================
  // tell FastLED about the LED strip configuration
  FastLED.addLeds<WS2812B, DATA_PIN, RGB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);  // Adjust the brightness
  fill_solid(leds, NUM_LEDS, CRGB::Black); // Change color of all LEDs
  FastLED.show();

  //===================================================================================

  Serial.begin(115200);
  SPI.begin();
  mySoftwareSerial.begin(9600, SERIAL_8N1, 16, 17);  // speed, type, RX, TX
  randomSeed(analogRead(33)); // Initialize the random number generator
  mfrc522.PCD_Init();
  mfrc522.PCD_DumpVersionToSerial();

  pinMode(dfpMute, OUTPUT);
  digitalWrite(dfpMute, LOW);

  // Buttons with PullUp
  pinMode(headphonePin, INPUT_PULLUP);
  pinMode(buttonPause, INPUT_PULLUP);
  pinMode(buttonUp, INPUT_PULLUP);
  pinMode(buttonDown, INPUT_PULLUP);

  // Busy Pin
  pinMode(busyPin, INPUT);

  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  // RESET --- KEEP ALL THREE BUTTONS STARTED PRESS -> all known cards are deleted
  // PAUSE/UP/DOWN
  if (digitalRead(buttonPause) == LOW && digitalRead(buttonUp) == LOW &&
      digitalRead(buttonDown) == LOW) {
    Serial.println(F("Reset -> EEPROM is deleted"));
    for (int i = 0; i < EEPROM.length(); i++) {
      EEPROM.write(i, 0);
    }
  }

  Serial.println("TonUINO V3.2.0 of ESP32 Basis");
  Serial.println("Original V2.0: T. Voss, Extended V3.0: C. Ulbrich");

  Serial.println();
  Serial.println(F("DFRobot DFPlayer Mini"));
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));

  myDFPlayer.begin(mySoftwareSerial, false, true);
  
    if (!myDFPlayer.begin(mySoftwareSerial)) {  //Use softwareSerial to communicate with mp3.

     Serial.println(myDFPlayer.readType(),HEX);
     Serial.println(F("Unable to begin:"));
     Serial.println(F("1.Please recheck the connection!"));
     Serial.println(F("2.Please insert the SD card!"));
     while(true);
    }
    Serial.println(F("DFPlayer Mini online."));

  myDFPlayer.setTimeOut(500); //Set serial communictaion time out 500ms
  delay(100);
  //----Set volume----
  myDFPlayer.volume(15);  //Set volume value (0~30).
  //myDFPlayer.volumeUp(); //Volume Up
  //myDFPlayer.volumeDown(); //Volume Down
  delay(100);
  //----Set different EQ----
  myDFPlayer.EQ(DFPLAYER_EQ_NORMAL);
  //  myDFPlayer.EQ(DFPLAYER_EQ_POP);
  //  myDFPlayer.EQ(DFPLAYER_EQ_ROCK);
  //  myDFPlayer.EQ(DFPLAYER_EQ_JAZZ);
  //  myDFPlayer.EQ(DFPLAYER_EQ_CLASSIC);
  //  myDFPlayer.EQ(DFPLAYER_EQ_BASS);
  delay(100);
  //----Set device we use SD as default----
  //  myDFPlayer.outputDevice(DFPLAYER_DEVICE_U_DISK);
  myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);
  //  myDFPlayer.outputDevice(DFPLAYER_DEVICE_AUX);
  //  myDFPlayer.outputDevice(DFPLAYER_DEVICE_SLEEP);
  //  myDFPlayer.outputDevice(DFPLAYER_DEVICE_FLASH);

  //----Mp3 control----
  //  myDFPlayer.sleep();     //sleep
  //  myDFPlayer.reset();     //Reset the module
  //  myDFPlayer.enableDAC();  //Enable On-chip DAC
  //  myDFPlayer.disableDAC();  //Disable On-chip DAC
  //  myDFPlayer.outputSetting(true, 15); //output setting, enable the output and set the gain to 15

  //----Read imformation----
  Serial.println("");
  Serial.println(F("readState--------------------"));
  Serial.println(myDFPlayer.readState()); //read mp3 state
  Serial.println(F("readVolume--------------------"));
  Serial.println(myDFPlayer.readVolume()); //read current volume
  //Serial.println(F("readEQ--------------------"));
  //Serial.println(myDFPlayer.readEQ()); //read EQ setting
  Serial.println(F("readFileCounts--------------------"));
  Serial.println(myDFPlayer.readFileCounts()); //read all file counts in SD card
  Serial.println(F("readCurrentFileNumber--------------------"));
  Serial.println(myDFPlayer.readCurrentFileNumber()); //read current play file number
  Serial.println(F("readFileCountsInFolder--------------------"));
  Serial.println(myDFPlayer.readFileCountsInFolder(2)); //read fill counts in folder SD:/03
  Serial.println(F("--------------------"));
  delay(2000);

  //Play welcome reduced the time of wifi connect
  myDFPlayer.playMp3Folder(900);

  preferences.begin("my-wifi", false);
  if (debug)WiFi.mode(WIFI_AP_STA);
  // takeout 3 Strings out of the Non-volatile storage
  String strSSID = preferences.getString("SSID", "");
  String strPassword = preferences.getString("Password", "");
  String strHostname = preferences.getString("Hostname", String("TonUINO-" + String((uint32_t)chipid)));

  // convert it to char*
  char* txtSSID = const_cast<char*>(strSSID.c_str());
  char* txtPassword = const_cast<char*>(strPassword.c_str());   // https://coderwall.com/p/zfmwsg/arduino-string-to-char
  char* txtHostname = const_cast<char*>(strHostname.c_str());

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to SSID: ");
  Serial.print(txtSSID);
  Serial.print(" with the following PW:  ");
  Serial.print(txtPassword);
  Serial.print(" with the following Hostname:  ");
  Serial.println(txtHostname);

  // try to connect to the LAN
  success = WiFi_RouterNetworkConnect(txtSSID, txtPassword, txtHostname);
  if (success == 1) {
    fill_solid(leds, NUM_LEDS, CRGB::Blue); // Change color of all LEDs
    FastLED.show();
  } else {
    fill_solid(leds, NUM_LEDS, CRGB::Red); // Change color of all LEDs
    FastLED.show();
  }

  // Start access point"
  if (success == -1) {
    WiFi_AccessPointStart("ESP32_TonUINO");
  }

  Serial.println ( "HTTP server started" );

  // Start NTP client, set the offset to the received time
  if (success == 1)timeClient.begin();
  if (success == 1)timeClient.setTimeOffset(+3600); //+1h Offset
  if (success == 1)timeClient.update();

  // References for receiving HTML client information
  server.on ( "/", handleRoot );
  server.on ("/play", handlePlay);
  server.on ("/pause", handlePause);
  server.on ("/prev", handlePrev);
  server.on ("/next", handleNext);
  server.on ("/vol+", handleVol_up);
  server.on ("/vol-", handleVol_down);
  server.on ("/eq_base", handleEQ_BASS);
  server.on ("/eq_pop", handleEQ_POP);
  server.on ("/eq_rock", handleEQ_ROCK);
  server.on ("/eq_classic", handleEQ_CLASSIC);
  server.on ("/eq_jazz", handleEQ_JAZZ);
  server.on ("/eq_norm", handleEQ_NORM);
  server.on ("/setup", handleSetup);
  server.on ("/update", handleUpdate);

  /*handling uploading firmware file */
  server.on("/upload", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });

  server.begin();
  if (success == 1)startTimer();
  Serial.println ( "===============////////////// ================" );
  Serial.println ( "===============/ END SETUP  / ================" );
  Serial.println ( "===============/ //////////// ================" );
}
//==============END SETUP================================

//==========================================================================================
// Loop
//==========================================================================================
void loop() {
  do {

    if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE) { //Timer Interrupt Routine
      uint32_t isrCount = 0, isrTime = 0;
      // Read the interrupt count and time
      portENTER_CRITICAL(&timerMux);
      isrCount = isrCounter;
      if (isrRead == false) {
        isrRead = true;
        isrTime = millis();
      }
      portEXIT_CRITICAL(&timerMux);
      //From here functions for timers

      if (success == 1) TimeCompare(); //Query time in seconds
      if (startSR == true) sunrise();

    }
    server.handleClient();

    if (!isPlaying()) {
      if (myDFPlayer.available()) {
        printDetail(myDFPlayer.readType(), myDFPlayer.read()); //Print the detail message from DFPlayer to handle different errors and states.
      }
    }
    // Buttons are now traded via JS_Button, so that each key can be assigned twice
    pauseButton.read();
    upButton.read();
    downButton.read();

    // Detecting if a headphone is plugged in, "headphoneIn" locks in each case the query so that it will go through only once
    if ((digitalRead(headphonePin) == 1) && (headphoneIn == 0)) {

      Serial.println("Earphone was plugged in");
      digitalWrite(dfpMute, HIGH);
      headphoneIn = 1;
      last_max_Volume = max_Volume; // Remember the last maximum volume
      last_Volume = myDFPlayer.readVolume();
      max_Volume = 10;
      if (myDFPlayer.readVolume() >= max_Volume) {
        myDFPlayer.volume(10);
      }

    } else if ((digitalRead(headphonePin) == 0) && (headphoneIn == 1)) {
      Serial.println("Headphones have been removed");
      headphoneIn = 0;
      max_Volume = last_max_Volume;
      myDFPlayer.volume(last_Volume);
    }

    if (pauseButton.wasReleased()) {
      if (ignorePauseButton == false) {
        if (isPlaying()) {
          myDFPlayer.pause();
          fill_solid(leds, NUM_LEDS, CRGB::Black); // Change color of all LEDs
          FastLED.show();
          startSR = false;
          heatIndex = 0;
        } else {
          myDFPlayer.start();

          ignorePauseButton = false;
        }
      }
    } else if (pauseButton.pressedFor(LONG_PRESS) &&
               ignorePauseButton == false) {
      Serial.println(F("Pause button was pressed long"));
      if (isPlaying()) {
        myDFPlayer.advertise(track);
      }
      else {
        knownCard = false;
        myDFPlayer.playMp3Folder(800);
        Serial.println(F("Reset card..."));
        resetCard();
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
      }
      ignorePauseButton = true;
    }

    if (upButton.pressedFor(LONG_PRESS)) {
      Serial.println(F("Volume Up"));
      myDFPlayer.volumeUp();
      nextTrack();
      ignoreUpButton = true;
      delay(1000);
    } else if (upButton.wasReleased()) {
      if (!ignoreUpButton) {
        //nextTrack();
        if (myDFPlayer.readVolume() <= max_Volume) {
          myDFPlayer.volumeUp();
        }
      } else {
        ignoreUpButton = false;
      }
    }

    if (downButton.pressedFor(LONG_PRESS)) {
      Serial.println(F("Volume Down"));
      myDFPlayer.volumeDown();
      previousTrack();
      ignoreDownButton = true;
      delay(1000);
    } else if (downButton.wasReleased()) {
      if (!ignoreDownButton)
        //previousTrack();
        myDFPlayer.volumeDown();
      else
        ignoreDownButton = false;
    }

    // End of the buttons
  } while (!mfrc522.PICC_IsNewCardPresent());

  // RFID card was launched

  if (!mfrc522.PICC_ReadCardSerial())
    return;

  if (readCard(&myCard) == true) {
    if (myCard.cookie == 322417479 && myCard.folder != 0 && myCard.mode != 0) {

      knownCard = true;
      numTracksInFolder = myDFPlayer.readFileCountsInFolder(myCard.folder);

      // Radio play mode: a random file from the folder
      if (myCard.mode == 1) {
        Serial.println(F("Radio play mode -> play random track"));
        track = random(1, numTracksInFolder + 1);
        Serial.println(track);
        myDFPlayer.playFolder(myCard.folder, track);
        fill_solid(leds, NUM_LEDS, myCard.color); // Change color of all LEDs
        FastLED.show();
      }
      // Album mode: play complete folder
      if (myCard.mode == 2) {
        Serial.println(F("Album mode -> play complete folder"));
        track = 1;
        myDFPlayer.playFolder(myCard.folder, track);
        fill_solid(leds, NUM_LEDS, myCard.color); // Change color of all LEDs
        FastLED.show();
      }
      // Party Mode: Folder in random order
      if (myCard.mode == 3) {
        Serial.println(
          F("Party Mode -> play folders in random order"));
        track = random(1, numTracksInFolder + 1);
        myDFPlayer.playFolder(myCard.folder, track);
        fill_solid(leds, NUM_LEDS, myCard.color); // Change color of all LEDs
      }
      // Single mode: play a file from the folder
      if (myCard.mode == 4) {
        Serial.println(
          F("Single mode -> play a file from the folder"));
        track = myCard.special;
        myDFPlayer.playFolder(myCard.folder, track);
        fill_solid(leds, NUM_LEDS, myCard.color); // Change color of all LEDs
        FastLED.show();
      }
      // Audiobook mode: play complete folder and remember progress
      if (myCard.mode == 5) {
        Serial.println(F("Audiobook mode -> play complete folder and remember progress"));
        track = EEPROM.read(myCard.folder);
        if (track == 0)track = 1;
        myDFPlayer.playFolder(myCard.folder, track);
        fill_solid(leds, NUM_LEDS, myCard.color); // Change color of all LEDs
        FastLED.show();
      }
    }

    // New card configured
    else {
      knownCard = false;
      setupCard();
    }
  }
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}
//==============END LOOP================================


//==========================================================================================
// Function voiceMenu 
int voiceMenu(int numberOfOptions, int startMessage, int messageOffset,
              bool preview , int previewFromFolder) {
  int returnValue = 0;

  Serial.println(F("Will play file no.:"));
  Serial.println(startMessage);

  if (startMessage != 0) {
    myDFPlayer.playMp3Folder(startMessage);
    myDFPlayer.playMp3Folder(startMessage);
  }
  do {
    pauseButton.read();
    upButton.read();
    downButton.read();
    //myDFPlayer.loop();
    if (pauseButton.wasPressed()) {
      if (returnValue != 0)
        return returnValue;
      delay(1000);
    }

    if (upButton.pressedFor(LONG_PRESS)) {
      returnValue = fmin(returnValue + 10, numberOfOptions);
      myDFPlayer.playMp3Folder(messageOffset + returnValue);
      delay(1000);
      if (preview) {
        do {
          delay(10);
        } while (isPlaying());
        if (previewFromFolder == 0)
          myDFPlayer.playFolder(returnValue, 1);
        else
          myDFPlayer.playFolder(previewFromFolder, returnValue);
      }
      ignoreUpButton = true;
    } else if (upButton.wasReleased()) {
      if (!ignoreUpButton) {
        returnValue = fmin(returnValue + 1, numberOfOptions);
        myDFPlayer.playMp3Folder(messageOffset + returnValue);
        delay(1000);
        if (preview) {
          do {
            delay(10);
          } while (isPlaying());
          if (previewFromFolder == 0)
            myDFPlayer.playFolder(returnValue, 1);
          else
            myDFPlayer.playFolder(previewFromFolder, returnValue);
        }
      } else
        ignoreUpButton = false;
    }

    if (downButton.pressedFor(LONG_PRESS)) {
      returnValue = fmax(returnValue - 10, 1);
      myDFPlayer.playMp3Folder(messageOffset + returnValue);
      delay(1000);
      if (preview) {
        do {
          delay(10);
        } while (isPlaying());
        if (previewFromFolder == 0)
          myDFPlayer.playFolder(returnValue, 1);
        else
          myDFPlayer.playFolder(previewFromFolder, returnValue);
      }
      ignoreDownButton = true;
    } else if (downButton.wasReleased()) {
      if (!ignoreDownButton) {
        returnValue = fmax(returnValue - 1, 1);
        myDFPlayer.playMp3Folder(messageOffset + returnValue);
        delay(1000);
        if (preview) {
          do {
            delay(10);
          } while (isPlaying());
          if (previewFromFolder == 0)
            myDFPlayer.playFolder(returnValue, 1);
          else
            myDFPlayer.playFolder(previewFromFolder, returnValue);
        }
      } else
        ignoreDownButton = false;
    }
  } while (true);
}

//==========================================================================================
// Function resetCard
void resetCard() {
  do {
    pauseButton.read();
    upButton.read();
    downButton.read();

    if (upButton.wasReleased() || downButton.wasReleased()) {
      Serial.print(F("Canceled!"));
      myDFPlayer.playMp3Folder(802);
      return;
    }
  } while (!mfrc522.PICC_IsNewCardPresent());

  if (!mfrc522.PICC_ReadCardSerial())
    return;

  Serial.print(F("Card is reconfigured!"));
  setupCard();
}

//==========================================================================================
// Function setupCard
void setupCard() {
  myDFPlayer.pause();
  Serial.print(F("New card configured"));

  // Query folder
  myCard.folder = voiceMenu(99, 300, 0, true);

  // Query playback mode
  myCard.mode = voiceMenu(6, 310, 310);


  // Interrogate color
  myCard.color = voiceMenu(7, 600, 600);
  switch (myCard.color) {
    case 4:
      myCard.color = CRGB::LawnGreen;
      break;
    case 3:
      myCard.color = CRGB::Yellow;
      break;
    case 6:
      myCard.color = CRGB::White;
      break;
    case 7:
      myCard.color = CRGB::Plum;
      break;
    case 2:
      myCard.color = CRGB::OrangeRed;
      break;
    case 5:
      myCard.color = CRGB::LightSkyBlue;
      break;
    case 1:
      myCard.color = CRGB::Black;
      break;
  }


  // Audiobook Mode -> Set progress in EEPROM to 1
  EEPROM.write(myCard.folder, 1);

  // Single mode -> Query file
  if (myCard.mode == 4)
    myCard.special = voiceMenu(myDFPlayer.readFileCountsInFolder(myCard.folder), 320, 0,
                               true, myCard.folder);

  // Admin Function
  if (myCard.mode == 6)
    myCard.special = voiceMenu(3, 316, 320);

  // Card is configured -> save
  writeCard(myCard);
}

//==========================================================================================
// Function readCard
bool readCard(nfcTagObject *nfcTag) {
  bool returnValue = true;
  // Show some details of the PICC (that is: the tag/card)
  Serial.print(F("Card UID:"));
  dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
  Serial.println();
  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  Serial.println(mfrc522.PICC_GetTypeName(piccType));

  byte buffer[18];
  byte size = sizeof(buffer);

  // Authenticate using key A
  Serial.println(F("Authenticating using key A..."));
  status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(
             MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    returnValue = false;
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return returnValue;
  }

  // Show the whole sector as it currently is
  Serial.println(F("Current data in sector:"));
  mfrc522.PICC_DumpMifareClassicSectorToSerial(&(mfrc522.uid), &key, sector);
  Serial.println();

  // Read data from the block
  Serial.print(F("Reading data from block "));
  Serial.print(blockAddr);
  Serial.println(F(" ..."));
  status = (MFRC522::StatusCode)mfrc522.MIFARE_Read(blockAddr, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
    returnValue = false;
    Serial.print(F("MIFARE_Read() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
  }
  Serial.print(F("Data in block "));
  Serial.print(blockAddr);
  Serial.println(F(":"));
  dump_byte_array(buffer, 20);
  Serial.println();
  Serial.println();

  uint32_t tempCookie;
  tempCookie = (uint32_t)buffer[0] << 24;
  tempCookie += (uint32_t)buffer[1] << 16;
  tempCookie += (uint32_t)buffer[2] << 8;
  tempCookie += (uint32_t)buffer[3];

  uint32_t tempColor;
  tempColor = (uint32_t)buffer[8] << 24;
  tempColor += (uint32_t)buffer[9] << 16;
  tempColor += (uint32_t)buffer[10] << 8;
  tempColor += (uint32_t)buffer[11];

  nfcTag->cookie = tempCookie;
  nfcTag->version = buffer[4];
  nfcTag->folder = buffer[5];
  nfcTag->mode = buffer[6];
  nfcTag->special = buffer[7];
  nfcTag->color = tempColor;

  return returnValue;
}

//==========================================================================================
// Function writeCard
void writeCard(nfcTagObject nfcTag) {
  MFRC522::PICC_Type mifareType;

  uint8_t bytes[4];

  bytes[0] = (nfcTag.color >> 0)  & 0xFF;
  bytes[1] = (nfcTag.color >> 8)  & 0xFF;
  bytes[2] = (nfcTag.color >> 16) & 0xFF;
  bytes[3] = (nfcTag.color >> 24) & 0xFF;

  byte buffer[20] = {0x13, 0x37, 0xb3, 0x47, // 0x1337 0xb347 magic cookie to
                     // identify our nfc tags
                     0x01,                   // version 1
                     nfcTag.folder,          // the folder picked by the user
                     nfcTag.mode,    // the playback mode picked by the user
                     nfcTag.special, // track or function for admin cards
                     bytes[3],  //Color to be switched on
                     bytes[2],  //Color to be switched on
                     bytes[1],  //Color to be switched on
                     bytes[0],  //Color to be switched on
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                    };

  byte size = sizeof(buffer);

  mifareType = mfrc522.PICC_GetType(mfrc522.uid.sak);

  // Authenticate using key B
  Serial.println(F("Authenticating again using key B..."));
  status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(
             MFRC522::PICC_CMD_MF_AUTH_KEY_B, trailerBlock, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    myDFPlayer.playMp3Folder(401);
    return;
  }

  // Write data to the block
  Serial.print(F("Writing data into block "));
  Serial.print(blockAddr);
  Serial.println(F(" ..."));
  dump_byte_array(buffer, 20);
  Serial.println();
  status = (MFRC522::StatusCode)mfrc522.MIFARE_Write(blockAddr, buffer, 16);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("MIFARE_Write() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    myDFPlayer.playMp3Folder(401);
  }
  else
    myDFPlayer.playMp3Folder(400);
  Serial.println();
  delay(100);
}

//==========================================================================================
// Function dump_byte_array
/**
   Helper routine to dump a byte array as hex values to Serial.
*/
void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

//==========================================================================================
// Function startTimer
void startTimer() {
  // Start an alarm
  timerAlarmEnable(timer);
}

//==========================================================================================
// Function stoppTimer
void stoppTimer() {
  timerEnd(timer);
  timer = NULL;
}

//==========================================================================================
// Function printDetail : Print the detail message from DFPlayer
void printDetail(uint8_t type, int value) {
  switch (type) {
    case TimeOut:
      Serial.println(F("Time Out!"));
      break;
    case WrongStack:
      Serial.println(F("Stack Wrong!"));
      break;
    case DFPlayerCardInserted:
      Serial.println(F("Card Inserted!"));
      break;
    case DFPlayerCardRemoved:
      Serial.println(F("Card Removed!"));
      break;
    case DFPlayerCardOnline:
      Serial.println(F("Card Online!"));
      break;
    case DFPlayerPlayFinished:
      Serial.print(F("Number:"));
      Serial.print(value);
      Serial.println(F(" Play Finished!"));
      Mp3Notify::OnPlayFinished(track);
      break;
    case DFPlayerError:
      Serial.print(F("DFPlayerError:"));
      switch (value) {
        case Busy:
          Serial.println(F("Card not found"));
          break;
        case Sleeping:
          Serial.println(F("Sleeping"));
          break;
        case SerialWrongStack:
          Serial.println(F("Get Wrong Stack"));
          break;
        case CheckSumNotMatch:
          Serial.println(F("Check Sum Not Match"));
          break;
        case FileIndexOut:
          Serial.println(F("File Index Out of Bound"));
          break;
        case FileMismatch:
          Serial.println(F("Cannot Find File"));
          break;
        case Advertise:
          Serial.println(F("In Advertise"));
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}

