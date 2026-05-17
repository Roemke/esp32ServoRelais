#ifndef POWER_H
#define POWER_H
#include "config.h"
#include <HTTPClient.h>
#include "credentials.h"

#include <ArduinoJson.h> //ArduinoJson hat ein anderes Speicherkonzept als Arduino_Json

//vom Pi abfragen, der liefert die Daten des SolarEdge Wechselrichters
#define inverterServerGet "http://192.168.0.203:8090/data"
//power direkt abfragen nicht über mqtt (ist so einfach schneller)
#define powerBlueInverterGet "http://192.168.0.237/cm?cmnd=status%2010"
#define powerDeyeInverterGet "http://192.168.0.239/cm?cmnd=status%2010"

//Die Leistungswerte

class Power {
   public:
    //int house;           //aus webapi (vom esp am Stromzaehler) - raus genommen solarEdge müsste reichen
    int blueInverter;    //aus webapi (von Tasmota Steckdose)
    int deyeInverter;    //aus webapi  ""
    
    //daten des Wechselrichters von solarEdge
    float seGrid; //Netzbezug, positiv = Einspeisung, negativ = Bezug
    float sePowerAC; // Wechselrichter-Ausgang (solar + batterie + wandlungsverluste)
    float sePowerDC;   // Solar DC
    float sePowerBat;  // Batterie (negativ=Entladung)
    float seSoe;        // Ladestand %
    float seHouse; // tatsächlicher Hausverbrauch, wird gerechnet aus sePowerAC - seGrid



    int bluettiOutDC;         //aus bluetooth ab hier
    int bluettiOutAC;
    int bluettiIn;
    int bluettiPercent; 
    int maxPowerBlue;
    int minPercentBlue;
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
    //bool eHouse;
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
        //eHouse =  raus
        eBlueInverter = eDeyeInverter = eBluetti = true;
        bluettiDCState = false; 
        //house = raus
        blueInverter = deyeInverter = bluettiOutDC = bluettiOutAC = bluettiIn = bluettiPercent = 0;        
        maxPowerBlue = 100;
        minPercentBlue = 20;
        seHouse = seGrid = sePowerAC = sePowerDC = sePowerBat = seSoe = 0; 
        http.useHTTP10(true); //use old http1.0 - stream is not chunked
      }
      void actualizeData();
      //void beginModBus();

      //char *getJSON(const char *action);
      size_t getJSON(const char *action, char *buf, size_t buflen);
      char *getString();

    private:
      HTTPClient http; 
      //Verwendung von modbus tcp, um den SolarEdge Inverter abzufragen
      //ModbusIP mb;                   // Modbus-Objekt als Mitglied
      //nein hatte damit irgend einen Stress, habe modbus abfrage auf pi laufen, der stellt eine kleine api
      //IPAddress inverterIP = IPAddress(192, 168, 0, 205); // Fest zugewiesene IP
      //uint16_t port = 1502;             // Port, normal 502
      //uint16_t slaveID = 1;            // Modbus-ID    - egal?

      //standard-Tasmotasteckdose
      void readTasmotaSteckdose(const char *getString, int &power, bool &err);
      
      void readFromInverter();
           
};
#endif
