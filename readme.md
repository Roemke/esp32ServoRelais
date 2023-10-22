Projekt aus meinem RFID-Relais Projekt kopiert

Ziel: 
 - Servo steuern (poti fuer den Inverter am Bluetti drehen)
 - bluetti auslesen und darstellen
 - Wert aus Tasmota IR-Kopf auslesen und darstellen 
 - Wert aus Tasmota Steckdose Bluetti-Inverter 
 - Wert aus Tasmota Steckdose Solar-Inverter (deye) 
 - bisschen Automatik einbauen 

Die Tasmota-Devices senden per mqtt an fhem, verwende die Werte hier nicht mehr, frage direkt ab. 

Bisher läuft 
 - Der Webserver mit websockets (da ich ihn vorher hatte)
 - Power wird dargestellt, habe pubsub mqtt client genommen
 - Servo lässt sich testweise drehen
 - Werte aus mqtt werden dargestellt
 - Relais lassen sich testweise schalten
 - bluetti wird ausgelesen und schalten geht 
 - Bluetti von mqtt abgekapselt, eigene "Bibliothek" geschrieben
   ist nicht perfekt aber geht
 - alles auf github geschoben     
 - dieses gerät kann als mqtt in fhem eingebunden werden, es bekommt von fhem 
   die daten gesendet, dies nur im mqtt branch, der seit ein paar Wochen nicht mehr weiter entwickelt wird. 
 - umgestellt auf webapi der verschiedenen tasmota-devices, eine Steckdose am Deye-Inverter, eine Steckdose am Inverter, der über 12 Volt von der Bluetti versorgt wird, ein IR-Lesekopf mit 
   spezieller Tasmota-Version um den Stromzähler auszulesen
 
littlefs macht aerger - seltsam - brauche ich das? - erstmal raus

Rahmenbedingungen hier: 
 Grundverbrauch ca. 80 W wenn nichts läuft (ohne Heizung, die nimmt sich noch etwas9
 Speicherwürfel Bluetti kann einspeisen, zwischen 0 und 150 Watt ist es sinnvoll
 geht nur bis 150-200 Watt, dann heizt der Inverter zu viel - von wegen 500 W
 die Leistung, die der Inverter liefert und die die Bluetti liefert weichen 
 immer stärker voneinander ab. 

Erste Überlegungen:
 sinnvolle automatisierung wäre (Bluetti bei 10% ausschalten, auch wenn sie selbst abschaltet, 
 und Lithium tiefentladung eigentlich egal ist, wer weiß wie gut das geht) 
 Wenn Solar <= Hausverbrauch oder Hausverbrauch groesser als 400 Watt 
    Verbrauchen, beide Panels am Deye-Inverter
    regle Bluetti Inverter (wenn mehr als 10% )
 sonst 
    wenn Bluetti <=95%
       wenn Hausverbrauch groesser als  200 Watt 
            ein Panel an Deye, eines an Bluetti (mehr als 400 s. oben)
            evtl. regle Bluetti dazu 
       sonst  
         Lade Bluetti mit 2 Panels und regle Bluetti Inverter
    sonst 
       beide Panels an Deye Inverter
Irgendetwas in der obigen Richtung umgesetzt :-)
Manuelles schalten sollte möglich sein, Automatik dann aus (nicht getan) 
Regelung des Inverters mittels Servos sollte eine Art hysterese berücksichtigen, damit das Teil nicht dauernd dreht, nein, ist nicht zwingend funktioniert auch so, checke nur alle paar Minuten die Anpassung. 


Schaltung mit 4er Relais, für Voll laden von Bluetti müssen die Panels in Reihe geschaltet sein
der Deye-Inverter hat 2x2 Eingänge für je ein Panel 

quer richtung normally open, also ein, wenn Relais "an geschaltet"

      Panel1-   Panel1+           Panel2-  Panel2+ 
       |           |in              |in        |
       |           R2---------------R3         |in
       |in         |nc              |nc        R4------- Blue + 
       R1---------------------------x----------|-------- Blue - 
       |nc         |                |          |nc     
       |           |                |          |        
       |           |-------         |          |
       |                  |         |          |
       Deye1-   Deye2- ---|---------|          |
                          |                    |
       Deye1+   Deye2+ ---|--------------------|
         |----------------|
       
       
       R1-R4 geschaltet:           Bluetti wird voll geladen 

       R1-R4 nicht geschaltet:     Deye1 wird versorgt und Deye2 wird versorgt
  
       R1,R2,R3 nicht geschaltet      Deye 1 wird versorgt und Blue über Panel 2geladen 
       R4 geschaltet  
       
       R1 geschaltet, R2 nicht, evtl. doof Deye1 hat + aber keinen Bezug , also R2 vor R1 schalten (sollte aber egal sein)
       R2 geschaltet, R3 nicht, evtl. doof + P1 liegt auf - Deye2 - nicht machen R3 vorher schalten  
       R1 geschaltet, R3 nicht, evtl. auch doof Panel1 und Panel 2 bekommen gemeinsames - mögen sie das? (auch egal?) 
       
       also beim Wechsel Bluetti -> Deye Reihenfolge R1,2,3, ggf. 4 aus
       beim Wechsel von Bluetti + deye auf Bluetti -> Reihenfolge R3, R1, R2 ein
       Wechsel Blue + Deye auf Deye -> R1,R2,R3, R4 aus  
       Wechsel Deye auf Blue -> R3, R2, R4, R1 einschalten (hier wichtig R3 vor R2)
       das müsste es doch sein 
       

schade, nachdem sehr viel lief habe ich updates laufen lassen, da die arduino ide herum zickte (fehlermeldungen)
und bin auf arduino 2.1 gewechselt. jetzt bekomme ich den Servo nicht mehr zum laufen 
Bluetooth funktioniert auch nicht mehr, scheint sich aufzuhängen, warning beim compilieren

warning beim compilieren lässt sich eliminieren, durch verwendung anderer methode, aber das ganze
funktioniert nicht mehr, ich bekomme keine Verbindung mehr zu dem Teil.
Problem ist natürlich auch, dass ich die Bluetooth funktionalität kopiert habe um dann eine eigene klasse zu bauen, ohne die Thematik wirklich zu verstehen.

Nein, das stimmt so nicht, der connect funktioniert, den Write Request kann ich senden, offenbar liefert Bluetti Bluetooth dann eine Antwort - jedenfalls wenn man die 1.4.0 der Bibliothek verwendet, in der 1.4.1 wird mein Callback nicht aufgerufen. Nochmal das NimBLE analysiert, erkenne keinen Fehler in meinem (kopierten) Code. Verwende halt 1.4.0 dann geht es. 
Servo das gleiche Problem, siehe readmeServo, auch hier geht eine ältere Version, die neuere will einen neuen gcc und damit läuft das compilieren des Boards nicht. 

Leider dauert es eine Zeit bis der Deye Inverter realisiert, dass er wieder Solar-Spannung hat
so 5 - 10 Minuten scheinen normal zu sein eigentlich müsste das aber doch normal sein, wenn er morgens aufwacht.

Todo: Warning AsynchTCP nicht verwenden o.ä. anscheinend unterstützen auch die normalen Libraries jetzt asynd, nicht weiter geschaut. 

