#include "power.h"    

void Power::getByWebApi()
{
  char *result = 0;
  result = httpGet(powerHouseGet);//ist zwar tasmota, jedoch eigene Geschichte für den IR Lesekopf
  if (result)//{"StatusSNS":{"Time":"2023-10-09T07:31:50","Power":{"Power_curr":134,"Total_in":3663.1794,"Total_out":85.5164,"Meter_Number":"0a014954520003504c4c"}}}
  {
    StaticJsonDocument<128> doc; //groesse mit dem assistant ermittelt, doc sollte nur einmal verwendet werden 
    DeserializationError error = deserializeJson(doc, result);//damit veraendert das JSON-Objekt den Speicher result - geht das?
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
  }
  //zwei mal die Standard tasmota steckdose 
  readTasmotaSteckdose(powerBlueInverterGet,blueInverter,eBlueInverter);
  readTasmotaSteckdose(powerDeyeInverterGet,deyeInverter,eDeyeInverter);
}    

const char * Power::httpGet(char *fullRequest)
{  
  HTTPClient http; 
  char *payLoad = 0;
  http.begin(fullRequest);
  int httpCode = http.GET();  
  if (httpCode == 200) 
  {
    payLoad = http.getString().c_str();       
  }
  http.end();
  return payLoad;  
}
    
void Power::readTasmotaSteckdose(char *getString, int &power, bool &error)
{
  char *result = 0;
  result = httpGet(getString);
  if (result)
  {//{"StatusSNS":{"Time":"2023-10-09T09:21:49","ENERGY":{"TotalStartTime":"2023-07-10T09:17:23","Total":49.766,"Yesterday":0.608,"Today":0.011,"Power":1,"ApparentPower":20,"ReactivePower":20,"Factor":0.06,"Voltage":230,"Current":0.085}}}
    StaticJsonDocument<256> doc; //groesse mit dem assistant ermittelt, doc sollte nur einmal verwendet werden 
    DeserializationError error = deserializeJson(doc, result);//damit veraendert das JSON-Objekt den Speicher result - geht das? - nein
    if (error) 
    {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      error = true; 
    }
    else
    {          
      error = false;
      JsonObject PO = doc["StatusSNS"]["ENERGY"];
      power = PO["Power"]; // 134 
    }                
  }
}
