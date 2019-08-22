# ESPTonUINO32
*ESPTonUINO32 V3.1.1 by M.Schwager*

- orginal [TonUINO-ESP32](http://discourse.voss.earth/t/esp32-port-inkl-webinterface/399)
_[TonUINO V2.0](https://www.voss.earth/tonuino/) Code for ESP32 with extensions, there is also [Discourse Seite](http://discourse.voss.earth/)_
- TonUNI-ESP32 V3.1 by C.Ulbrich, 

## *actual under development*
 - additional buttons
   - seperate buttons for next/last track / 5 button control

 - SDCard files  
   - Language menu and structure expanded to Tag to save Ambilight color
   - additional files added
   - sorting mp3 files

 - electric
   - actual schema
   - actual layout

 - 3d housing

**Currently implemented:**
  
  - Web interface for remote control
    - Play, Pause, Start, Stop, Volume
    - Adjust equalizer
    - max. Adjust Volume
    - Off timer
    - On timer
    
  - Ambientlight with WS2812 LEDss
    - change the color via web interface
    
  - Timer for switching on and off
    - Sunrise and sunset simulation for switch-on timer
    - MP3 playback when switching on / off
    
  - AP mode to configure WLAN without anchoring this (SSID, PW) in the code
    - Voice message if there is an error with the connection
   
  - Welcome at startup 
   
  - Headphone detection
    - reduces the volume when headphones plugged
    - Limit the volume when headphones are plugged
    - switches to previous volume when headphones are no longer plugged in
      
  - Function of the up-down keys changed
    - short press = Volume +/-
    - long press = Track +/- (unused if 5 buttons enabled)

  - Viual Studio Code mit platformio-code
    
 **Hardware / Layout**
  (http://hobbycomponents.com/images/forum/Wemos_Lolin_D32_Diagram_HCWEMO0014.png)
 
 ![fritzing Layout](https://raw.githubusercontent.com/lrep/TonUINO-ESP32/master/Fritzing/Layout_schema.png)
 
 **Still open and planned:**
 
  - Saving the settings in EEPROM / Flash
  - Manage tags
  

  **additional goodies**

   - precompiles bin file
