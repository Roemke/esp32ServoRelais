//------------------------------------------------------------------------------------------
//die haupseite, sie wird im flash abgelegt (wegen PROGMEM) R" heißt "roh", rawliteral( als trennzeichen
const char index_html[] PROGMEM = R"rawliteral(<!doctype html> 
<!-- seite wird vom esp generiert, zum testen mal als html gespeichert, geht mit chrome beachte aber Aederungen wg Prozent, s.u. -->          
<html lang='de'>
  <head>
    <meta charset='utf-8'>
    <meta name='viewport' content='width=device-width'>
    <title>Webserver Solar</title>
    <style>
     .framed { 
       border: 1px solid black; 
       padding: 1em;
      }
      li input  {
        width: 15em;
      }
      input:nth-of-type(2){
        width: 25em;
      }
      
      #divNachrichten {
        border: 1px solid blue;
        padding: 1em;
        margin-top:  1em;
        margin-left: 1em;
      }
      ul {
        border: 1px solid blue;
        padding: 1em;
        list-style-type: none;
      }
      h1 {
        font-size: 135%%; /* im Template keine Prozentangababen moeglich :-), leistet template ein..., %% - yes */
        padding: 0em 1em;
      }
      h2 {
        font-size: 110%%;
      }
      .hinweis {
        border: 2px solid darkgreen;
        padding: 2em;
      }
      #dateTime {
        font-weight: normal;
      }
      #controlContainer {
          display: grid;
          position: sticky;
          top: 0em;
          background-color: #fafaff;
          /*  grid-template-rows: 20%% 20%% 20%% 20%% 20%% ; sowieso automatisch */
           grid-template-columns: 30%% 1fr minmax(min-content,8em) 10%%;
           grid-auto-flow: column;
           padding-bottom: 1em; 
           /*justify-items: center; */             
      }
      #controlContainer h1 {
        display: inline-block;
        grid-row-start: 1; /* beachte rasterlinien, nicht zeilen, oben unten ist linie */ 
        grid-row-end  : 7; 
      }
      #controlContainer h2 {
          font-size: 120%%;
      }
      #controlContainer button {
        grid-column-start: 3;
        margin-top: 0.5em;
      }
      #controlContainer label {
        grid-column-start: 3;
        margin-top: 0.5em;
      }
      #status {
        padding: 1em; 
        grid-row: 1 / 7 ; /* kurzform */ 
        grid-column-start:4; 
      }
      #sSymbol {
        font-size:200%%;
      }
    </style>
    <script>
      let gateway = `ws://${window.location.hostname}/ws`;
      var websocket;
      let bluettiOn = false;
      function htmlToElement(html) 
      {
        var template = document.createElement('template');
        html = html.trim(); // Never return a text node of whitespace as the result
        template.innerHTML = html;
        return template.content.firstChild;
      }

      
      function initWebSocket()
      { 
         websocket = new WebSocket(gateway);
         websocket.onopen = () =>
         {  //hatte hier noch getAnfragen, an sich doch unnoetig, Ajax-Denke, der Server kann sie senden wenn ein client connected
            //ja das klappt
            //----------------------------
            console.log('Connection opened');
         } 
         websocket.onclose = () => 
         {
            console.log('Connection closed');
            setTimeout(initWebSocket, 2000); //wenn stress versuche reconnect - ich glaube das funktioniert nicht sauber
         }
         websocket.onmessage = (e) =>
         {
            console.log(`Received a notification from ${e.origin}`);
            console.log(e);
            console.log(e.data);
            let data =JSON.parse(e.data);
            //console.log(data);
            let now = new Date();
            switch (data.action)
            {
              case "message":
                document.getElementById("iMessage").innerHTML += data.text + "<br>";
                break;
              case "power": 
                let vDateTime = document.getElementById("dateTime"); 
                let vph = document.getElementById("valPowerHouse"); 
                let vSDP = document.getElementById("valSolarDeyePower"); 
                let vBIP = document.getElementById("valBlueInvPower");

                let vBSolarP = document.getElementById("valSolarBluePower"); 
                let vBDCP = document.getElementById("valBluePower");
                let vBP = document.getElementById("valBluettiPercent");
                
                vDateTime.innerHTML =  "(" + now.toLocaleDateString('en-CA') + 
                                       " " + now.toLocaleTimeString('de-DE') + ")";
                

                vph.innerHTML = (!data.values.eHouse) ? data.values.powerHouse + " W " : '?'; 
                vSDP.innerHTML = (!data.values.eDeyeInverter) ? data.values.powerDeyeInv + " W I." : '?';
                vBIP.innerHTML = (!data.values.eBlueInverter) ? data.values.powerBlueInv + " W I." : '?';                                
                                 
                vBSolarP.innerHTML = (!data.values.eBluetti) ? data.values.bluettiIn + " W" : '?'; 
                vBDCP.innerHTML = (!data.values.eBluetti) ? data.values.bluettiOut + " W": '?'; 
                vBP.innerHTML = (!data.values.eBluetti) ? data.values.bluettiPercent + " %": "?" ;
                
              break;
            } 
         }
      }
      
      window.addEventListener("load", () => 
      {
        initWebSocket();
        document.getElementById('bReboot').addEventListener("click",() =>
        {
           websocket.send(JSON.stringify({'action':'rebootESP'}));
        });
        document.getElementById('bClear').addEventListener("click",() => 
        { 
          document.getElementById('iMessage').innerHTML = "";
        });        
        document.getElementById('bServoLeft').addEventListener("click",() => 
        {
           websocket.send(JSON.stringify({'action':'servo','value':'left'}));
        });
        document.getElementById('bServoRight').addEventListener("click",() => 
        {
           websocket.send(JSON.stringify({'action':'servo','value':'right'}));
        });
        document.getElementById('bServoStop').addEventListener("click",() => 
        {
           websocket.send(JSON.stringify({'action':'servo','value':'stop'}));
        });
        document.getElementById('bToggleDC').addEventListener("click",() => 
        { 
            websocket.send(JSON.stringify({'action':'dc_output_on','value':'toggle'}));
        });
        //anpassen der Leistung von Bluetti ins Hausnetz
        document.getElementById('bAdjustBluetti').addEventListener("click",() => 
        { 
            websocket.send(JSON.stringify({'action':'adjustBluetti','value':''}));
        });
        //Ladeverhalten, Relais schalten
        document.getElementById('bBluettiOnly').addEventListener("click",() => 
        { 
            websocket.send(JSON.stringify({'action':'changeCharge','value':'bluettiOnly'}));
        });
        document.getElementById('bDeyeOnly').addEventListener("click",() => 
        { 
            websocket.send(JSON.stringify({'action':'changeCharge','value':'deyeOnly'}));
        });
        document.getElementById('bDeyeBluetti').addEventListener("click",() => 
        { 
            websocket.send(JSON.stringify({'action':'changeCharge','value':'bluettiDeye'}));
        });
        //--------------
        //die relais testen -> buttons im form, im Betrieb nicht sinnvoll, die Relais dürfen nicht unabhängig geschaltet werden !
        /*
        let fButtons = fRelais.querySelectorAll("#fRelais > button"); //sollten 8 sein 
        fButtons.forEach( element => 
        {
           element.addEventListener("click", evt =>
           {
             console.log("click on event listener of Relais-Button " + evt.target.id);
             websocket.send(JSON.stringify({'action':'relais','value':evt.target.id}));
           });
        });
        */
         
      }); 
    </script>
  </head>  
  <body style='font-family:Helvetica, sans-serif'> 
    <div id='controlContainer'>
        <h1>Solar / Power Control </h1>
        <button type="button" id="bReboot">reboot Esp</button>
    </div>
    <div class="framed">
      <h2>Information <span id="dateTime">?</span></h2>
      <table>
        <tr>
          <td>P. Haus</td><td><span id="valPowerHouse">?</span></td>
        </tr>
        <tr>
          <td>Blue State</td><td><span id="valBluettiPercent">?</span></td>
        </tr>
        <tr>
          <td>Blue Solar</td><td><span id="valSolarBluePower">?</span></td>
        </tr>
        <tr>
          <td>Blue P</td><td><span id="valBluePower">? </span></td>
        </tr>
        <tr>
          <td>             </td><td><span id="valBlueInvPower">?</span></td>
        </tr>
        <tr>
          <td>Deye Solar</td><td><span id="valSolarDeyePower">?</span></td>
        </tr>
      </table>
    </div>
    <div class="framed">
    <h2> manuelle Schalter </h2>
    <form>
      <button id="bToggleDC"   type="button">toggle Bluetti DC</button> <!-- immer noch (2023-09) ein Button sendet ab außer er ist type button -->
      <!------- Servo testen -->
      <button id="bServoLeft"  type="button">Servo Left</button> <!-- immer noch (2023-09) ein Button sendet ab außer er ist type button -->
      <button id="bServoRight" type="button">Servo Right</button> <!-- immer noch (2023-09) ein Button sendet ab außer er ist type button -->
      <button id="bServoStop"  type="button">Servo Stop</button> <!-- immer noch (2023-09) ein Button sendet ab außer er ist type button -->
    </form>
    <form>
          <button id="bAdjustBluetti"  type="button">BluettiOut anpassen</button>
          <button id="bBluettiOnly"  type="button">Bluetti voll laden</button>
          <button id="bDeyeOnly"  type="button">nur Haus versorgen (Deye)</button>
          <button id="bDeyeBluetti"  type="button">Haus versorgen, Bluetti laden</button>
    </form>
    <!-- relais dürfen nicht mehr unabhängig geschaltet werden 
    <form id="fRelais">
      <button id="bR1on"   type="button">R1 on</button>
      <button id="bR1off"  type="button">R1 off</button>
      <button id="bR2on"   type="button">R2 on</button>
      <button id="bR2off"  type="button">R2 off</button>
      <button id="bR3on"   type="button">R3 on</button>
      <button id="bR3off"  type="button">R3 off</button>
      <button id="bR4on"   type="button">R4 on</button>
      <button id="bR4off"  type="button">R4 off</button> 
    </form> -->
    </div>
    <div id='divNachrichten'>
     Nachrichten <button id='bClear' type='button'>(clear)</button>: 
     <p id="iMessage">
     </p>
    </div>
    <h2>Erl&auml;uterungen</h2>
    <p>Zeitg den Zustand der Solar-Geschichten an und ermöglicht teilweise das Schalten. </p>
    <div class = "hinweis">Ein Update der Firmware / des Sketches kann &uuml;ber OTA erfolgen.
      <ol>
        <li>Bin-Datei erzeugen, in der IDE Sketch -> Kompilierte ... exportieren oder Strg Alt s </li>
        <li>Upload der Firmware über %UPDATE_LINK% (findet sich im Sketchordner) </li>
      </ol>
      Das sollte auch fuer das Dateisystem statt Firmware gehen, ist aber unnoetig, da ich mir damit die 
      gespeicherten RFIDs loesche. Da sonst nichts dort liegt - egal. (Gibt wohl auch Probleme mit der Arduino-Ide das Dateisystem als bin 
      zu erzeugen, wenn kein USB.
    </div>
  </body></html>
)rawliteral";
