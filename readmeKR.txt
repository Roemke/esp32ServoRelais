Projekt aus meinem RFID-Relais Projekt kopiert

Ziel: 
 - Servo steuern (poti fuer den Inverter am Bluetti drehen)
 - bluetti auslesen und darstellen
 - Wert aus Tasmota IR-Kopf auslesen und darstellen 
 - Wert aus Tasmota Steckdose Bluetti-Inverter 
 - Wert aus Tasmota Steckdose Solar-Inverter (deye) (existiert noch nicht)

Die Tasmota-Devices senden per mqtt an fhem 

Bisher läuft 
 - Der Webserver mit websockets (da ich ihn vorher hatte)
 - Power wird dargestellt, habe pubsub mqtt client genommen
 - Servo lässt sich testweise drehen    
 
zu bluetti projekt gefunden und erstmal herein kopiert
Problem: das nutzt platformio - die moderne Variante, aber die verstehe ich noch nicht 
nein, tut es nicht - es hat beides

Starte mal ein neues Projekt in einem neuen Ordner, erstmal sehen, ob ich meinen bisherigen kram 
zum laufen bekomme
füge meinen code mal zur main.cpp hinzu
in platformio füge AsyncElegantOta hinzu (library manager)
-> weniger rot :-)
baue nach und nach libs ein
littlefs macht aerger - seltsam 

es geht wohl irgendwie aber ich lasse alles damit erstmal weg, da ich es nicht brauche
alles mit littlefs erstmal raus geworfen, compiliert, lädt hoch monitor geht nichth 
speed stimmt nicht
und nach ein paar weiteren versuchen geht gar nichts mehr - so ein schrott auch nicht besser als arduino 
habe in der platform io ini die geschwindigkeit auf 115200 gesetzt, neu gestartet, dann ging der monitor 
und auch upload geht wieder (lief einer Zeit lang nicht, unklar)

so komme ich auf jeden Fall nicht weeiter, ich kann das Bluetti Projekt nicht in meines einbinden, vielleicht anders heraum 
nein, vielleicht geht es, in dem ich die Arduino ide verwende, da funktionierte ja das File-System, wobei ich das eigentlich überhaupt nicht 
brauche 

Doch einiges weiter gekommen, kann jetzt das Gerät als mqtt2 device in fhem einbinden, habe ein wenig im raspiFHEM Ordner notiert
  