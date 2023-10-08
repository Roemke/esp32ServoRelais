
#include <Servo.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <Preferences.h>        
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>//mist, der braucht den ESPAsyncWebServer, habe mal in der Bibliothek angepasst, so dasss es auch mit ESPAsyncWebSrv.h geht
#include <ArduinoJson.h>   //und wieder zurück, nehme den AsyncWebServer Bibliothek manuell installiert aus zip
#include <PubSubClient.h>
#define DEBUG 1
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

Servo servo;
static const int servoPin = 15;//ist io15 = tdo

//und die Relais
const int r1pin = 26; 
const int r2pin = 18;
const int r3pin = 19;
const int r4pin = 23;


AsyncWebServer server(80);
//fuer den Websocket
AsyncWebSocket ws("/ws");
unsigned long keepWebServerAlive = 0; //240000; //milliseconds also 4 Minuten, danach wird wifi abgeschaltet.
//in dieser Zeit ein client connect -> der Server bleibt aktiv, 0 er bleibt aktiv, lohnt nicht, bringt bei 12 V nur 6mA gewinn
unsigned long startTime;


//ein wenig was zum OTA Update, nicht getestet, keine Ahnung, ob das mit dem Asynchronous Webserver funktioniert
//nein, es geht nicht, es geht mit dem AsyncWebServer einfacher

//---------------------------------------------
//und fuer mqtt
const char* mqttServer = mqttBROKER;
const int mqttPort = 1883;
long lastMsg = 0;
WiFiClient wifiClient; //nicht ganz klar - ein Client der die Verbindung nutzen kann
PubSubClient mqttClient(wifiClient); 

//mqtt funktioniert so weit erstmal, kann ich das auf http-Client umstellen + Bluetooth - ich kann ja eigentlich
//alle Werte sinnvoll über http abfragen, es sind alles tasmota teile, dann bin ich nicht auf mqtt angewiesen 
//mqtt auf tasmota aktualisiert nur alle 10 Sekunden mache mal einen branch mqtt - der ist nicht fertig, wenn http geht
//wird er auch nicht weiter entwickelt :-)

//bluetti fertig stellen
//der callback - vielleicht besser 
//von der Bluetti-Klasse erben und mit virtuellen Methode arbeiten?
void bleNotifyCallback(char * topic , String value)
{
  //Serial.println("We have topic " + topic + " and val " + value);
  char * str = 0;
  const char * end = "}";
  if (!strcmp(topic,"total_battery_percent"))
  {
    const char * begin = "{\"action\":\"bluettiPercent\",\"data\":";
    str = new char[strlen(begin) + value.length() + strlen(end) + 24 ];
    strcpy (str,begin);
    strcat (str,value.c_str());
    strcat (str,end);
  }
  else if (!strcmp(topic, "dc_input_power"))
  {
    const char * begin = "{\"action\":\"blue_dc_input_power\",\"data\":";
    str = new char[strlen(begin) + value.length() + strlen(end) + 24 ];
    strcpy(str,begin);
    strcat (str,value.c_str());
    strcat (str,end);      
  }
  /* an aus ist überflüssig - nee doch nicht, muss nur nicht angezeigt werden */
  else if (!strcmp(topic , "dc_output_on"))
  {
    const char * begin = "{\"action\":\"bluettiDC\",\"data\":";
    str = new char [strlen(begin) + 24 ];
    strcpy(str,begin);
    if (value=="0")
      strcat(str,"\"off\"");
    else
      strcat(str,"\"on\"");
    strcat(str,end);
  }
  else if (!strcmp(topic, "dc_output_power"))
  {
    const char * begin = "{\"action\":\"blue_dc_output_power\",\"data\":";
    str = new char[strlen(begin) + value.length() + strlen(end) + 24 ];
    strcpy(str,begin);
    strcat (str,value.c_str());
    strcat (str,end);    
  }
  if (str)  
    ws.textAll(str);

  delete [] str;
}

Bluetti blue("AC200M2308002058882",bluettiCommand,bleNotifyCallback); //die bluetoothid, das command fuer das modell und der Callback 



void mqttCallback(char* topic, byte* message, unsigned int length) {
  //folgendes spaeter raus, ist viel
  /*
   * Serial.print("Message arrived on topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
  }
  Serial.println();
  */

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (strcmp(topic,"tele/DVES_352360/SENSOR")==0) //Strom-Messgerät liefert JSON-Format
  {
    //interessant: https://arduinojson.org/v6/assistant/#/step1 - gibt 96 fuer meinen String an und bei i(nformation) meine unten überlegte capacity
    //danach gibt er noch quelltext - nett 
    //{"Time":"2023-08-26T11:10:53","Power":{"Meter_Number":"0a014954520003504c4c","Total_in":3460.0965,"Power_curr":1710,"Total_out":62.0456}}
    /*wandle nicht um, sondern sende die Nachricht einfach komplett 
    const int capacity = JSON_OBJECT_SIZE(2)+JSON_OBJECT_SIZE(4);
    //ein json-objekt mit 2 membern, eines der member ist ein object mit 4 membern
    StaticJsonDocument<capacity> doc; 
    DeserializationError error = deserializeJson(doc, message, length);
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }    
    const char* Time = doc["Time"]; // "2023-08-26T11:10:43"
    JsonObject Power = doc["Power"];
    const char* Power_Meter_Number = Power["Meter_Number"]; // "0a014954520003504c4c"
    //float Power_Total_in = Power["Total_in"]; // 3460.092
    //int Power_Power_curr = Power["Power_curr"]; // 1702
    //char powerString[] = "{\"action\":\"powerHouse\",\"power\":\"xxxxxxxxxxxxxxxxx\"}" );
    sprintf(powerString "{\"action\":\"powerHouse\",\"power\":\"%d\"}",Power["Power_curr"] );
    float Power_Total_out = Power["Total_out"]; // 62.0456
    */
    const char * begin = "{\"action\":\"powerHouse\",\"data\":";
    const char * end = "}";
    char * str = new char[length+strlen(begin)+strlen(end)+24];//Puffer +24 statt +1 :-)
    strcpy(str,begin);
    int start = strlen(str);
    //strcat(str,(char *)message); message ist nicht null-terminated
    for (int i = 0; i < length; i++) 
      str[start++] = (char) message[i];
    str[start]=0;
    strcat(str,end);
    //Serial.print("Sende ");
    //Serial.println(str);
    ws.textAll(str);
    
    delete [] str;          
  }
  else if (strcmp(topic,"tele/DVES_17B73E/SENSOR")==0)//tasmota bluetti inverter
  { //das ist doch alles murcks, obwohl ich mit telePeriod 10 in der console von tasmota den wert angepasst habe 
    //und tasmota laut log in der console alle 10s sendet, bekomme ich hier nicht bescheid
    //auch fhem sendet angeblich regelmäßig mqtt2 traffic ist vorhanden im log - fehler an buffer-size, die war bei mqtt2 zu klein (hier)
    const char * begin = "{\"action\":\"powerBluettiInverter\",\"data\":";
    const char * end = "}";
    char * str = new char[length+strlen(begin)+strlen(end)+24];//Puffer +24 statt +1 :-)
    strcpy(str,begin);
    int start = strlen(str);
    //strcat(str,(char *)message); message ist nicht null-terminated
    for (int i = 0; i < length; i++) 
      str[start++] = (char) message[i];
    str[start]=0;
    strcat(str,end);
    //Serial.print("Sende ");
    //Serial.println(str);
    ws.textAll(str);
    
    delete [] str;              
  }
  else if (strcmp(topic,"tele/DVES_183607/SENSOR")==0)
  {
    const char * begin = "{\"action\":\"powerSolarDeye\",\"data\":";
    const char * end = "}";
    char * str = new char[length+strlen(begin)+strlen(end)+24];//Puffer +24 statt +1 :-)
    strcpy(str,begin);
    int start = strlen(str);
    //strcat(str,(char *)message); message ist nicht null-terminated
    for (int i = 0; i < length; i++) 
      str[start++] = (char) message[i];
    str[start]=0;
    strcat(str,end);
    //Serial.print("Sende ");
    //Serial.println(str);
    ws.textAll(str);
    
    delete [] str;                  
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect("ESP32Solar")) {
      Serial.println("connected");
      // Subscribe
      mqttClient.subscribe("tele/DVES_17B73E/SENSOR");
      mqttClient.subscribe("tele/DVES_352360/SENSOR");//power haus ist tasmota, das kann ich so abgreifen
      mqttClient.subscribe("tele/DVES_9C2197/SENSOR");//alles andere an tasmota devices geht nicht, seltsam buffer problem
      mqttClient.subscribe("tele/DVES_183607/SENSOR");
      
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
  mqttClient.setCallback(mqttCallback);
}


//------------------------------------------
//viele Funktionen anonym, bzw. als Lambda Ausdruck, hier der Rest zum Server-Kram
void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found - ich kann das nicht");
}

String processor(const String& var)
{
  /*if(var == "NEW_RFID_LINES")
  {
    String lines = rfidsNew.htmlLines("add");
    return lines;
  }*/
  String result = "";
 
  if (var == "UPDATE_LINK")
  { 
    result = "<a href=\"http://" + WiFi.localIP().toString() +"/update\"> Update </a>";
  }
  return result;
}


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

void wsMsgSerial(const char *message, AsyncWebSocketClient * client = 0)
{
  wsMessage(message,client);
  Serial.println(message);
}


//alle daten, die am Anfang gebraucht werden 
void sendAvailableData(AsyncWebSocketClient * client, AsyncWebSocket *server)
{
   //sende die Daten an den Client
   wsMessage(startmeldungen.htmlLines().c_str(),client);
   String msg = String("WebSocket client ") + String(client->id()) + String(" connected from ") +  client->remoteIP().toString();
   wsMsgSerial(msg.c_str());
   msg = String("Anzahl Clients: ") + server->count();
   wsMsgSerial(msg.c_str());
}

//bei einem connect muesste ich doch eigentlich die notwendigen Daten an den speziellen client 
//senden koennen. Ich war ein wenig in der "Ajax-Denke" gefangen - da hat der Client die Daten 
//abgerufen, das ist hier aber unnoetig und fuehrt zu weiteren get-Geschichten im behandeln 
//der events. 
void onWSEvent(AsyncWebSocket       *server,  //
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
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) 
{
    AwsFrameInfo *info = (AwsFrameInfo*)arg; 
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) 
    {
        data[len] = 0; //sollte ein json object als String sein        
        char *cData = (char *)data;
        //String s = cData; //legt eine Kopie an
        String message;
        const int capacity = JSON_OBJECT_SIZE(3)+2*JSON_OBJECT_SIZE(3);//+JSON_ARRAY_SIZE(RFID_MAX); //
        //bisher ein Objekt mit 3 membern und RFID_MAX Objekte mit 3 membern (die authorisierten rfids, maximal RFID_MAX), aber das ist ein Array mit RFID_MAX Objekten, 
        //lieber auf Nr sicher gehen
        //Serial.println("Rfids1");
        //rfidsOk.serialPrint();
        //Serial.println("new");
        //rfidsNew.serialPrint();
        
        StaticJsonDocument<capacity> doc; //hinweis: doc nur einmal verwenden 
        Serial.println("we have in WebSocketMessage: ");
        Serial.println(cData);
        
        DeserializationError err = deserializeJson(doc, cData); //damit veraendert das JSON-Objekt den Speicher cData
        if (err) //faengt auch Probleme beim Speicherplatz ab. 
        {
          Serial.print(F("deserializeJson() failed with code "));
          Serial.println(err.f_str());
          ws.textAll("Fehler beim Derialisieren");//erhalte die Nachricht, javascript hat dann natuerlich einen json-error, das ist ok, dann sieht man es :-)
          ws.textAll(err.f_str());
        }
        else
        {
          if(strcmp(doc["action"],"clearNew") == 0)
          {
            //rfidsNew.clear();
            ws.textAll("{\"action\":\"clearNew\"}");
            wsMsgSerial("Neu gelesene geloescht");
          }
          else if (strcmp(doc["action"],"dc_output_on") ==0)
          {
              //String state =  (const char *) doc["value"];
              blue.switchOut("dc_output_on", (const char *) doc["value"]);  
              //info an angeschlossene devices / smartphone muesste ueber bluetti(bluetooth)->dieser esp(websocket)-> angeschlosse devices laufen
          }
          else if ( strcmp(doc["action"],"servo")==0)
          {
              //steuere mal den Servo an   
                //lese Wert von Serial
                //wsMsgSerial("Warte auf eingabe");
                //while (Serial.available()==0){}             // wait for user input
                //int val = Serial.parseInt();                    //Read user input and hold it in a variable
                //char buffer[64] = "";
                //sprintf(buffer,"Habe Wert %d gelesen", val);
              
                //mein servo 98:  langsam links  97 er ruckelt
                //87 ruckelnd links oder nichts 
                //86 langsam rechts bei 30-40 ist er schon bei max speed rechts ich denke die Bibliothek ist halt fuer normale Servos gedacht:-)
                /* die spezielle biblio bringt aber auch nichts, da sie einen parallax feedback sensor erwartet, der konstet sehr viel + viel versand */
                //geeignet: 85 langsam rechts
                //          92 stop 
                //          99 langsam links 
                // 80 und 102  passen gefühlt gut zueinander
                // 70 und 110 schneller
                // 40 und 140 max speed 
                
                //wsMsgSerial(buffer);
                int val = 92;
                if (!strcmp(doc["value"],"left")) 
                   val = 110;  
                else if (!strcmp(doc["value"],"right"))
                   val = 70; 
                
                servo.write(val);    
          }
          else if (strcmp(doc["action"],"keepWebServerAlive")==0) 
          {
            startTime = millis();
            keepWebServerAlive = (unsigned long ) doc["time"]; //0 fuer ewig
            Serial.print("Zeit : ");
            Serial.println((unsigned long ) doc["time"]);
            String msg("Zeit bis zum Stopp des Webservers: ");
            msg += keepWebServerAlive; 
            wsMsgSerial(msg.c_str());
          }
          else if (strcmp(doc["action"],"rebootESP")==0)
          {
            wsMsgSerial("Restart of ESP");
            ESP.restart();
          }
          else if (!strcmp(doc["action"],"relais"))
          {
            schalteRelais( doc["value"]);
          }
        }
    }
}


//--------------------------------------------------------------
void schalteRelais(const char * value) //muss const char * sein sonst meckert die JSON-Bibliothek
{
  char msg[64]="Bin in Relais schalten: ";
  strcat(msg,value);
  wsMsgSerial(msg);
  
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
  
  sprintf(msg,"Write to relais %s switchTo is %s, pin %d mode is %d",relais,switchTo,pin,mode);
  wsMsgSerial(msg);

  if (pin)
  {
    digitalWrite(pin,mode);
    sprintf(msg,"Write to pin %d mode is %d",pin,mode);
    wsMsgSerial(msg);
  }
}

              
//--------------------------------------------------------------------
void setup() {
  //beginn der seriellen Kommunikation mit 115200 Baud
  Serial.begin(115200);

  //Servo Geschichte 
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
    request->send_P(200, "text/html", index_html, processor);      
  });
  ws.onEvent(onWSEvent);
  server.addHandler(&ws); //WebSocket dazu 
  AsyncElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  //andernfalls startet er ohne wifi, registrieren von Elementen nicht möglich, nein habe mal einen ap-modus gesetzt  
  //eine kleine Pause von 50ms.
  delay(50);

}

//Zahlen einlesen serial



void loop() {
  if (WiFi.status() == WL_CONNECTED)
  {
    ws.cleanupClients(); // aeltesten client heraus werfen, wenn maximum Zahl von clients ueberschritten, 
                       // manchmal verabschieden sich clients wohl unsauber / gar nicht -> werden wir brutal  
  }  

  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();
  blue.handleBluetooth();

  //send information to mqtt --------------------------------------------------
  long now = millis();
  int delay = now - lastMsg;
  if (delay > 15000) 
  {
    lastMsg = now;
    mqttClient.publish("esp32solar/state","online");
    //Serial.println("try to publish esp32solar/state online");
  }  
  //---------------------------------------------------------------------------
}
