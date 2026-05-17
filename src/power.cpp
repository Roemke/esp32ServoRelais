#include "power.h"    

/*void Power::beginModBus()
{
  mb.begin();
} */
void Power::actualizeData()
{
  /*
    http.begin(powerHouseGet);
    http.GET();
    //{"StatusSNS":{"Time":"2023-10-10T11:49:16","Power":{"Power_curr":-6,"Total_in":3670.2250,"Total_out":85.5679,"Meter_Number":"0a014954520003504c4c"}}}
    //man koennte noch filtern
    StaticJsonDocument<256> doc; //groesse mit dem assistant ermittelt, doc sollte nur einmal verwendet werden 
    DeserializationError error = deserializeJson(doc, http.getStream());
    if (error) 
    {
        Serial.printf("deserializeJson() of %s failed: ",powerHouseGet);
        Serial.println(error.c_str());
        eHouse = true; 
    }
    else
    {
      eHouse = false;
      JsonObject PO = doc["StatusSNS"]["SML"]; //aenderung auf SML (von Power), da Script auf dem ESP an Power geändert
      house = PO["Power_curr"]; // 134 
    }
    http.end();
  */
  //zwei mal die Standard tasmota steckdose 
  readTasmotaSteckdose(powerBlueInverterGet,blueInverter,eBlueInverter);
  readTasmotaSteckdose(powerDeyeInverterGet,deyeInverter,eDeyeInverter);

  //daten per http-abfrage über pi vom Inverter holen
  readFromInverter();
}    


    
void Power::readTasmotaSteckdose(const char *getString, int &power, bool &err)
{
  http.begin(getString);
  http.GET();
  //{"StatusSNS":{"Time":"2023-10-09T09:21:49","ENERGY":{"TotalStartTime":"2023-07-10T09:17:23","Total":49.766,"Yesterday":0.608,"Today":0.011,"Power":1,"ApparentPower":20,"ReactivePower":20,"Factor":0.06,"Voltage":230,"Current":0.085}}}
  //beachte: Stream braucht mehr speicher, trotz assistant der 384 empfohlen hat, bekomme ich Nomemory - manchmal 
  //filter würde auch etwas reduzieren
    JsonDocument doc; //groesse mit dem assistant ermittelt, doc sollte nur einmal verwendet werden 
    DeserializationError error = deserializeJson(doc, http.getStream());
    if (error) 
    {
      Serial.printf("deserializeJson() of %s failed: ",getString);
      Serial.println(error.c_str());
      err = true; 
    }
    else
    {          
      err = false;
      JsonObject PO = doc["StatusSNS"]["ENERGY"];
      power = PO["Power"]; // 134 
    }
    http.end();
  
}
void Power::readFromInverter()
{
  http.begin(inverterServerGet);
  http.GET();//{"power":878.55,"power_ac":768.35,"power_bat":0.11,"power_dc":780.22,"soe":98.89}
  //https://arduinojson.org/v6/assistant/#/step1 esp32 stream
  JsonDocument doc;

  //folgender Code wird generiert, mit input
  DeserializationError error = deserializeJson(doc,http.getStream());// input);

  if (error) 
  {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    //err = true;
  }
  else
  {   //folgendes muessten die nötigen Daten sein        
      //err = false;
      seGrid    = doc["power"];      // Netzbezug, negativ = Bezug, positiv = Einspeisung
      sePowerAC = doc["power_ac"];   // Wechselrichter-Ausgang
      sePowerDC = doc["power_dc"];   // Solar DC
      sePowerBat = doc["power_bat"]; // Batterie (negativ=Entladung)
      seSoe     = doc["soe"];        // Ladestand %
      seHouse   = sePowerAC - seGrid; // tatsächlicher Hausverbrauch
  }
}

//meine erste variante arbeitete mit einem static char, das geht, kann aber gerade bei esp32 daneben gehen - mehrere Threads
//char * Power::getJSON(const char * action)
//methode sollte mit einem puffer von 384 bytes gerufen werden
size_t Power::getJSON(const char *action, char *buf, size_t buflen)
{
   const char *fallback = "{\"action\":\"power\",\"error\":\"json_overflow\"}";
  if (!buf || buflen < strlen(fallback)+1 ) return 0; //klarer fehlaufruf
  
  JsonDocument doc;
  doc["action"] = action;
  //static char output[384]; //zu gefährlich
  
  JsonObject values = doc["values"].to<JsonObject>(); 
  values["powerHouse"] = seHouse;
  values["powerBlueInv"] = blueInverter;
  values["powerDeyeInv"] = deyeInverter;
  values["bluettiOutDC"] = bluettiOutDC;
  values["bluettiOutAC"] = bluettiOutAC;
  values["bluettiIn"] = bluettiIn;
  values["bluettiPercent"] = bluettiPercent;
  values["bluettiDCState"] = bluettiDCState ? "on" : "off";
  values["eBluetti"] = eBluetti;
  values["eBlueInverter"] = eBlueInverter;  
  values["eDeyeInverter"] = eDeyeInverter;    
  values["seGrid"]     = seGrid;      // Netzbezug
  values["sePowerAC"]  = sePowerAC;   // Wechselrichter-Ausgang
  values["sePowerDC"]  = sePowerDC;   // Solar DC
  values["sePowerBat"] = sePowerBat;  // Batterie (negativ=Entladung)
  values["seSoe"]      = seSoe;       // SolarEdge Ladestand %
  /*
  values["mHouse"] = mHouse;
  values["mBlueInverter"] = mBlueInverter;
  values["mDeyeInverter"] = mDeyeInverter;
  values["mBluettiPercent"] = 200;
  values["meanBluettiIn"] = 200;
  */
  size_t n = serializeJson(doc, buf, buflen);
  if (n == 0) { //fehler   
    // sicher kopieren
    strncpy(buf, fallback, buflen - 1);//schließt mit 0
    buf[buflen - 1] = '\0'; //sicher gehen, falls platz nicht reichte    
  }
  return n ? n : strlen(buf);
}
char * Power::getString()
{
  static char out[96];
  sprintf(out, "house: %.1f, bluePerc: %d, bluettiSolarIn: %d, deyeSolar: %d, blueOut: %d",
                 seHouse, bluettiPercent, bluettiIn, deyeInverter, blueInverter);
  return out;
}