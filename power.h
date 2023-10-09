#ifndef POWER_H
#define POWER_H
#include "config.h"
#include <HTTPClient.h>
#include <ArduinoJson.h> //ArduinoJson hat ein anderes Speicherkonzept als Arduino_Json


//power direkt abfragen nicht über mqtt (ist so einfach schneller)
#define powerHouseGet        "http://192.168.0.238/cm?cmnd=status%2010"
#define powerBlueInverterGet "http://192.168.0.237/cm?cmnd=status%2010"
#define powerDeyeInverterGet "http://192.168.0.239/cm?cmnd=status%2010"

//Die Leistungswerte

class Power {
   public:
    int house;           //aus webapi
    int blueInverter;    //aus webapi
    int deyeInverter;    //aus webapi
    int bluetti;         //aus bluetooth 
    int bluettiPercent;  //aus bluetooth

    //Mittelwerte
    int mHouse;           
    int mBlueInverter; 
    int mDeyeInverter;    
    int mBluetti;         
    int mBluettiPercent;  

    //Fehler 
    bool eHouse;
    bool eBlueInverter;
    bool eDeyeInverter;
    bool eBluetti;  //bluetooth

    //zaehler fuer die Mittelwerte
    int blueCounter; 
    int apiCounter; 

    //Methoden 
    public:
      Power()
      {
        eHouse = eBlueInverter = eDeyeInverter = eBluetti = true; 
      }
      void getByWebApi();
    
    private:
      const char * httpGet(char *fullRequest);
      //standard-Tasmotasteckdose
      void readTasmotaSteckdose(char *getString, int &power, bool &error);
    
    
    
};
#endif
