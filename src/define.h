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

// KY 040
//========================================================================
#define PIN_A   26 //ky-040 clk pin, interrupt & add 100nF/0.1uF capacitors between pin & ground!!!
#define PIN_B   25 //ky-040 dt  pin,             add 100nF/0.1uF capacitors between pin & ground!!!
#define BUTTON  27 //ky-040 sw  pin, interrupt & add 100nF/0.1uF capacitors between pin & ground!!!

//http://hobbycomponents.com/images/forum/Wemos_Lolin_D32_Diagram_HCWEMO0014.png
#define GND_SWI_PIN GPIO_NUM_12

int max_Volume = 29;

#define DEEPSLEEP_WAKEUP_PIN GPIO_NUM_14

#define AUTOSLEEP_TIME 420000 //900000UL //15min

bool isplaying=false;

const char* PARAM_MESSAGE = "folder";