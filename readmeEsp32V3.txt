hatte geupdate - mehr aus versehen :-) esp32 v3 ist erst 2024 heraus gekommen
board auf esp32 v3 - leider viel geaendert, so dass die Bibliotheken nicht mehr laufen. 
Habe dabei die folgenden Dateien angefasst:
 1965  joe WebAuthentication.cpp 
 1966  joe AsyncEventSource.cpp 
 1967  joe AsyncWebSocket.cpp 
 1968  joe /home/roemke/Arduino/libraries/ServoESP32/src/Servo.cpp

danach kann ich kompilieren, aber der Servo tut es nicht mehr, einfach keine reaktion

downgrade 
/home/roemke/Arduino/libraries/AsyncElegantOTA/src/AsyncElegantOTA.h:18:14: fatal error: esp_private/esp_int_wdt.h: No such file or directory
     #include "esp_private/esp_int_wdt.h"
ok ist erst ab v3 im Verzeichnis, schaue also mal ob das mit ifdefs geht, einer hatte damit gearbeitet, mal sehen
AsyncWebSocket.cpp -> ifdefs drin
WebAuthentication.cpp auch drin, obwohl ich den Aufbau nicht ganz verstehe
~/Arduino/libraries/AsyncElegantOTA/src/AsyncElegantOTA.h:
so muesste es doch gehen 
#if defined(ESP_ARDUINO_VERSION_MAJOR) &&  ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)    
    #include "esp_private/esp_int_wdt.h"
#else
    #include "esp_int_wdt.h"
#endif
passe Servo.cpp analog an

kompilieren wieder möglich
und es läuft auch 
