
/*
Information über und Steuerung der kleinen Solaranlage zu Hause. 
zwei Panels a 420 W, ein Deye-Inverter, ein Bluetti Würfel
Bluetti benötigt ideal eine Reihenschaltung, deye hat 2 x 2 Eingänge, Reihe geht nicht
kleiner 12 V inverter der angeblich 500 W kann (bei 180 heizt er nur noch...) Regelung über eine 
kleine Schraube möglich -> servo
getestet mit Esp32 D1 Mini, Partition Schema Minimal SPIFFS sonst dürfte OTA nicht gehen
todo: 
  - paar Warnings
  - Asynch Webserver geht inzwischen woll besser, s. Warning
  - Servo hier nur in alter Version, neue braucht gnu++17 damit geht aber der esp nicht
  - NimBLE - Version 1.4.0 genutzt, 1.4.1 tut es nicht für mich, bekomme keine Antwort von der Bluetti
  - bluetooth - evtl. doch reboot esp, inzwischen die settings in preferences gespeichert
**/
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <Preferences.h>        
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>//mist, der braucht den ESPAsyncWebServer, habe mal in der Bibliothek angepasst, so dasss es auch mit ESPAsyncWebSrv.h geht
                              //und wieder zurück, nehme den AsyncWebServer Bibliothek manuell installiert aus zip
#include <ArduinoJson.h> //ArduinoJson hat ein anderes Speicherkonzept als Arduino_Json
#include <PubSubClient.h>
#include <HTTPClient.h>

#include <Servo.h>


//#define DEBUG 1
//----------------------------------die Bluetti betreffend
#define AC200M          2
#define BLUETTI_TYPE AC200M //noetig vor dem Einbinden der Bibliothek
//unklar, ob man obiges nicht besser machen könnte, aber ich muss natürlich nicht so viel einbinden, wenn 
//schon im Define festgelegt ist, welches Modul behandelt wird
#include <Bluetti.h> //bekomme beim kompilieren von Bluetti.cpp fehler 'bluetti_polling_command' was not declared in this scope unklar warum
//warum nicht in dem Scope - vielleicht nur im Scope des Main-Programms? in Bluetti.cpp hilft ein 
// extern  device_field_data_t bluetti_polling_command[]; 
// alternativ könnte man alle eincompilieren, aber dann wird es groesser - hmm, gefällt mir alles nicht 
// andere Lösung aber notwendig, denn so funktioniert sizeof auf das command nicht, uebergebe das polling_command
// das auch noetig mit DeviceType? - hmm, denkfehler - sizeof kann bei einem Pointer nicht funktionieren
// das betrifft bluetti_polling_command, bluetti_device_command und bluetti_device_state
/*
 * Die Def von AC200M und der include sorgen dafür, dass die passenden commands, die ich unten in einer Struktur zusammen fasse 
 * geladen werden. Genauso der device_state, er ist abhängig vom Modell
 * ich kann nur die AC200M testen
 */

bluetti_command_t bluettiCommand = {bluetti_polling_command,sizeof(bluetti_polling_command),
                                bluetti_device_command,sizeof(bluetti_device_command),
                                bluetti_device_state,sizeof(bluetti_device_state)};
                                //Bluetti 


//---------------------------------------------------------

#include "credentials.h" //erstellen, s. credentials_template.h

#include "ownLists.h"
#include "index_htmlWithJS.h" //variable mit dem HTML/JS anteil
#include "power.h"
/*
 * Alte Datei kopiert und auf servo reais etc angepasst, Steuerung / Auslesen der Werte Solar und Strom-Kram 
 * Leider probleme mit dem selbst gebastelten (nach diversen Tutorials) webserver
 * Erster Versuch noch mit eigenem Webserver, jedoch Fehlermeldungen im Betrieb, nach etwas Recherche stelle um
 * auf https://github.com/dvarrel/ESPAsyncWebSrv das ist ein Fork von https://github.com/me-no-dev/ESPAsyncWebServer
 * dahinter steckt dann Hristo Gochkov's ESPAsyncWebServer 
 * Der unterstützt auch Websockets und nachdem ich 2014 oder so damit keinen Erfolg hatte (Browserstress), sollte es inzwischen ja gehen 
 * Tutorial: https://m1cr0lab-esp32.github.io/remote-control-with-websocket/
 * geht sogar gut, stelle großteils darauf um
 * 
 * Außerdem: ArduinoJson -> dort ein Link auf HeapFragmentation, hoffe mal das es kein Thema bei den par Strings ist - die sorgen 
 * für stress, ArduinoJson sorgt jedoch für eine Vermeidung von HeapFragmentation, auch wenn man dynamisch alloziiert. 
 * auf der WebSite findet sich ein calculator für den Speicher
 */

//in credentials.h, s. auch credentials_template.h
//#define mySSID "todo"
//#define myPASSWORD "passwort"
const char * ssid = mySSID;
const char *password = myPASSWORD;
 
ObjectList <String> startmeldungen(16); //dient zum Puffern der Meldungen am Anfang

Power power; //fuer die Werte die über Bluetooth, eigene Api-Calls entstehen, mqtt registrierung bei fhem nehme ich später mal raus 
//nein, sende per mqtt wesentliche werte, nur frage ich keine mehr ab. in fhem hatte ich die subscriptions tele/DVES_17B73E/SENSOR tele/DVES_183607/SENSOR tele/DVES_352360/SENSOR tele/DVES_9C2197/SENSOR
//dann kann ich diese Daten hier empfangen, sie sind aber jetzt unnötig, da die Abfrage über WebApi einfach schneller ist.
//die alte Variante lässt sich noch im Branch mqttOLD nachlesen

Preferences preferences;                        

Servo servo;
static const int servoPin = 15;//ist io15 = tdo

//und die Relais
const int r1pin = 26;  
const int r2pin = 18;
const int r3pin = 19;
const int r4pin = 23;

enum class ServoStatus {Left=105,Right=79,Stop=92} servoStatus;
enum class LadeStatus  {DeyeOnly=0,BluettiOnly=1,BluettiDeye=2,initDeyeOnly=3,initBluettiOnly=4,initBluettiDeye=5} ladeStatus;
  
long lastMsg = 0;
long lastMQTTMsg = 0;
bool adjustBluettiFlag = false;
bool chargeSelectFlag = false;
bool autoAdjustBlue = false;
bool autoCharge = false;

int intervalAutoAdjust = 120;
int intervalAutoCharge = 120;
long lastBluettiAdjust = 0;
long lastAutoCharge = 0;


 


AsyncWebServer server(80);
//fuer den Websocket
AsyncWebSocket ws("/ws");
unsigned long keepWebServerAlive = 0; //240000; //milliseconds also 4 Minuten, danach wird wifi abgeschaltet.
//in dieser Zeit ein client connect -> der Server bleibt aktiv, 0 er bleibt aktiv, lohnt nicht, bringt bei 12 V nur 6mA gewinn
unsigned long startTime;

//---------------------------------------------
//und fuer mqtt (aus credentials)
const char* mqttServer = mqttBROKER;
const int mqttPort = mqttPORT ;

WiFiClient wifiClient; //nicht ganz klar - ein Client der die Verbindung nutzen kann
PubSubClient mqttClient(wifiClient); 

//--------------------websocket kram 
void wsMessage(const char *message,AsyncWebSocketClient * client = 0)
{ 
  const char * begin = "{\"action\":\"message\",\"text\":\"";
  const char * end = "\"}";
  char * str = new char[strlen(message)+strlen(begin)+strlen(end)+24];//Puffer +24 statt +1 :-)
  strcpy(str,begin);
  strcat(str,message);
  strcat(str,end);
  if (!client)
    ws.textAll(str);
  else
    client->text(str);
    
  delete [] str;
}
void wsMessageNLB(const char *message,AsyncWebSocketClient * client = 0)
{ 
  const char * begin = "{\"action\":\"messageNLB\",\"text\":\"";
  const char * end = "\"}";
  char * str = new char[strlen(message)+strlen(begin)+strlen(end)+24];//Puffer +24 statt +1 :-)
  strcpy(str,begin);
  strcat(str,message);
  strcat(str,end);
  if (!client)
    ws.textAll(str);
  else
    client->text(str);    
  delete [] str;
}



void wsMsgSerial(const char *message, AsyncWebSocketClient * client = 0)
{
  wsMessage(message,client);
  Serial.println(message);
}

void wsMsgSerialNLB(const char *message, AsyncWebSocketClient * client = 0) //No line break
{
  wsMessageNLB(message,client);
  Serial.print(message);
}


//daten aus dem Power-Objekt an die Clients senden
void informClients()
{
  char *json =  power.getJSON("power");
  ws.textAll(json);

  //und statusmeldungen als confirm senden, nein als status
  char status[384];
  sprintf(status,"{\"action\":\"status\",\"intervalAutoAdjust\":%d,\"intervalAutoCharge\":%d,\"maxPowerBlue\":%d,\"minPercentBlue\":%d,",intervalAutoAdjust,intervalAutoCharge,power.maxPowerBlue,power.minPercentBlue);
  strcat(status,"\"values\":[");
  char end[]="]}";
  if (ladeStatus == LadeStatus::BluettiDeye)
    strcat(status,"\"bluettiDeye\",");
  else if ( ladeStatus == LadeStatus::BluettiOnly )
    strcat(status,"\"bluettiOnly\",");
  else if ( ladeStatus == LadeStatus::DeyeOnly )
    strcat(status,"\"deyeOnly\",");


  (power.bluettiOutDC) ? strcat(status,"\"bluettiDCOn\",") : strcat(status,"\"bluettiDCOff\",");
  
  if (servoStatus == ServoStatus::Left)
    strcat(status,"\"servoLeft\","); 
  else if (servoStatus == ServoStatus::Right)
    strcat(status,"\"servoRight\","); 
  else if (servoStatus == ServoStatus::Stop)
    strcat(status,"\"servoStop\","); 

  (autoCharge) ? strcat(status,"\"autoChargeOn\",") : strcat(status,"\"autoChargeOff\",");  
  (autoAdjustBlue) ? strcat(status,"\"autoAdjustBlueOn\"") : strcat(status,"\"autoAdjustBlueOff\"");  
  strcat(status,end);
  ws.textAll(status);
}

//wesentlich daten per mqtt versenden
void mqttPublish()
{
  char status[16];
  if (ladeStatus == LadeStatus::BluettiDeye)
    mqttClient.publish("esp32solar/LadeStatus","BluettiDeye");
  else if ( ladeStatus == LadeStatus::BluettiOnly )
    mqttClient.publish("esp32solar/LadeStatus","BluettiOnly");
  else if ( ladeStatus == LadeStatus::DeyeOnly )
    mqttClient.publish("esp32solar/LadeStatus","DeyeOnly");
  
  sprintf(status,"%d",power.bluettiOutAC);
  mqttClient.publish("esp32solar/BluettiOutAC",status);
  sprintf(status,"%d",power.bluettiOutDC);
  mqttClient.publish("esp32solar/BluettiOutDC",status);
  sprintf(status,"%d",power.bluettiIn ); //Solar-Leistung
  mqttClient.publish("esp32solar/BluettiInSolar",status);
  sprintf(status,"%d",power.deyeInverter ); //Solar-Leistung
  mqttClient.publish("esp32solar/DeyeSolar",status);
  sprintf(status,"%d",power.bluettiPercent); 
  mqttClient.publish("esp32solar/BluettiStatePercent",status);

  sprintf(status,"%d",(power.deyeInverter + power.bluettiIn) ); //Solar-Leistung gesamt
  mqttClient.publish("esp32solar/GesamtSolar",status);

  sprintf(status,"%d",autoCharge );
  mqttClient.publish("esp32solar/AutoSelectCharge",status);

  sprintf(status,"%d",autoAdjustBlue );
  mqttClient.publish("esp32solar/AutoAdjustBlue",status);

}
//alle daten, die am Anfang gesendet werden
void sendAvailableData(AsyncWebSocketClient * client, AsyncWebSocket *server)
{
   //sende die Daten an den Client
   wsMessage(startmeldungen.htmlLines().c_str(),client);
   String msg = String("WebSocket client ") + String(client->id()) + String(" connected from ") +  client->remoteIP().toString();
   wsMsgSerial(msg.c_str());
   msg = String("Anzahl Clients: ") + server->count();
   wsMsgSerial(msg.c_str());
   char * json = power.getJSON("power");
   ws.textAll(json);
   informClients();
}

//bluetti fertig stellen
//der callback - vielleicht besser 
//von der Bluetti-Klasse erben und mit virtuellen Methode arbeiten?
void bleNotifyCallback(const char * topic , String value)
{
  //Serial.println("We have topic " + topic + " and val " + value);
  static char out[256];
  #ifdef DEBUG
  strcpy(out,"in Callback, topic is");
  strcat(out,topic);
  wsMsgSerial(out);
  #endif
  if (!strcmp(topic,"total_battery_percent"))
  {
    power.bluettiPercent = value.toInt();
    power.eBluetti = false;
  }
  else if (!strcmp(topic, "dc_input_power"))
  {
    power.bluettiIn = value.toInt();
    power.eBluetti = false;
  }
  /* an aus ist überflüssig - nee doch nicht, muss nur nicht angezeigt werden */
  else if (!strcmp(topic , "dc_output_on"))
  {
    power.eBluetti = false;
    power.bluettiDCState = (value == "0") ? false : true;
  }
  else if (!strcmp(topic, "dc_output_power"))
  {
    power.bluettiOutDC = value.toInt();
    power.eBluetti = false;
  }
  else if (!strcmp(topic, "ac_output_power"))
  {
    power.bluettiOutAC = value.toInt();
    power.eBluetti = false;
  }

  #ifdef DEBUG
  sprintf(out,"Power: State: %d Percent %d out DC %d out AC %d in %d",power.bluettiDCState, power.bluettiPercent,power.bluettiOutDC,power.bluettiOutAC, power.bluettiIn);
  wsMsgSerial(out);
  #endif
}

Bluetti blue((char *) "AC200M2308002058882",bluettiCommand,bleNotifyCallback); //die bluetoothid, das command fuer das modell und der Callback 


void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect("ESP32Solar")) {
      Serial.println("connected");
      // Subscribe - nein ist raus
      /*
      mqttClient.subscribe("tele/DVES_17B73E/SENSOR");
      mqttClient.subscribe("tele/DVES_352360/SENSOR");//power haus ist tasmota, das kann ich so abgreifen
      mqttClient.subscribe("tele/DVES_9C2197/SENSOR");//alles andere an tasmota devices geht nicht, seltsam buffer problem
      mqttClient.subscribe("tele/DVES_183607/SENSOR");
      */
      /*
       * Hmm, unter http://192.168.0.203:8083/fhem?detail=MQTT2_ESP32Solar und dann subscriptions ist auch nur das erste 
       */
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void setupMQTT() 
{
  mqttClient.setBufferSize(1024);
  mqttClient.setServer(mqttServer, mqttPort);
  // set the callback function
  //mqttClient.setCallback(mqttCallback); //(noetig?)
}


//------------------------------------------
//404 message
void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found - ich kann das nicht");
}

//aufruf auf / wird durch den prozessor gesendet, hier wenig einzutragen, man könnnte die Power werte setzen, 
//die werden aber auch durch den websocket gesendet
String processor(const String& var)
{
  String result = "";
 
  if (var == "UPDATE_LINK")
    result = "<a href=\"http://" + WiFi.localIP().toString() +"/update\"> Update </a>";
  else if (var == "bluettiDeye")
    result = String((int) LadeStatus::BluettiDeye);
  else if (var == "bluettiOnly")
    result = String((int) LadeStatus::BluettiOnly);
  else if (var == "deyeOnly")
    result = String((int) LadeStatus::DeyeOnly);

  return result;
}


//bei einem connect muesste ich doch eigentlich die notwendigen Daten an den speziellen client 
//senden koennen. Ich war ein wenig in der "Ajax-Denke" gefangen - da hat der Client die Daten 
//abgerufen, das ist hier aber unnoetig und fuehrt zu weiteren get-Geschichten im behandeln 
//der events. 
void onWSEvent(AsyncWebSocket     *server,  //
             AsyncWebSocketClient *client,  //
             AwsEventType          type,    // the signature of this function is defined
             void                 *arg,     // by the `AwsEventHandler` interface
             uint8_t              *data,    //
             size_t                len)    
{
    // we are going to add here the handling of
    // the different events defined by the protocol
     switch (type) 
     {
        case WS_EVT_CONNECT:
            //bei einem connect werden die Startmeldungen ausgegeben, hier muesste ich doch schon die Daten senden können?       
            sendAvailableData(client, server);
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
     }
}
 
//schalten, die confirmMessage wird gesendet
void schalteLaden(LadeStatus dest)
{
  static char out[256];
  strcpy(out,"{\"action\":\"confirm\",\"topic\":\"");
  int confirmLength = strlen(out);
  char confirmEnd[] = "\"}";

  if ( dest == LadeStatus::BluettiOnly  && ladeStatus != LadeStatus::BluettiOnly || dest==LadeStatus::initBluettiOnly)
  {
    schalteRelais("bR3on"); //strange syntax has "historical" reasons :-) hatte erst Testaufbau
    schalteRelais("bR2on");
    schalteRelais("bR4on");
    schalteRelais("bR1on");
    ladeStatus = LadeStatus::BluettiOnly;
    strcat(out,"bluettiOnly");
    wsMsgSerial("Schalte auf Bluetti Only");
  }
  else if ( dest == LadeStatus::DeyeOnly  && ladeStatus != LadeStatus::DeyeOnly || dest==LadeStatus::initDeyeOnly) 
  {
    schalteRelais("bR1off");
    schalteRelais("bR2off");
    schalteRelais("bR3off");
    schalteRelais("bR4off");
    ladeStatus = LadeStatus::DeyeOnly;
    strcat(out,"deyeOnly");
    wsMsgSerial("Schalte auf Deye Only");
  }
  else if ( dest == LadeStatus::BluettiDeye  && ladeStatus != LadeStatus::BluettiDeye || dest==LadeStatus::initBluettiDeye)
  {
    schalteRelais("bR1off");
    schalteRelais("bR2off");
    schalteRelais("bR3off");
    schalteRelais("bR4on");
    ladeStatus = LadeStatus::BluettiDeye;
    strcat(out,"bluettiDeye");
    wsMsgSerial("Schalte auf Bluetti und Deye");
  }
  if (strlen(out)!=confirmLength)
  {
    preferences.putUInt ("ladeStatus",(uint) ladeStatus);
    strcat(out,confirmEnd);
    ws.textAll(out);
  }
}
//Falls Zustand ok die Bluetti einschalten 
/*
  Die Funktion ist noch nicht ok, hatte delays zum Testen eingebaut, wird sie ohne ausgeführt, dann bekomme ich einen Absturz
  des ESP - noch unklar, noe muesste inzwischen ok sein 
  So geht erhöhen der Leistung, verringern nicht, da keine Sonne mehr :-)
*/
void handleAdjustBluetti()
{
  static char out[256];
  static bool firstCall = true;
  if (adjustBluettiFlag)
  {
    if (firstCall)
    {
      strcpy(out,"check blue adjust - "); //erst dann ausgabe, kommt sonst zu oft, da kein Delay da ist
          //      012345678901234567890 
      strcat(out,power.getString());
      wsMsgSerial(out);
      firstCall = false;
    }
    else 
    {
      sprintf(out,"(%d)",power.blueInverter); //Blueinverter ist die aktuelle Leistung 
      wsMsgSerialNLB(out);
    }
    if (!power.eBluetti && power.bluettiPercent > power.minPercentBlue  )
    {
      if (power.house > 0 && ! power.bluettiDCState  && power.blueInverter < 10 ) //power.blueInverter reagiert schneller als der bluetooth state
      {
        wsMsgSerial("Hausverbrauch > 0, schalte Bluetti an");
        blue.switchOut((char *) "dc_output_on",(char *) "on"); //glaube, das geht nicht, vielleicht weil die handleBluetooth geschichte nicht gerufen wird?
        //ja daran lag es, arbeite inzwischen mit flag
        blue.handleBluetooth();
        delay (2000);
      }
      if (power.house > 20 && power.bluettiPercent > power.minPercentBlue && !power.eBluetti && power.blueInverter < 0.85 *power.maxPowerBlue) //faktor experimentell
      {
        if (servoStatus != ServoStatus::Left)
        {
          servoStatus = ServoStatus::Left;
          servo.write((int) servoStatus);
        }
        //wsMsgSerial("Hausverbrauch > 0, bleibe beim Erhöhen der Bluetti ");
        wsMsgSerialNLB("(+)");
        delay (100);
      } 
      else if (power.house < -10 && power.blueInverter>38 || power.blueInverter > power.maxPowerBlue) //kleiner als 34 klappt nicht
      {
        if (servoStatus != ServoStatus::Right)
        {
          servoStatus = ServoStatus::Right;
          servo.write((int) servoStatus);
        }
        //wsMsgSerial("Hausverbrauch <0 oder input inverter zu hoch , bleibe beim Verringern der Bluetti");
        wsMsgSerialNLB("(-)");
        delay (100);
        if (power.house < -35 && power.bluettiDCState && power.blueInverter > 20) //power.blueInverter reagiert schneller als der bluetooth state
        {
          blue.switchOut((char *) "dc_output_on",(char *) "off");
          wsMsgSerial("Verbrauch < -35 schalte Blue aus, adjust blue done");
          servoStatus = ServoStatus::Stop;
          servo.write((int) servoStatus);
          adjustBluettiFlag = false;
          firstCall = false;
          ws.textAll("{\"action\":\"confirm\",\"topic\":\"adjustBluettiDone\"}");
        }
      }
      else // abschalten servo
      {
          servoStatus = ServoStatus::Stop;
          servo.write((int) servoStatus);
          adjustBluettiFlag = false;
          wsMsgSerial("adjust Blue done");
          firstCall = false;
          ws.textAll("{\"action\":\"confirm\",\"topic\":\"adjustBluettiDone\"}");
          delay (100);
      }
    }

  }
}

void handleChargeSelect()
{
  static char out[256];
  if (chargeSelectFlag)
  {
    int solar = power.bluettiIn + power.deyeInverter;
    int house = power.house + power.deyeInverter + power.blueInverter;
    sprintf(out,"check autoCharge, house %d, solar %d and ",house, solar);
    strcat(out,power.getString());
    wsMsgSerial(out);
    if (power.eBluetti && ladeStatus == LadeStatus::BluettiDeye ) //fehler bluetooth  
        solar = 2*power.deyeInverter;
    if ((solar < house  || house  > 400)&& power.deyeInverter > 10 ) //power deyeInverter dazu, denn falls er hängt wenigstens die Bluetti laden 
    {
      schalteLaden(LadeStatus::DeyeOnly);
      //adjustBluettiFlag = true; lassen wir
    }
    else 
    {
      if (power.bluettiPercent <= 95)
      {
        //lassen wir adjustBluettiFlag = true;
        if (house > power.maxPowerBlue * 1.5 )
          schalteLaden(LadeStatus::BluettiDeye);
        else 
        {
          schalteLaden(LadeStatus::BluettiOnly); //hmm, dann sollte aber die Ausgabe der Bluetti angepasst werden ?
          adjustBluettiFlag = true;
        }
      }
      else 
        schalteLaden(LadeStatus::DeyeOnly);
    }
  }
  chargeSelectFlag = false;
}


//nachricht vom client
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) 
{
  AwsFrameInfo *info = (AwsFrameInfo*)arg; 
  static char out[256];
  strcpy(out,"{\"action\":\"confirm\",\"topic\":\"");
  int confirmLength = strlen(out);
  char confirmEnd[] = "\"}";
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) 
  {
    data[len] = 0; //sollte ein json object als String sein        
    char *cData = (char *)data;
    //String s = cData; //legt eine Kopie an
    String message;
    const int capacity = JSON_OBJECT_SIZE(3)+2*JSON_OBJECT_SIZE(3);//+JSON_ARRAY_SIZE(RFID_MAX); //
    
    StaticJsonDocument<capacity> doc; //hinweis: doc nur einmal verwenden 
    Serial.println("we have in WebSocketMessage: ");
    Serial.println(cData);
    
    DeserializationError err = deserializeJson(doc, cData); //damit veraendert das JSON-Objekt den Speicher cData
    if (err) //faengt auch Probleme beim Speicherplatz ab. 
    {
      Serial.print(F("deserializeJson() failed with code "));
      Serial.println(err.f_str());
      wsMsgSerial("Fehler beim Derialisieren");//erhalte die Nachricht, javascript hat dann natuerlich einen json-error, das ist ok, dann sieht man es :-)
    }
    else
    {
      /*
      if(strcmp(doc["action"],"clearNew") == 0) //:-) gibts gar nicht mehr 
      {
        //rfidsNew.clear();
        strcat(out,"clearNew");
        ws.textAll("{\"action\":\"clearNew\"}");
        wsMsgSerial("Neu gelesene geloescht");
      }*/
      if (strcmp(doc["action"],"dc_output") ==0)
      {
          //String state =  (const char *) doc["value"];
          if ( !strcmp((const char *) doc["value"],"toggle") )
          {
              if(power.bluettiDCState)
              {
                blue.switchOut((char *) "dc_output_on",(char*) "off");
              }
              else
              { 
                blue.switchOut((char *) "dc_output_on", (char *) "on");
              }
          }
          else
          {
            blue.switchOut((char *) "dc_output_on", (const char *) doc["value"]);
            if (!strcmp((const char * )doc["value"],"on"))
            {
              power.bluettiDCState = true; //schon mal setzen, auch wenn es noch nicht stimmt
              strcat(out,"bluettiDCOn");
            }
            else 
            {
              power.bluettiDCState = false; 
              strcat(out,"bluettiDCOff");
            } 
          }
          //info an angeschlossene devices / smartphone muesste ueber bluetti(bluetooth)->dieser esp(websocket)-> angeschlosse devices laufen
      }
      else if ( strcmp(doc["action"],"servo")==0)
      {
            //wsMsgSerial(buffer);
            
            if (power.bluettiDCState && !power.eBluetti) //nur wenn Bluetti auch an ist
            {
              if (!strcmp(doc["value"],"left"))
              { 
                servoStatus = ServoStatus::Left;
                strcat(out,"servoLeft");
              }  
              else if (!strcmp(doc["value"],"right"))
              {
                servoStatus = ServoStatus::Right;
                strcat(out,"servoRight");
              }
              else 
              { 
                strcat(out,"servoStop");
                servoStatus = ServoStatus::Stop;
              }  
            }
            else 
            { 
              strcat(out,"servoStop");
              servoStatus = ServoStatus::Stop;
            }  
            servo.write((int) servoStatus);    
      }
      else if (strcmp(doc["action"],"rebootESP")==0)
      {
        wsMsgSerial("Restart of ESP");
        ws.closeAll();
        delay(1000);
        strcat(out,"rebootESP");
        ESP.restart(); //Restart behält den Zustand des pins für die relais bei - den Servo auch?
      }
      else if (strcmp(doc["action"],"setStandard")==0)
      {
        wsMsgSerial("Reset to Standard values");
        resetStandardSettings();
      }
      else if (!strcmp(doc["action"],"relais"))
      { //nicht mehr unabhängig schalten
        //schalteRelais( doc["value"]); //soll nicht mehr unabhängig möglich sein 
      }
      else if (!strcmp(doc["action"],"adjustBluetti"))
      {
        if (!strcmp(doc["value"],"start"))
        {
          strcat(out,"adjustBluettiStart");
          adjustBluettiFlag = true; //hier flag setzen statt aufzurufen
        }
        else 
        {
          strcat(out,"adjustBluettiStop");
          adjustBluettiFlag = false; //hier flag setzen statt aufzurufen
          autoAdjustBlue = false; //anpassung stoppen, dann auch autoAdjust stoppen
          preferences.putBool("autoAdjustBlue",autoAdjustBlue);
        }
      }
      else if (!strcmp(doc["action"],"changeCharge"))
      {
        LadeStatus dest = (LadeStatus) (uint) doc["value"];
        schalteLaden(dest);
      }
      else if (!strcmp(doc["action"], "autoCharge"))
      {
        if (!strcmp((const char * )doc["value"],"on"))
        {
          autoCharge = true; //schon mal setzen, auch wenn es noch nicht stimmt
          strcat(out,"autoChargeOn");
        }
        else 
        {
          autoCharge = false; 
          strcat(out,"autoChargeOff");
        }
        preferences.putBool("autoCharge",autoCharge);         
      }
      else if (!strcmp(doc["action"], "autoAdjustBlue"))
      {
        if (!strcmp((const char * )doc["value"],"on"))
        {
          autoAdjustBlue = true; //schon mal setzen, auch wenn es noch nicht stimmt
          strcat(out,"autoAdjustBlueOn");
        }
        else 
        {
          autoAdjustBlue = false; 
          strcat(out,"autoAdjustBlueOff");
        }         
        preferences.putBool("autoAdjustBlue",autoAdjustBlue);
      }
      else if (!strcmp(doc["action"],"intervalAutoAdjust"))
      {
        intervalAutoAdjust = (int) doc["value"];
        preferences.putInt("intAutoAdjust",intervalAutoAdjust);
      }
      else if (!strcmp(doc["action"],"intervalAutoCharge"))
      {
        intervalAutoCharge = (int) doc["value"];
        preferences.putInt("intAutoCharge",intervalAutoCharge);
      }
      else if (!strcmp(doc["action"],"maxPowerBlue"))
      {
        power.maxPowerBlue = doc["value"];
        preferences.putInt("maxPowerBlue",power.maxPowerBlue);
      }
      else if (!strcmp(doc["action"],"minPercentBlue"))
      {
        power.minPercentBlue = doc["value"];
        preferences.putInt("minPercentBlue",power.minPercentBlue);
      }
    }
    if (strlen(out)!=confirmLength)
    {
      strcat(out,confirmEnd);
      ws.textAll(out);
    }
  }
}

//--------------------------------------------------------------
void schalteRelais(const char * value) //muss const char * sein sonst meckert die JSON-Bibliothek
{
  static char out[256];
  strcpy(out,"Bin in Relais schalten: ");
  strcat(out,value);
  //wsMsgSerial(out);
  
  char relais[3]="";
  char switchTo[4]=""; 
  int mode = LOW;
  int pin = 0; 
  strncat(relais,value+1,2); //aufbau bR1on oder bR1off
  strcpy(switchTo,value+3);

  if (!strcmp(relais,"R1")) //naja wenig elegant
    pin = r1pin; 
  else if (!strcmp(relais,"R2"))
    pin = r2pin;
  else if (!strcmp(relais,"R3"))
    pin = r3pin;
  else if (!strcmp(relais,"R4"))
    pin = r4pin;
     
  if (!strcmp(switchTo,"on"))
    mode = HIGH;
  
  //sprintf(out,"Write to relais %s switchTo is %s, pin %d mode is %d",relais,switchTo,pin,mode);
  //wsMsgSerial(out);

  if (pin)
  {
    digitalWrite(pin,mode);
    sprintf(out,"Write to pin %d mode is %d",pin,mode);
    //wsMsgSerial(out);
  }
  delay(50); //zur sicherheit kleine Pause vor dem nächsten schalten - ob es das bringt, k.A.
   //und damit ich die Schaltfolge sehe
}


void resetStandardSettings()
{
  //stelle definierten Zustand her
  //power nur auf deye (wenn ich weg bin und der Würfel ist nicht da, dann sollte das der Standard sein)
  power.bluettiDCState = false;
  preferences.putBool("bluettiDCState",power.bluettiDCState);
  blue.switchOut((char *) "dc_output_on",(char *) "off");
  servoStatus = ServoStatus::Stop;
  servo.write((int) servoStatus); //stop
  schalteLaden(LadeStatus::DeyeOnly); 
  preferences.putUInt("ladeStatus", (uint) LadeStatus::DeyeOnly);
  autoCharge =false;
  preferences.putBool("autoCharge",autoCharge);
  autoAdjustBlue = false; 
  preferences.putBool("autoAdjustBlue",autoAdjustBlue);
  power.maxPowerBlue = 100;
  preferences.putInt("maxPowerBlue",power.maxPowerBlue );
  power.minPercentBlue = 20;
  preferences.putInt("minPercentBlue",power.minPercentBlue );
  intervalAutoAdjust = 120;
  preferences.putInt("intAutoAdjust",intervalAutoAdjust );
  intervalAutoCharge = 120;
  preferences.putInt("intAutoCharge",intervalAutoCharge );
}

              
//--------------------------------------------------------------------
void setup() {
  //beginn der seriellen Kommunikation mit 115200 Baud
  Serial.begin(115200);

  servo.attach(servoPin);
  //pins setzen 
  pinMode(r1pin ,OUTPUT); 
  pinMode(r2pin ,OUTPUT);
  pinMode(r3pin ,OUTPUT);
  pinMode(r4pin ,OUTPUT);

  WiFi.begin(ssid, password);

  //Netz nicht da, setze eigenen AP auf - ich hatte zu Hause in Abhängigkeit vom Router massive 
  //Probleme in das Netz zu kommen
  if (WiFi.waitForConnectResult() != WL_CONNECTED) //waitForConnect lässt sich viel Zeit
  {
     WiFi.disconnect(true);  // Disconnect from the network
     ssid     = apSSID;     //wieder in credentials.h
     password = apPASSWORD;  
     Serial.printf("WiFi Failed ! - go to ap mode \n");
     startmeldungen.add("WiFi  fehlgeschlagen, ap mode");
     WiFi.softAP(ssid, password);   
  }
  if (WiFi.status() == WL_CONNECTED) //wifi connected
  {
    Serial.print("Wifi Connected, address ");
    Serial.println(WiFi.localIP());
    String msg = String("Wifi connected, address ") + WiFi.localIP().toString();
    startmeldungen.add(msg.c_str());
  }

  //mqtt aufsetzen 
  setupMQTT();

  //bluetti bluetooth
  blue.initBluetooth();
  
  //Der Rest sind doch nur callbacks und der WebSocket-Server sollte gehen 
  server.onNotFound(notFound);
  //lustig, ich bin alt,  [] leitet einen Lambda Ausdruck ein, also eine anonyme Funktion
  //die gabs frueher nicht :-), dafür mehr Lametta
  server.on("/favicon.ico", [](AsyncWebServerRequest *request)
  {
    request->send(204);//no content
  });

  //normale Anfrage
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send_P(200, "text/html", index_html, processor); //durch processor filtern     
  });
  ws.onEvent(onWSEvent);
  server.addHandler(&ws); //WebSocket dazu 
  AsyncElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  //andernfalls startet er ohne wifi, registrieren von Elementen nicht möglich, nein habe mal einen ap-modus gesetzt  
  //eine kleine Pause von 50ms.
  delay(50);

  //lade gespeicherte Daten // setze initiale Werte 
  preferences.begin("settings",false); //"false heißt read/write"
  power.bluettiDCState = preferences.getBool("bluettiDCState", false);
  servoStatus = ServoStatus::Stop;              
  servo.write((int) servoStatus); //stop
  ladeStatus = (LadeStatus) preferences.getUInt("ladeStatus", (uint) LadeStatus::DeyeOnly);
  schalteLaden((LadeStatus) ((int)ladeStatus + 3)); //+3 für init
  autoCharge = preferences.getBool("autoCharge", false);
  autoAdjustBlue = preferences.getBool("autoAdjustBlue", false);
  power.maxPowerBlue  = preferences.getInt("maxPowerBlue",100);
  power.minPercentBlue  = preferences.getInt("minPercentBlue",20);
  intervalAutoAdjust  = preferences.getInt("intAutoAdjust",120);
  intervalAutoCharge = preferences.getInt("intAutoCharge",120 );
  //informClients(); nein, in Startmeldungen, das reicht 
}


void loop() {
  if (WiFi.status() == WL_CONNECTED)
  {
    ws.cleanupClients(); // aeltesten client heraus werfen, wenn maximum Zahl von clients ueberschritten, 
                       // manchmal verabschieden sich clients wohl unsauber / gar nicht -> werden wir brutal
    //power auslesen    
  }  

  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();
  blue.handleBluetooth();
  handleAdjustBluetti();
  handleChargeSelect();
  
  long now = millis();
  long delta = now - lastMsg;
  if (delta > 2000) //alle 2 Sekunden Information heraus und auf jeden Fall Bluetti adjust wenn power house < -50 
  {
    lastMsg = now;
    power.getByWebApi();
    informClients(); //
    //Serial.println("try to publish esp32solar/state online");
    //ein paar checks 
    //immer: Falls zu niedrig - abschalten 
    if (power.bluettiPercent <=power.minPercentBlue && !power.eBluetti && power.bluettiDCState)
    { //auf nummer sicher gehen
      wsMsgSerial("Bluetti Low, schalte ab");
      blue.switchOut((char *) "dc_output_on",(char *) "off");
      ws.textAll("{\"action\":\"confirm\",\"topic\":\"bBlueDCOff\"}");
    }  
  } 
  now = millis();
  delta = now - lastMQTTMsg;
  if (delta > 10000)// alle 10 s
  {
      mqttPublish(); 
      lastMQTTMsg = now;
  }
  now = millis();
  delta = now -  lastBluettiAdjust ;
  if (delta > 1000 * intervalAutoAdjust && autoAdjustBlue )
  {
    lastBluettiAdjust = now;
    adjustBluettiFlag = true;
  }
  else if (power.house < -30 && power.blueInverter > 20) //zu starke bluetti einspeisung auf jeden Fall anpassen
      adjustBluettiFlag = true;

  

  delta = now -  lastAutoCharge ;
  if (delta > 1000 * intervalAutoCharge && autoCharge )
  {//besser auch auf flag umstellen
    lastAutoCharge = millis();
    chargeSelectFlag = true;
  }
  else if (power.house < -30 && autoCharge)
    chargeSelectFlag = true;
 
}
