# [TonUINO-ESP32](http://discourse.voss.earth/t/esp32-port-inkl-webinterface/399)
_[TonUINO V2.0](https://www.voss.earth/tonuino/) Code for ESP32 with extensions, there is also [Discourse Seite](http://discourse.voss.earth/)_

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
    
  - Language menu and structure expanded to Tag to save Ambilight color
    
  - Function of the up-down keys changed
    - short press = Volume +/-
    - long press = Track +/-
    
 **Layout**
 
 
 ![fritzing Layout](https://raw.githubusercontent.com/lrep/TonUINO-ESP32/master/Fritzing/Layout_schema.png)
 
 **Still open and planned:**
 
  - Saving the settings in EEPROM / Flash
  - Manage tags
  
  
  
