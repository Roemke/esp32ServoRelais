#include "power.h"    

void Power::getByWebApi()
{
  const char *result = httpGet(powerHouseGet);//ist zwar tasmota, jedoch eigene Geschichte für den IR Lesekopf
  if (result)//{"StatusSNS":{"Time":"2023-10-09T07:31:50","Power":{"Power_curr":134,"Total_in":3663.1794,"Total_out":85.5164,"Meter_Number":"0a014954520003504c4c"}}}
  {
    char *lres = new char[strlen(result)+1];
    strcpy(lres,result);
    StaticJsonDocument<128> doc; //groesse mit dem assistant ermittelt, doc sollte nur einmal verwendet werden 
    DeserializationError error = deserializeJson(doc, lres);//damit veraendert das JSON-Objekt den Speicher result - geht das? - nein
    if (error) 
    {
       Serial.print("deserializeJson() failed: ");
       Serial.println(error.c_str());
       eHouse = true; 
    }
    else
    {
      eHouse = false;
      JsonObject PO = doc["StatusSNS"]["Power"];
      house = PO["Power_curr"]; // 134 
    }
    delete [] lres;
  }
  //zwei mal die Standard tasmota steckdose 
  readTasmotaSteckdose(powerBlueInverterGet,blueInverter,eBlueInverter);
  readTasmotaSteckdose(powerDeyeInverterGet,deyeInverter,eDeyeInverter);
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
  values["bluettiOut"] = bluettiOut;
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
const char * Power::httpGet(char *fullRequest)
{  
  HTTPClient http; 
  const char *payLoad = 0;
  http.begin(fullRequest);
  int httpCode = http.GET();  
  if (httpCode == 200) 
  {
    payLoad = http.getString().c_str();       
  }
  http.end();
  return payLoad;  
}
    
void Power::readTasmotaSteckdose(char *getString, int &power, bool &err)
{
  const char *result = httpGet(getString);
  if (result)
  {//{"StatusSNS":{"Time":"2023-10-09T09:21:49","ENERGY":{"TotalStartTime":"2023-07-10T09:17:23","Total":49.766,"Yesterday":0.608,"Today":0.011,"Power":1,"ApparentPower":20,"ReactivePower":20,"Factor":0.06,"Voltage":230,"Current":0.085}}}
    char * lres = new char[strlen(result)+1];
    strcpy(lres,result);
    StaticJsonDocument<256> doc; //groesse mit dem assistant ermittelt, doc sollte nur einmal verwendet werden 
    DeserializationError error = deserializeJson(doc, lres);//damit veraendert das JSON-Objekt den Speicher result - geht das? - nein
    if (error) 
    {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      err = true; 
    }
    else
    {          
      err = false;
      JsonObject PO = doc["StatusSNS"]["ENERGY"];
      power = PO["Power"]; // 134 
    }
    delete []lres;                
  }
}
