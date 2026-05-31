/*
Information über und Steuerung der kleinen Solaranlage zu Hause. 
zwei Panels a 420 W, ein Deye-Inverter, ein Bluetti Würfel
Bluetti benötigt ideal eine Reihenschaltung, deye hat 2 x 2 Eingänge, Reihe geht nicht
kleiner 12 V inverter der angeblich 500 W kann (bei 180 heizt er nur noch...) Regelung über eine 
kleine Schraube möglich -> servo
getestet mit Esp32 D1 Mini, Partition Schema Minimal SPIFFS sonst dürfte OTA nicht gehen
todo: 
  - paar Warnings
  - Asynch Webserver geht inzwischen wohl besser, s. Warning
  - Servo hier nur in alter Version, neue braucht gnu++17 damit geht aber der esp nicht
  - NimBLE - Version 1.4.0 genutzt, 1.4.1 tut es nicht für mich, bekomme keine Antwort von der Bluetti
  - bluetooth - evtl. doch reboot esp, inzwischen die settings in preferences gespeichert
**/

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <Preferences.h>        
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
//#include <AsyncElegantOTA.h>//mist, der braucht den ESPAsyncWebServer, habe mal in der Bibliothek angepasst, so dasss es auch mit ESPAsyncWebSrv.h geht
                              //und wieder zurück, nehme den AsyncWebServer Bibliothek manuell installiert aus zip
#include <ElegantOTA.h> //modernerer OTA, der mit dem AsyncWebServer zusammenarbeitet
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
//left ist Leistung erhöhen, right verringern
enum class LadeStatus  {DeyeOnly=0,BluettiOnly=1,BluettiDeye=2,initDeyeOnly=3,initBluettiOnly=4,initBluettiDeye=5} ladeStatus;
  
long lastMsg = 0;
long lastMQTTMsg = 0;
bool adjustBluettiFlag = false;  // Servo-Regelung (Leistung Bluetti) aktiv (manuell per Webinterface gesetzt)
bool chargeSelectFlag = false;  //Panel-Schaltung neu bewerten (Deye/Bluetti laden/Einspeisen)
bool autoAdjustBlue = false;   //Automatische Anpassung der Leistung der Bluetti, im Webinterface
bool autoCharge = false;      //Automatische Wahl der Solareinspeisung webinterface
bool autoBlueInverter = false; //automatisches Zuschalten des Bluetti Inverters (Webinterface)


int intervalAutoAdjust = 120;
int intervalAutoCharge = 120;
long lastBluettiAdjust = 0;
long lastAutoCharge = 0;


 
void handleWebSocketMessage(void*, uint8_t*, size_t);
void schalteRelais(const char*);
void resetStandardSettings();

void handleBlueInverter();
void handleAdjustBluetti();
void handleChargeSelect();


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
  static constexpr size_t MAX_MSG = 1152;
  char buf[MAX_MSG];
  int n = snprintf(buf, sizeof(buf),
                 "{\"action\":\"message\",\"text\":\"%s\"}", message);
  //snprintf schreibt max sizeof(buf)-1 bytes + 0 - sicher. 
  //Rückgabe Zahl der zu schriebenden bytes ohne die 0, also bei sizeof(buf) schon eins zu viel
  // Rückgabe <0 format fehler, zeichensatzfehler o.ä. unwahrscheinlich

  if ( n < 0 || n >= (int)sizeof(buf) ) {
      strcpy(buf, "{\"action\":\"message\",\"text\":\"Strange - Message too large\"}");
  }

  if (!client)
    ws.textAll(buf);
  else
    client->text(buf);
    
  //delete [] str; hatte vorher str aber heap defragmentation möglich und gefährlich bei asynchron (nicht ganz klar use before copy, glaube ich nicht chat gpt)
}

void wsMessageNLB(const char *message,AsyncWebSocketClient * client = 0)
{ 
  static constexpr size_t MAX_MSG = 1152;
  char buf[MAX_MSG];
  int n = snprintf(buf, sizeof(buf),
                 "{\"action\":\"messageNLB\",\"text\":\"%s\"}", message);

  if ( n < 0 || n >= (int)sizeof(buf) ) {
      strcpy(buf, "{\"action\":\"messageNLB\",\"text\":\"Strange - Message too large\"}");
  }

  if (!client)
    ws.textAll(buf);
  else
    client->text(buf);    
  //delete [] str; alte variante
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
  char json[512];
  power.getJSON(json,sizeof(json));
  ws.textAll(json);

  //und statusmeldungen als confirm senden, nein als status
  char status[512];
  int n = snprintf(status, sizeof(status),
    "{\"action\":\"status\",\"intervalAutoAdjust\":%d,"
    "\"intervalAutoCharge\":%d,\"maxPowerBlue\":%d,"
    "\"minPercentBlue\":%d,\"seGridMinCharge\":%d,"
    "\"seGridBothCharge\":%d,\"values\":[", 
    intervalAutoAdjust, intervalAutoCharge, 
    power.maxPowerBlue, power.minPercentBlue,
    power.seGridMinCharge, power.seGridBothCharge);
  
  
  if (n < 0 || n >= (int)sizeof(status)) return; //ohne status zurück

  //lambda zum anhängen,    [&] referenz-zugriff auf die Daten des scope, auto - typ selbst bestimmen
  auto append = [&](const char *s) {
    size_t len = strlen(status);
    size_t sl  = strlen(s);
    if (len + sl + 1 < sizeof(status))
      strcat(status, s);
  };

  if (ladeStatus == LadeStatus::BluettiDeye)
    append("\"bluettiDeye\",");
  else if ( ladeStatus == LadeStatus::BluettiOnly )
    append("\"bluettiOnly\",");
  else if ( ladeStatus == LadeStatus::DeyeOnly )
    append("\"deyeOnly\",");


  (power.bluettiOutDC) ? append("\"bluettiDCOn\",") : append("\"bluettiDCOff\",");
  
  if (servoStatus == ServoStatus::Left)
    append("\"servoLeft\","); 
  else if (servoStatus == ServoStatus::Right)
    append("\"servoRight\","); 
  else if (servoStatus == ServoStatus::Stop)
    append("\"servoStop\","); 

  (autoCharge) ? append("\"autoChargeOn\",") : append("\"autoChargeOff\",");  
  (autoBlueInverter) ? append("\"autoBlueInverterOn\",") : append("\"autoBlueInverterOff\",");
  (autoAdjustBlue) ? append("\"autoAdjustBlueOn\"") : append("\"autoAdjustBlueOff\"");  
  
  append("]}");
  ws.textAll(status);
}

//wesentlich daten per mqtt versenden
void mqttPublish()
{
    char val[16];

    // Power
    power.calculate(); //sicherstellen, dass die Werte aktuell sind
    sprintf(val, "%.1f", power.house);
    mqttClient.publish("esp32solar/power/powerHouse", val);

    sprintf(val, "%.1f", power.seGrid);
    mqttClient.publish("esp32solar/power/seGrid", val);

    sprintf(val, "%d", power.blueInverter);
    mqttClient.publish("esp32solar/power/BluettiOutInverter", val);

    sprintf(val, "%.1f", power.seBatDischarging);
    mqttClient.publish("esp32solar/power/SolarEdgeDischarge", val);

    // Akkus

    sprintf(val, "%d", power.bluettiPercent);
    mqttClient.publish("esp32solar/akkus/BluettiSoe", val);

    sprintf(val, "%.1f", power.seSoe);
    mqttClient.publish("esp32solar/akkus/SolarEdgeSoe", val);

    sprintf(val, "%.1f", power.sePowerBat);
    mqttClient.publish("esp32solar/akkus/ladeLeistungSolarEdge", val);

    sprintf(val, "%d", power.ladeLeistungBluetti);
    mqttClient.publish("esp32solar/akkus/ladeLeistungBluetti", val);
    
    // Panels
    sprintf(val, "%d", power.deyeInverter);
    mqttClient.publish("esp32solar/panels/solarDeye", val);

    sprintf(val, "%d", power.bluettiIn);
    mqttClient.publish("esp32solar/panels/solarBluetti", val);

    sprintf(val, "%.1f", power.seSolar);
    mqttClient.publish("esp32solar/panels/solarEdge", val);

    sprintf(val, "%.1f", power.totalSolar);
    mqttClient.publish("esp32solar/panels/solarTotal", val);

    // Schaltung
    if (ladeStatus == LadeStatus::BluettiOnly)
        mqttClient.publish("esp32solar/panels/ladeStatus", "BluettiOnly");
    else if (ladeStatus == LadeStatus::BluettiDeye)
        mqttClient.publish("esp32solar/panels/ladeStatus", "BluettiDeye");
    else
        mqttClient.publish("esp32solar/panels/ladeStatus", "DeyeOnly");
}

//alle daten, die am Anfang gesendet werden
void sendAvailableData(AsyncWebSocketClient * client, AsyncWebSocket *server)
{
   //sende die Daten an den Client
   char json[512];
   wsMessage(startmeldungen.htmlLines().c_str(),client);
   String msg = String("WebSocket client ") + String(client->id()) + String(" connected from ") +  client->remoteIP().toString();
   wsMsgSerial(msg.c_str());
   msg = String("Anzahl Clients: ") + server->count();
   wsMsgSerial(msg.c_str());
   power.getJSON(json,sizeof(json));
   ws.textAll(json);
   informClients();
}

//bluetti fertig stellen
//der callback - vielleicht besser 
//von der Bluetti-Klasse erben und mit virtuellen Methode arbeiten?
void bleNotifyCallback(const char * topic , String value)
{
  //Serial.println("We have topic " + topic + " and val " + value);
  char out[256]; //ohne static, wg nebenläufigkeits problem
  #ifdef DEBUG
  strcpy(out,"in Callback, topic is");
  strcat(out,topic);
  wsMsgSerial(out);
  #endif
  if (!strcmp(topic,"total_battery_percent"))
  {
    power.bluettiPercent = value.toInt();
    power.eBluetti = false;

     /* char dbg[64];
      sprintf(dbg, "bluettiPercent raw: %s", value.c_str());
      wsMsgSerial(dbg);
     */
  
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
  if (mqttClient.connected()) return;
  
  long now = millis();
  static long lastReconnectAttempt = 0;
  
  if (now - lastReconnectAttempt < 5000) return; // nur alle 5s versuchen
  lastReconnectAttempt = now;
  
  Serial.print("Attempting MQTT connection...");
  if (mqttClient.connect("ESP32Solar")) {
    Serial.println("connected");
  } else {
    Serial.print("failed, rc=");
    Serial.println(mqttClient.state());
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
  char out[256]; //wieder besser ohne static
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

//inverter der Bluetti dazu schalten
void handleBlueInverter()
{
    // nur wenn autoBlueInverter aktiv
    if (!autoBlueInverter) return;

    // Ausschalten wenn Panels Bluetti laden
    if (ladeStatus != LadeStatus::DeyeOnly)
    {
        if (power.bluettiDCState)
        {
            blue.switchOut((char *) "dc_output_on", (char *) "off");
            blue.handleBluetooth();
            servoStatus = ServoStatus::Stop;
            servo.write((int) servoStatus);
            adjustBluettiFlag = false;
            wsMsgSerial("BlueInverter: Panels laden Bluetti, schalte Inverter ab");
        }
        return;
    }

    // Ausschalten wenn Bluetti zu leer
    if (power.bluettiPercent <= power.minPercentBlue)
    {
        if (power.bluettiDCState)
        {
            blue.switchOut((char *) "dc_output_on", (char *) "off");
            blue.handleBluetooth();
            servoStatus = ServoStatus::Stop;
            servo.write((int) servoStatus);
            adjustBluettiFlag = false;
            wsMsgSerial("BlueInverter: Bluetti zu leer, schalte ab");
        }
        return;
    }

    // Einschalten wenn Bedarf
    if ((power.seGrid < -50.0 || power.sePowerBat < -50.0)
        && power.bluettiPercent > power.minPercentBlue)
    {
        if (!power.bluettiDCState)
        {
            blue.switchOut((char *) "dc_output_on", (char *) "on");
            blue.handleBluetooth();
            wsMsgSerial("BlueInverter: Bedarf erkannt, schalte Inverter ein");
        }
        return;
    }

    // Ausschalten wenn kein Bedarf mehr
    if (power.seGrid > 30.0 && power.sePowerBat >= 0)
    {
        if (power.bluettiDCState)
        {
            blue.switchOut((char *) "dc_output_on", (char *) "off");
            blue.handleBluetooth();
            servoStatus = ServoStatus::Stop;
            servo.write((int) servoStatus);
            adjustBluettiFlag = false;
            wsMsgSerial("BlueInverter: kein Bedarf mehr, schalte ab");
        }
    }
}
//Falls Zustand ok die Bluetti einschalten und Leistung anpassen.
void handleAdjustBluetti()
{
  char out[256];
  static bool firstCall = true;
  static unsigned long lastAction = 0;
  static bool switchOnSent = false;

  if (!adjustBluettiFlag) return;

  if (firstCall)
  {
    strcpy(out,"check blue adjust - ");
    strcat(out,power.getString());
    wsMsgSerial(out);
    firstCall = false;
  }
  else
  {
    sprintf(out,"(%d)",power.blueInverter);
    wsMsgSerialNLB(out);
  }

  // Abbruchbedingungen
  if (power.eBluetti || power.bluettiPercent <= power.minPercentBlue)
  {
    adjustBluettiFlag = false;
    firstCall = true;
    switchOnSent = false;
    return;
  }


  // Netzbezug vorhanden und unter maxPowerBlue -> erhöhen
  if ((power.seGrid < -50.0 || power.sePowerBat < -50.0) && power.blueInverter < power.maxPowerBlue)
  {
    if (servoStatus != ServoStatus::Left)
    {
      servoStatus = ServoStatus::Left;
      servo.write((int) servoStatus);
    }
    unsigned long now = millis();
    if (now - lastAction < 1000) return;
    lastAction = now;
    wsMsgSerialNLB("(+)"); //zeige Erhöhung    
    return;
  }

  // kein Netzbezug mehr oder maxPowerBlue erreicht -> verringern
  if (power.seGrid > 30.0 || power.blueInverter > power.maxPowerBlue)
  {
    // abschalten wenn kaum noch Last
    if (power.seGrid > 30.0 && power.blueInverter < 40)
    {
      servoStatus = ServoStatus::Stop;
      servo.write((int) servoStatus);
      adjustBluettiFlag = false;
      firstCall = true;
      switchOnSent = false;
      ws.textAll("{\"action\":\"confirm\",\"topic\":\"adjustBluettiDone\"}");
      lastAction = millis();
      return;
    }
    // sonst verringern
    if (servoStatus != ServoStatus::Right)
    {
      servoStatus = ServoStatus::Right;
      servo.write((int) servoStatus);
    }
    unsigned long now = millis();
    if (now - lastAction < 1000) return;
    lastAction = now;
    wsMsgSerialNLB("(-)"); //zeige verringerung    
    return;
  }

  // Servo stoppen - im Lot (-30 < seGrid < 30)
  if (servoStatus != ServoStatus::Stop)
  {
    servoStatus = ServoStatus::Stop;
    servo.write((int) servoStatus);
    wsMsgSerial("adjust Blue - Servo stop, im Lot");
    firstCall = true;
    ws.textAll("{\"action\":\"confirm\",\"topic\":\"adjustBluettiDone\"}");
  }
}

void handleChargeSelect()
{
  if (!chargeSelectFlag) return;
  chargeSelectFlag = false;

  char out[256];

  // Priorität 1: Bluetti laden wenn genug energie
  if (power.bluettiPercent < 98.0 
      && power.seSoe > 98.0
      && power.seGrid > power.seGridMinCharge)//200 im Standard
  {
    if (power.seGrid > power.seGridBothCharge)//400 im Standard
    {
      schalteLaden(LadeStatus::BluettiOnly);
      wsMsgSerial("AutoCharge: viel Überschuss -> beide Panels auf Bluetti");
    }
    else
    {
      schalteLaden(LadeStatus::BluettiDeye);
      wsMsgSerial("AutoCharge: Überschuss -> ein Panel Bluetti, ein Panel Deye");
    }        
    return;
  }

  // Priorität 2: Bluetti unterstützt SolarEdge
  if ((power.seGrid < -50.0 || power.sePowerBat < -50.0 )
        && power.bluettiPercent > power.minPercentBlue)
  {
    schalteLaden(LadeStatus::DeyeOnly);
    wsMsgSerial("AutoCharge: Netzbezug -> Bluetti unterstützt");
    return;
  }

  // Sonst: alles auf Deye, Bluetti aus
  schalteLaden(LadeStatus::DeyeOnly); 
}

//nachricht vom client, stelle auf statische größen um, um heap-Fragmentierung zu vermeiden, keine Ahnung, ob
//das tatsächlich ein Thema ist, aber ich kenne ja genau die maximale Websocket message, sie kommt ja vom eigenen
//JS-Interfaces, ich sende action mit String und einen Wert
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) 
{
  AwsFrameInfo *info = (AwsFrameInfo*)arg; 
  //static char out[256]; //chat gpt sagt, dass static ein problem sein könnte, wenn messages von unterschiedlichen clients herein kommen, 
                        //da die Methode noch nicht durch ist. Ich dürfte mit lokalen Buffern kein Problem vom platz her haben, lege also 
                        //diese an. Chat gpt spricht von ein paar kb Stack pro Funktion, genauer: AsyncWebServer-Tasks haben typischerweise 4–8 KB Stack
  //static char in[256]; //duerfre reichen 
  char out[256];
  char in[256]; 
  if (len >= 255) {   //
    wsMsgSerial("WS message too large, should be impossible");
    return;
  }


  strcpy(out,"{\"action\":\"confirm\",\"topic\":\"");
  int confirmLength = strlen(out);
  memcpy(in, data, len);
  in[len] = '\0';

  char confirmEnd[] = "\"}";
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) 
  {
    
    //data[len] = 0; //sollte ein json object als String sein, uups Grenze nicht bedacht.

    char *cData = (char *)in; //data; 
    //String s = cData; //legt eine Kopie an    
    //const int capacity = JSON_OBJECT_SIZE(3)+2*JSON_OBJECT_SIZE(3);//+JSON_ARRAY_SIZE(RFID_MAX); //
    
    JsonDocument doc; //hinweis: doc nur einmal verwenden 
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
            
            //if (power.bluettiDCState && !power.eBluetti) //nur wenn Bluetti auch an ist
            { //zum test heraus
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
            /*
            else 
            { 
              strcat(out,"servoStop");
              servoStatus = ServoStatus::Stop;
            } 
            */ 
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
      else if (!strcmp(doc["action"], "autoCharge")) //automatische Anpassung solareinspeisung
      {
        if (!strcmp((const char * )doc["value"],"on"))
        {
          autoCharge = true; //schon mal setzen, auch wenn es noch nicht stimmt, was meinte ich damit :-)
          strcat(out,"autoChargeOn");
        }
        else 
        {
          autoCharge = false; 
          strcat(out,"autoChargeOff");
        }
        preferences.putBool("autoCharge",autoCharge);         
      }
      else if (!strcmp(doc["action"], "autoBlueInverter"))
      {
          if (!strcmp((const char *) doc["value"], "on"))
          {
              autoBlueInverter = true;
              strcat(out, "autoBlueInverterOn");
          }
          else
          {
              autoBlueInverter = false;
              strcat(out, "autoBlueInverterOff");
          }
          preferences.putBool("autoBlueInv", autoBlueInverter);
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
      else if (!strcmp(doc["action"],"seGridMinCharge"))
      {
          power.seGridMinCharge = doc["value"];
          preferences.putInt("seGridMin", power.seGridMinCharge);
      }
      else if (!strcmp(doc["action"],"seGridBothCharge"))
      {
          power.seGridBothCharge = doc["value"];
          preferences.putInt("seGridBoth", power.seGridBothCharge);
      }
      else if (!strcmp(doc["action"],"status"))
      {

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
  char out[256];
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
  autoBlueInverter = false;
  preferences.putBool("autoBlueInv", autoBlueInverter);
  power.maxPowerBlue = 100;
  preferences.putInt("maxPowerBlue",power.maxPowerBlue );
  power.minPercentBlue = 10;
  preferences.putInt("minPercentBlue",power.minPercentBlue );
  intervalAutoAdjust = 120;
  preferences.putInt("intAutoAdjust",intervalAutoAdjust );
  intervalAutoCharge = 120;
  preferences.putInt("intAutoCharge",intervalAutoCharge );
  power.seGridMinCharge = 200;
  preferences.putInt("seGridMin", 200);
  power.seGridBothCharge = 400;
  preferences.putInt("seGridBoth", 400);
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
    request->send(200, "text/html", index_html, processor);
  });
  ws.onEvent(onWSEvent);
  server.addHandler(&ws); //WebSocket dazu 
  ElegantOTA.begin(&server);   server.begin();
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
  autoBlueInverter = preferences.getBool("autoBlueInv", false);
  power.maxPowerBlue  = preferences.getInt("maxPowerBlue",100);
  power.minPercentBlue  = preferences.getInt("minPercentBlue",10);
  power.seGridMinCharge  = preferences.getInt("seGridMin", 200);
  power.seGridBothCharge = preferences.getInt("seGridBoth", 400);
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
  handleChargeSelect(); //zuerst, sorgt ggf. dafür das Bluetti nicht angeschaltet wird. 
  handleBlueInverter(); //dann dieser
  handleAdjustBluetti();//und zu letzt
  
  
  long now = millis();
  long delta = now - lastMsg;
  if (delta > 2000) //alle 2 Sekunden Information heraus und auf jeden Fall Bluetti adjust wenn power house < -50 
  {
    lastMsg = now;
    power.actualizeData();
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
  if (delta > 1000 * intervalAutoAdjust && autoAdjustBlue && ladeStatus == LadeStatus::DeyeOnly)
  //nur anpassen, wenn Bluetti nicht  lädt, denn dann braucht sie nicht einspeisen, wir haben überschuss.
  {
    lastBluettiAdjust = now;
    adjustBluettiFlag = true;
  }
  

  delta = now -  lastAutoCharge ;
  if (delta > 1000 * intervalAutoCharge && autoCharge )
  {//besser auch auf flag umstellen
    lastAutoCharge = millis();
    chargeSelectFlag = true;
  }
  /*
  else if (power.house < -30 && autoCharge)
    chargeSelectFlag = true;
  */
}
