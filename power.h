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
    int bluettiOut;         //aus bluetooth ab hier
    int bluettiIn;
    int bluettiPercent; 
    bool bluettiDCState; 

    //Mittelwerte
    /* das ist noch nicht durchdacht, weiss nicht, ob ich das möchte  
    int mHouse;           
    int mBlueInverter; 
    int mDeyeInverter;    
    int mBluetti;         
    int mBluettiPercent;  
    */

    //Fehler 
    bool eHouse;
    bool eBlueInverter;
    bool eDeyeInverter;
    bool eBluetti;  //bluetooth

    //zaehler fuer die Mittelwerte
    /*
    int blueCounter; 
    int apiCounter; 
    */
    //Methoden 
    public:
      Power()
      {
        eHouse = eBlueInverter = eDeyeInverter = eBluetti = true;
        bluettiDCState = false; 
        house = blueInverter = deyeInverter = bluettiOut = bluettiIn = bluettiPercent = 0;
        http.useHTTP10(true); //use old http1.0 - stream is not chunked
      }
      void getByWebApi();
      char *getJSON(const char *action);
    
    private:
      HTTPClient http; 
      //standard-Tasmotasteckdose
      void readTasmotaSteckdose(const char *getString, int &power, bool &err);
           
};
#endif
