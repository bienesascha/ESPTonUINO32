
#include "define.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiMulti.h>
#include "DFRobotDFPlayerMini.h"
#include <ArduinoJson.h>
#include <SPI.h>        // RC522 Module uses SPI protocol
#include <MFRC522.h>  // Library for Mifare RC522 Devices
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
#include "SPIFFS.h"
#include "myTimer.h"
#include <RotaryEncoder.h>


void nextTrack();

WiFiMulti wifiMulti;

card myCard;

ArrayToInteger converter; //Create a converter

myTimer timer;

int16_t position = 0;

RotaryEncoder encoder(PIN_A, PIN_B, BUTTON);





void encoderISR()
{
  encoder.readAB();
}

void encoderButtonISR()
{
  encoder.readPushButton();
}

//= myDFPlayer.readFileCountsInFolder(myCard.folder);
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

//Set Pins for RC522 Module
const int resetPin = 22; // Reset pin
const int ssPin = 21;    // Slave select pin

//Created object for communication with the module
MFRC522 mfrc522 = MFRC522(ssPin, resetPin); // Create instance
byte readCard[4];   // Stores scanned ID read from RFID Module

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");


HardwareSerial mySoftwareSerial(2);
DFRobotDFPlayerMini myDFPlayer;

const size_t capacity = JSON_OBJECT_SIZE(40) + 20;
DynamicJsonDocument doc(capacity);



void WiFiEvent(WiFiEvent_t event)
{
    Serial.printf("[WiFi-event] event: %d\n", event);

    switch (event) {
        case SYSTEM_EVENT_WIFI_READY: 
            Serial.println("WiFi interface ready");
            break;
        case SYSTEM_EVENT_SCAN_DONE:
            Serial.println("Completed scan for access points");
            break;
        case SYSTEM_EVENT_STA_START:
            Serial.println("WiFi client started");
            break;
        case SYSTEM_EVENT_STA_STOP:
            Serial.println("WiFi clients stopped");
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            Serial.println("Connected to access point");
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            Serial.println("Disconnected from WiFi access point");
            break;
        case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
            Serial.println("Authentication mode of access point has changed");
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            Serial.print("Obtained IP address: ");
            Serial.println(WiFi.localIP());
            break;
        case SYSTEM_EVENT_STA_LOST_IP:
            Serial.println("Lost IP address and IP address is reset to 0");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
            Serial.println("WiFi Protected Setup (WPS): succeeded in enrollee mode");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_FAILED:
            Serial.println("WiFi Protected Setup (WPS): failed in enrollee mode");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
            Serial.println("WiFi Protected Setup (WPS): timeout in enrollee mode");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_PIN:
            Serial.println("WiFi Protected Setup (WPS): pin code in enrollee mode");
            break;
        case SYSTEM_EVENT_AP_START:
            Serial.println("WiFi access point started");
            break;
        case SYSTEM_EVENT_AP_STOP:
            Serial.println("WiFi access point  stopped");
            break;
        case SYSTEM_EVENT_AP_STACONNECTED:
            Serial.println("Client connected");
            break;
        case SYSTEM_EVENT_AP_STADISCONNECTED:
            Serial.println("Client disconnected");
            break;
        case SYSTEM_EVENT_AP_STAIPASSIGNED:
            Serial.println("Assigned IP address to client");
            break;
        case SYSTEM_EVENT_AP_PROBEREQRECVED:
            Serial.println("Received probe request");
            break;
        case SYSTEM_EVENT_GOT_IP6:
            Serial.println("IPv6 is preferred");
            break;
        case SYSTEM_EVENT_ETH_START:
            Serial.println("Ethernet started");
            break;
        case SYSTEM_EVENT_ETH_STOP:
            Serial.println("Ethernet stopped");
            break;
        case SYSTEM_EVENT_ETH_CONNECTED:
            Serial.println("Ethernet connected");
            break;
        case SYSTEM_EVENT_ETH_DISCONNECTED:
            Serial.println("Ethernet disconnected");
            break;
        case SYSTEM_EVENT_ETH_GOT_IP:
            Serial.println("Obtained IP address");
            Serial.println("IP address: ");
            Serial.println(WiFi.localIP());
            break;
    }
  }



void loadJSON(){
    //const char* json = "{\"123798\":1,\"484856\":2}";
    const char *filename = "/config.txt";  // <- SD library uses 8.3 filenames
    File file = SPIFFS.open(filename);
    DeserializationError error=deserializeJson(doc, file);
    if (error)
        Serial.println(F("Failed to read file, using default configuration"));

    file.close();
    //deserializeJson(doc, json);
}

void nextTrack(){
    if(myCard.track<myCard.numTracksInFolder){
        myCard.track=myCard.track+1;
        Serial.print("Folder: ");Serial.print(myCard.folder);
        Serial.print(" - Track: ");Serial.print(myCard.track);Serial.print("/");Serial.println(myCard.numTracksInFolder);
        delay(100);
        timer.set();
        isplaying=true;
        myDFPlayer.playFolder(myCard.folder, myCard.track);
    }
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
    client->printf("Hello Client %u :)", client->id());
    client->ping();
  } else if(type == WS_EVT_DISCONNECT){
    Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  } else if(type == WS_EVT_ERROR){
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_PONG){
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
  } else if(type == WS_EVT_DATA){
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    String msg = "";
    if(info->final && info->index == 0 && info->len == len){
      //the whole message is in a single frame and we got all of it's data
      Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);

      if(info->opcode == WS_TEXT){
        for(size_t i=0; i < info->len; i++) {
          msg += (char) data[i];
        }
      } else {
        char buff[3];
        for(size_t i=0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }
      Serial.printf("%s\n",msg.c_str());

      if(info->opcode == WS_TEXT)
        client->text("I got your text message");
      else
        client->binary("I got your binary message");
    } else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if(info->index == 0){
        if(info->num == 0)
          Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);

      if(info->opcode == WS_TEXT){
        for(size_t i=0; i < info->len; i++) {
          msg += (char) data[i];
        }
      } else {
        char buff[3];
        for(size_t i=0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }
      Serial.printf("%s\n",msg.c_str());

      if((info->index + len) == info->len){
        Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if(info->final){
          Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
          if(info->message_opcode == WS_TEXT)
            client->text("I got your text message");
          else
            client->binary("I got your binary message");
        }
      }
    }
  }
}

String getIndexHTML(){
  String ret = F("<!DOCTYPE html><html><body><h2><a href='");
  //ret+=WiFi.localIP();
  ret+=F("/edit'>Edit Pages</a></h2></body></html>");
  return ret;
}

void SpiffServerSetup(){
    SPIFFS.begin();

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  events.onConnect([](AsyncEventSourceClient *client){
    client->send("hello!",NULL,millis(),1000);
  });
  server.addHandler(&events);

  server.addHandler(new SPIFFSEditor(SPIFFS,"",""));

  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  server.on("/play", HTTP_GET, [](AsyncWebServerRequest *request){
    myDFPlayer.play();
    request->send(200, "text/plain", "PLAY");
  });

server.on("/pause", HTTP_GET, [](AsyncWebServerRequest *request){
    myDFPlayer.pause();
    request->send(200, "text/plain", "PAUSE");
  });

  server.on("/sleep", HTTP_GET, [](AsyncWebServerRequest *request){
    //myDFPlayer.sleep();
    //delay(100);
    //esp_deep_sleep_start();
    request->send(200, "text/plain", "SLEEP");
  });

    server.on("/playfolder", HTTP_GET, [] (AsyncWebServerRequest *request) {
        String message;
        if (request->hasParam(PARAM_MESSAGE)) {
            message = request->getParam(PARAM_MESSAGE)->value();
            myDFPlayer.playFolder(message.toInt(), 1);
        }
        request->send(200, "text/plain", "Hello, GET: " + message);
    });

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", getIndexHTML());
  });
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.htm");

  server.onNotFound([](AsyncWebServerRequest *request){
    Serial.printf("NOT_FOUND: ");
    if(request->method() == HTTP_GET)
      Serial.printf("GET");
    else if(request->method() == HTTP_POST)
      Serial.printf("POST");
    else if(request->method() == HTTP_DELETE)
      Serial.printf("DELETE");
    else if(request->method() == HTTP_PUT)
      Serial.printf("PUT");
    else if(request->method() == HTTP_PATCH)
      Serial.printf("PATCH");
    else if(request->method() == HTTP_HEAD)
      Serial.printf("HEAD");
    else if(request->method() == HTTP_OPTIONS)
      Serial.printf("OPTIONS");
    else
      Serial.printf("UNKNOWN");
    Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

    if(request->contentLength()){
      Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
      Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
    }

    int headers = request->headers();
    int i;
    for(i=0;i<headers;i++){
      AsyncWebHeader* h = request->getHeader(i);
      Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
    for(i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isFile()){
        Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      } else if(p->isPost()){
        Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      } else {
        Serial.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }

    request->send(404);
  });
  server.onFileUpload([](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index)
      Serial.printf("UploadStart: %s\n", filename.c_str());
    Serial.printf("%s", (const char*)data);
    if(final)
      Serial.printf("UploadEnd: %s (%u)\n", filename.c_str(), index+len);
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    if(!index)
      Serial.printf("BodyStart: %u\n", total);
    Serial.printf("%s", (const char*)data);
    if(index + len == total)
      Serial.printf("BodyEnd: %u\n", total);
  });
  server.begin();
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
      Mp3Notify::OnPlayFinished(myCard.track);
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


///////////////////////////////////////// Get PICC's UID ///////////////////////////////////
uint8_t getID() {
  // Getting ready for Reading PICCs
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {   //Since a PICC placed get Serial and continue
    return 0;
  }
  // There are Mifare PICCs which have 4 byte or 7 byte UID care if you use 7 byte PICC
  // I think we should assume every PICC as they have 4 byte UID
  // Until we support 7 byte PICCs
  Serial.println(F("Scanned PICC's UID:"));
  for ( uint8_t i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}

bool playCard(int id){
    bool ret=false;
    myCard.cardID=id;
    myCard.folder=doc[String(id)];
    //Serial.print(F("Play Folder: "));
    //Serial.println(myCard.folder);
    if(myCard.folder>0){ //card exists
            myCard.numTracksInFolder=myDFPlayer.readFileCountsInFolder(myCard.folder);
            myCard.track=0;
            nextTrack();
            ret=true;
    }
    return ret;
}

void DFPlayerSetup(){
      mySoftwareSerial.begin(9600, SERIAL_8N1, 16, 17);  // speed, type, RX, TX
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

  // //----Read imformation----
  // Serial.println("");
  // Serial.print(F("readState: "));
  // Serial.println(myDFPlayer.readState()); //read mp3 state
  // Serial.print(F("readVolume: "));
  // Serial.println(myDFPlayer.readVolume()); //read current volume
  // //Serial.println(F("readEQ--------------------"));
  // //Serial.println(myDFPlayer.readEQ()); //read EQ setting
  // Serial.print(F("readFileCounts: "));
  // Serial.println(myDFPlayer.readFileCounts()); //read all file counts in SD card
  // Serial.print(F("readCurrentFileNumber: "));
  // Serial.println(myDFPlayer.readCurrentFileNumber()); //read current play file number
  // Serial.print(F("readFileCountsInFolder: "));
  // Serial.println(myDFPlayer.readFileCountsInFolder(2)); //read fill counts in folder SD:/03
  // Serial.println(F("--------------------"));
  // delay(2000);
  
}

void WIFISetup(){

    WiFi.onEvent(WiFiEvent);
    
    //wifiMulti.addAP("ssid_from_AP_2", "your_password_for_AP_2");
    //wifiMulti.addAP("ssid_from_AP_3", "your_password_for_AP_3");
    
    Serial.println("Connecting Wifi...");
    // if(wifiMulti.run() == WL_CONNECTED) {
    //     Serial.println("");
    //     Serial.println("WiFi connected");
    //     Serial.println("IP address: ");
    //     Serial.println(WiFi.localIP());
    //     }

}

void OTASetup(){
    // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  // ArduinoOTA.setHostname("myesp32");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
}

void setupNFC(){
  SPI.begin();
   mfrc522.PCD_Init();
  mfrc522.PCD_DumpVersionToSerial();
}

/****************************************************************
Method to print the reason by which ESP32
has been awaken from sleep
*/
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}
//***************************************************************

void deepSleepSETUP(){
   timer.set_max_delay(AUTOSLEEP_TIME);
   timer.set();
    //esp_sleep_enable_ext0_wakeup(DEEPSLEEP_WAKEUP_PIN,1); //1 = High, 0 = Low
}


void setupKY040(){
  encoder.begin();                                                           //set encoders pins as input & enable built-in pullup resistors

  attachInterrupt(digitalPinToInterrupt(PIN_A),  encoderISR,       CHANGE);  //call encoderISR()       every high->low or low->high changes
  attachInterrupt(digitalPinToInterrupt(BUTTON), encoderButtonISR, FALLING); //call encoderButtonISR() every high->low              changes
}
void setup() {
    Serial.begin(115200);
    Serial.println("Booting");
    print_wakeup_reason();  
    setupKY040();
    WIFISetup();
    SpiffServerSetup();
    loadJSON();
    DFPlayerSetup();
    setupNFC();
    OTASetup();
    deepSleepSETUP();
    // myDFPlayer.volume(15);  //Set volume value (0~30).
    // delay(50);
    // Serial.println(myDFPlayer.readVolume());
    myDFPlayer.playMp3Folder(400);
    for(int i=0;i<20;i++){delay(100);}
    
    Serial.println("Setup finished");
    
}


void loop() {
    bool knowncard=false;
    if(getID()){
        converter.array[0]=readCard[0];converter.array[1]=readCard[1];converter.array[2]=readCard[2];converter.array[3]=readCard[3];
        if(myCard.cardID!=converter.integer){
            myCard.cardID=converter.integer;
            Serial.println("Detected Card");Serial.println(myCard.cardID);
            knowncard=playCard(myCard.cardID);
        }
        if(!knowncard){
            //play unknown Card
            Serial.print("unknown CARD: ");
            myDFPlayer.playMp3Folder(300);
            for(int i=0;i<21;i++){delay(100);}
            Serial.println(converter.integer);
            int n=converter.integer;
            char digits[15];
            int c=0;
            while(n>0 && c< 15){
              digits[++c]=n%10;
              n/=10;
            }
            digits[++c]=n;
            while(c>1){
              int dig=digits[--c];
              myDFPlayer.playMp3Folder(dig);
              for(int i=0;i<10;i++){delay(100);}
              Serial.println(dig);
            } 
            //myDFPlayer.playMp3Folder(1);
        }
    }
    if (myDFPlayer.available()) {
        printDetail(myDFPlayer.readType(), myDFPlayer.read()); //Print the detail message from DFPlayer to handle different errors and states.
    }
    if(timer.check()){
      Serial.println("*******************************");
      Serial.println("Timer abgelaufen, DEEP SLEEP: ");
      Serial.println("*******************************");
      //myDFPlayer.sleep();
      //delay(100);
      //esp_deep_sleep_start();
    }

    if (position < encoder.getPosition())
    {
      //Serial.println(position);
      myDFPlayer.volumeDown();
    }
    if (position > encoder.getPosition())
    {
      //Serial.println(position);
      myDFPlayer.volumeUp();
    }
    if(position!=encoder.getPosition()){
      position=encoder.getPosition();
    }
    if (encoder.getPushButton() == true){
      isplaying?myDFPlayer.pause():myDFPlayer.start();
      isplaying=!isplaying;
      //Serial.println(F("PRESSED"));         //(F()) saves string to flash & keeps dynamic memory free
    }
    ArduinoOTA.handle();
}