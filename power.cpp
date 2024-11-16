#include "power.h"    

void Power::getByWebApi()
{
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

  //zwei mal die Standard tasmota steckdose 
  readTasmotaSteckdose(powerBlueInverterGet,blueInverter,eBlueInverter);
  readTasmotaSteckdose(powerDeyeInverterGet,deyeInverter,eDeyeInverter);
}    


    
void Power::readTasmotaSteckdose(const char *getString, int &power, bool &err)
{
  http.begin(getString);
  http.GET();
  //{"StatusSNS":{"Time":"2023-10-09T09:21:49","ENERGY":{"TotalStartTime":"2023-07-10T09:17:23","Total":49.766,"Yesterday":0.608,"Today":0.011,"Power":1,"ApparentPower":20,"ReactivePower":20,"Factor":0.06,"Voltage":230,"Current":0.085}}}
  //beachte: Stream braucht mehr speicher, trotz assistant der 384 empfohlen hat, bekomme ich Nomemory - manchmal 
  //filter würde auch etwas reduzieren
    StaticJsonDocument<512> doc; //groesse mit dem assistant ermittelt, doc sollte nur einmal verwendet werden 
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

char * Power::getJSON(const char * action)
{
  StaticJsonDocument<256> doc;
  static char output[384];
  doc["action"] = action;

  JsonObject values = doc.createNestedObject("values");
   
  values["powerHouse"] = house;
  values["powerBlueInv"] = blueInverter;
  values["powerDeyeInv"] = deyeInverter;
  values["bluettiOutDC"] = bluettiOutDC;
  values["bluettiOutAC"] = bluettiOutAC;
  values["bluettiIn"] = bluettiIn;
  values["bluettiPercent"] = bluettiPercent;
  if (!bluettiDCState)
    values["bluettiDCState"] = "off";
  else 
    values["bluettiDCState"] = "on";
  values["eBluetti"] = eBluetti;
  values["eHouse"] = eHouse;
  values["eBlueInverter"] = eBlueInverter;  
  values["eDeyeInverter"] = eDeyeInverter;    
  /*
  values["mHouse"] = mHouse;
  values["mBlueInverter"] = mBlueInverter;
  values["mDeyeInverter"] = mDeyeInverter;
  values["mBluettiPercent"] = 200;
  values["meanBluettiIn"] = 200;
  */
  serializeJson(doc, output);
  return output;
}
char * Power::getString()
{
  static char out[96];
  sprintf(out,"house: %d, bluePerc: %d, bluettiSolarIn: %d, deyeSolar: %d, blueOut: %d",house,bluettiPercent,bluettiIn,deyeInverter,blueInverter);
  return out;
}