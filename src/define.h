//define-files 
// version 3.1.1 by M.Schwager 2019-08-21
//
/*
struct card {
  int folder=0;
  int track=0;
  uint32_t cardID=0;
  int numTracksInFolder=0;
};

union ArrayToInteger {
  byte array[4];
 uint32_t integer;
};
*/

// KY 040
//========================================================================
// #define PIN_A   26 //ky-040 clk pin, interrupt & add 100nF/0.1uF capacitors between pin & ground!!!
// #define PIN_B   25 //ky-040 dt  pin,             add 100nF/0.1uF capacitors between pin & ground!!!
// #define BUTTON  27 //ky-040 sw  pin, interrupt & add 100nF/0.1uF capacitors between pin & ground!!!

// normal Buttons
#define buttonPause 25
#define buttonUp 26
#define buttonDown 27
// #define buttonNext 32
// #define buttonLast 33

#define LONG_PRESS 1000

//WS2812b settings
#define DATA_PIN 2        // signal pin 
#define NUM_LEDS 24       // number of LEDs in your strip
#define BRIGHTNESS 32     // brightness  (max 254) 
#define LED_TYPE WS2812B  // I know we are using ws2812, but it's ok!
#define COLOR_ORDER GRB   // Datasheet WS2812 : "Follow the order of GRB to sent data"
#define OK_COLOR CRGB::Green
#define KO_COLOR CRGB::Red
#define WARNING_COLOR CRGB::Orange

//Set Pins for Dfplayer Mini Module
#define busyPin 4
#define headphonePin 32
#define dfpMute 33
#define RX_PIN 16
#define TX_PIN 17

//Set Pins for RC522 Module
#define RESET_PIN 22     // Reset pin 
#define SS_PIN 21        // Slave select pin 

// ================================================
// Parameter
// ================================================
#define GND_SWI_PIN GPIO_NUM_12

//Variable for reading the HTML response
String TimerOFF = "00:00";
String TimerON = "00:00";
uint8_t TMR_OFF_HH, TMR_OFF_MM, TMR_ON_HH, TMR_ON_MM;
int TMR_OFF_REP = 0;
int TMR_ON_REP = 0;
unsigned int max_Volume = 40;
unsigned int akt_Volume = 10;
unsigned int init_Volume = 12; //Set volume value (0~30).
unsigned int max_Volume_headphone = 35;

bool TMP_OFFTIME = false;
bool TMP_ONTIME = false;
bool WakeUpLight = false;
bool SleepLight = false;
bool startSR = false;        // Bool to start Sunrise simulation

#define DEEPSLEEP_WAKEUP_PIN GPIO_NUM_14

#define AUTOSLEEP_TIME 420000 //900000UL //15min

const char* PARAM_MESSAGE = "folder";