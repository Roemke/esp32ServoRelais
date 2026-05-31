//------------------------------------------------------------------------------------------
//die haupseite, sie wird im flash abgelegt (wegen PROGMEM) R" heißt "roh", rawliteral( als trennzeichen
const char index_html[] PROGMEM = R"rawliteral(<!doctype html> 
<html lang='de'>
  <head>
    <meta charset='utf-8'>
    <meta name='viewport' content='width=device-width'>
    <title>Solar</title>
    <style>
     .framed { 
       border: 1px solid black; 
       padding: 1em;
      }
      li input  {
        width: 15em;
      }
      
      #divNachrichten {
        border: 1px solid blue;
        padding: 1em;
        margin-top:  1em;
        margin-bottom:  1em;
      }
      h1 {
        font-size: 135%%; /* im Template keine Prozentangababen moeglich :-), leistet template ein..., %% - yes */
        padding: 0em 1em; /* seltsam im JavaScript habe ich Prozent drin, nein geht auch nicht*/
      }
      h2 {
        font-size: 110%%;
      }
      .hinweis {
        /* border: 2px solid darkgreen; */
        padding: 2em;
      }
      h2 span{
        font-weight: normal;
      }
      h2 button, #dInfoS button {
          border: 0px solid grey;
          width: 2em;
          max-width: 2em;
          min-width: 2em;
          margin: 0em;
          margin-left: 2em;
      }
      
      #dSticky {
          position: sticky;
          top: 0em;
      }
      #dInfo {
        background: rgba(252, 252, 252, 0.95);
      }
      #dInfo h2 {
        margin-bottom: 0.5em;
      }
      .infoGrid {
          display: grid;
          grid-template-columns: auto auto;
          gap: 1em;
          justify-content: start;
      }
      .infoGrid table {
          border-collapse: collapse;
      }
      .infoGrid td {
          padding: 2px 8px 2px 0;
      }
      @media only screen and (max-width: 500px) {
          .infoGrid {
              grid-template-columns: 1fr;
          }
      }        
      #dInfoS{
        background: rgba(252, 252, 252, 0.95);
        display: grid;
        gap: 		0.5em  2em; /* row column */ 
        grid-template-columns: auto auto auto;   
        justify-content: start;
        justify-items: stretch; /* in der spalte */
        align-items: center; /*vertical*/   
      }
      #dInfo.displayNone, #dInfoS.displayNone {
          display: none; 
      }


      #dInfo h2 {
      	 grid-column: 1 / span 2;
      }

      @media only screen { 
          #dManuell {
              display: grid;
              gap: 		0.5em  1em; /* row column */ 
              grid-template-columns: auto auto auto auto auto auto auto auto ;   /* 8 spalten */
              justify-content: start;
              justify-items: stretch; /* in der spalte */   
              align-items: center; /* in der zeile */
            }
            #dManuell  .haelfte {grid-column: auto / span 4;}
      }

      @media only screen and (max-width: 800px) { 
          #dManuell {
              display: grid;
              gap: 		0.5em  2em; /* row column */ 
              grid-template-columns: auto auto auto auto auto auto ;   /* 6 Spalten */
              justify-content: start;
              justify-items: stretch; /* in der spalte */   
              align-items: center; /* in der zeile */
            }
            #dManuell p {
              grid-column: 1 / -1 ; /* kleines Display, paragraphen über ganze Zeile */
              margin-top: 0em;
            }
          #dManuell button {margin-bottom: 1em;}
          #dManuell .haelfte {grid-column: auto / span 3;}

      }
      #dManuell h2, #dManuell h3 {
      	grid-column: 1 / -1; /* bis zum Ende */
      	padding-top: 1em;
      	margin-bottom: 0.5em;
      }
      #dManuell .span3 {
        grid-column: auto / span 3;
      } 
      #dManuell small {
        grid-column: 1 / -1;
      }

      #dManuell * {
      	margin-top: 0em;
      	margin-bottom: 0em; 
        grid-column: auto / span 2;
      }
               
      .framed { 
        border: 1px solid black; 
        padding: 1em;
      }
      
      #controlContainer { /* hmm ein grid ist etwas übertrieben */
          display: grid;
          background-color: #fafaff;
          /*  grid-template-rows: 20%% 20%% 20%% 20%% 20%% ; sowieso automatisch */
           grid-template-columns: 50%%  50%%;
           grid-auto-flow: column;
           padding-bottom: 0em; 
           /*justify-items: center; */             
      }
      #controlContainer h1 {
        font-size: 130%%; 
      }
      #controlContainer button {
        margin-top: 0.5em;
      }
      #status {
        padding: 1em; 
        grid-row: 1 / 7 ; /* kurz */ 
        grid-column-start:4; 
      }
      #sSymbol {
        font-size:200%%;
      }
      button {
      	margin: 4px;
      	background-color: #eee;
      	padding: 4px;
      	min-width: 5em;
      }
      .oneTouch {
      	background-color : #dfd;
      }
      .confirmedTouch {
      	background-color: #0a0;
      }
      .green {
      	color: rgb(7, 81, 7);
      }
      .red {
      	color: rgb(99, 8, 8);
      }  
    </style>
    <script>
      let gateway = `ws://${window.location.hostname}/ws`;
      gateway = "ws://192.168.0.240/ws"; //zum testen 
      var websocket;
      let bluettiOn = false;
      function htmlToElement(html) 
      {
        var template = document.createElement('template');
        html = html.trim(); // Never return a text node of whitespace as the result
        template.innerHTML = html;
        return template.content.firstChild;
      }

      function evaluateConfirm(topic)
      {
        let el = 0;
        let elReset = false;
        let offList = [];
        switch (topic)
        {
          case "bluettiDCOn":
            el = document.getElementById("bBlueDCOn");
            offList=["bBlueDCOff"];
          break;
          case "bluettiDCOff":
            el = document.getElementById("bBlueDCOff");
            offList=["bBlueDCOn"];
          break;
          case "servoLeft":
            el = document.getElementById("bServoLeft");
            offList = ["bServoRight","bServoStop"];
          break;
          case "servoRight":
            el = document.getElementById("bServoRight");
            offList = ["bServoLeft","bServoStop"];
          break;
          case "servoStop":
            el = document.getElementById("bServoStop"); 
            offList = ["bServoRight","bServoLeft"];              			             			
          break;
          case "rebootESP":
            el = document.getElementById("bReboot");
            elReset = true;
          break;
          case "setStandard":
            el = document.getElementById("bStandard");
            elReset = true;
          break;
          case "adjustBluettiStart":
            el = document.getElementById("bAdjustBluetti");
            offList = ["bAdjustBluettiStop"];              			
          break;
          case "adjustBluettiStop":
            el = document.getElementById("bAdjustBluettiStop");
            elReset = true;
            offList = ["bAdjustBluetti"];              			
          break;
          case "bluettiOnly":
            el = document.getElementById("bBluettiOnly");
            offList = ["bDeyeOnly","bBluettiDeye"];              			
          break;
          case "deyeOnly":
            el = document.getElementById("bDeyeOnly");
            offList = ["bBluettiOnly","bBluettiDeye"];              			
          break;
          case "bluettiDeye":
            el = document.getElementById("bBluettiDeye");
            offList = ["bDeyeOnly","bBluettiOnly"];              												
          break;									
          case "autoChargeOn":
            el = document.getElementById("bAutoChargeOn");
            offList = ["bAutoChargeOff"];              												
          break;									
          case "autoChargeOff":
            el = document.getElementById("bAutoChargeOff");
            offList = ["bAutoChargeOn"];              												
          break;				
          case "autoBlueInverterOn":
              el = document.getElementById("bAutoBlueInverterOn");
              offList = ["bAutoBlueInverterOff"];
          break;
          case "autoBlueInverterOff":
              el = document.getElementById("bAutoBlueInverterOff");
              offList = ["bAutoBlueInverterOn"];
          break;					
          case "autoAdjustBlueOff":
            el = document.getElementById("bAutoAdjustBlueOff");
            offList = ["bAutoAdjustBlueOn"];              												
          break;									
          case "autoAdjustBlueOn":
            el = document.getElementById("bAutoAdjustBlueOn");
            offList = ["bAutoAdjustBlueOff"];              												
          break;									
          //special
          case "adjustBluettiDone": 
            offList = ["bAdjustBluetti"];
          break;	
        }
        if (el)
        {
            el.classList.remove("oneTouch");
            el.classList.add("confirmedTouch");
            if (elReset)
              setTimeout( () => { el.classList.remove("confirmedTouch"); } ,3000);
        }
        //maybe switch buttons off
        offList.forEach( el => document.getElementById(el).classList.remove("oneTouch","confirmedTouch") );
        //offList.forEach( el => document.getElementById(el).classList.remove("oneTouch","confirmedTouch"); ); so nicht

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
            //console.log(`Received a notification from ${e.origin}`);
            console.log(e);
            //console.log(e.data);
            let data =JSON.parse(e.data);
            //console.log(data);
            let now = new Date();
            switch (data.action)
            {
              case "message":
                let tStamp = now.toLocaleDateString('en-CA') + " " + now.toLocaleTimeString('de-DE') + ": ";
                document.getElementById("iMessage").innerHTML += tStamp + data.text + "<br>";
                break;
              case "messageNLB":
                document.getElementById("iMessage").innerHTML += data.text 
                break;  
              case "power": 
                let vDateTime = document.getElementById("dateTime"); 
                let vph = document.getElementById("valPowerHouse"); 
                let vphS = document.getElementById("valPowerHouseS");
                let vps  = document.getElementById("valSolar"); 
                let vSDP = document.getElementById("valSolarDeyePower"); 
                let vBIP = document.getElementById("valBlueInvPower");

                let vBSolarP = document.getElementById("valSolarBluePower"); 
                let vBDCP = document.getElementById("valBluePowerDC");
                let vBACP = document.getElementById("valBluePowerAC");
                let vBP = document.getElementById("valBluettiPercent");
                let vSEGrid    = document.getElementById("valSEGrid");
                let vSEPowerAC = document.getElementById("valSEPowerAC");
                let vSEPowerBat= document.getElementById("valSEPowerBat");
                
                let vSESolar     = document.getElementById("valSESolar");
                let vTotalSolar  = document.getElementById("valTotalSolar");
                                
                let vSEInverterLoss  = document.getElementById("valSEInverterLoss");
                let vSEInverterLossPct = document.getElementById("valSEInverterLossPct");
                
                let vSESoe     = document.getElementById("valSESoe");
                let seGrid = data.values.seGrid;

                vSEGrid.classList.remove("green","red");
                let seGridString = seGrid >=0 ? seGrid.toFixed(0) + " W Einspeisung" 
                                              : (-seGrid).toFixed(0) + " W Bezug";
                if (seGrid >= 0)
                  vSEGrid.classList.add("green");
                else
                  vSEGrid.classList.add("red");
                vSEGrid.innerHTML    =  seGridString;
                
                //batterie analog
                let bat = data.values.sePowerBat;
                let seBatString = bat > 0 ? bat.toFixed(0) + " W lädt" 
                                              : (bat < 0) ? (-bat).toFixed(0) + " W entlädt"
                                              : "0 W";
                vSEPowerBat.classList.remove("green","red");
                if (bat > 0) 
                    vSEPowerBat.classList.add("green");
                else if (bat < 0)                     
                    vSEPowerBat.classList.add("red");
                vSEPowerBat.innerHTML = seBatString;  


                vSEPowerAC.innerHTML = data.values.sePowerAC.toFixed(0) + " W";
                vSESolar.innerHTML    = data.values.seSolar.toFixed(0)    + " W";
                vTotalSolar.innerHTML = data.values.totalSolar.toFixed(0) + " W";

                vSESoe.innerHTML     = data.values.seSoe.toFixed(1)     + " %%";
                vSEInverterLoss.innerHTML    = data.values.seInverterLoss >= 0 
                    ? data.values.seInverterLoss.toFixed(0) + " W" : "n/a";
                vSEInverterLossPct.innerHTML = data.values.seInverterLossPct >= 0 
                    ? data.values.seInverterLossPct.toFixed(1) + " %%" : "n/a";
                    
                vDateTime.innerHTML =  "(" + now.toLocaleDateString('en-CA') + 
                                       " " + now.toLocaleTimeString('de-DE') + ")";
                

                vph.innerHTML =  data.values.powerHouse.toFixed(0) + " W " ; 
                vphS.innerHTML = data.values.powerHouse.toFixed(0) + " W " ; 
                vSDP.innerHTML = (!data.values.eDeyeInverter) ? data.values.powerDeyeInv.toFixed(0) + " W I." : '?';
                vBIP.innerHTML = (!data.values.eBlueInverter) ? data.values.powerBlueInv.toFixed(0) + " W I." : '?';                                
                                 
                vBSolarP.innerHTML = (!data.values.eBluetti) ? data.values.bluettiIn.toFixed(0) + " W" : '?';
                
                vps.innerHTML = data.values.totalSolar.toFixed(0) + " W";
                vBDCP.innerHTML = (!data.values.eBluetti) ? data.values.bluettiOutDC.toFixed(0) +" W" : '? W'; 
                vBACP.innerHTML = (!data.values.eBluetti) ? data.values.bluettiOutAC.toFixed(0) +" W" : '? W'; 
                vBP.innerHTML = (!data.values.eBluetti) ? data.values.bluettiPercent.toFixed(1) + " %%": "?" ;
                


              break;
              case "confirm":
                evaluateConfirm(data.topic);
              break; 
              case "status": // statt mehrere confirms, eine status-Meldung, kann aber genauso wie die confirm ausgewertet werden
                if (document.activeElement.id != 'intervalAutoAdjust')
                  document.getElementById('intervalAutoAdjust').value = data.intervalAutoAdjust;
                if (document.activeElement.id != 'intervalAutoCharge')
                  document.getElementById('intervalAutoCharge').value = data.intervalAutoCharge;
                if (document.activeElement.id != 'maxPowerBlue')
                  document.getElementById('maxPowerBlue').value = data.maxPowerBlue;
                if (document.activeElement.id != 'minPercentBlue')
                  document.getElementById('minPercentBlue').value = data.minPercentBlue;
                if (document.activeElement.id != 'seGridMinCharge')
                  document.getElementById('seGridMinCharge').value = data.seGridMinCharge;
                if (document.activeElement.id != 'seGridBothCharge')
                  document.getElementById('seGridBothCharge').value = data.seGridBothCharge;  
                data.values.forEach(  evaluateConfirm );//sollte ein Array sein, dieses abarbeiten
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
        document.getElementById('bStandard').addEventListener("click",() =>
        {
           websocket.send(JSON.stringify({'action':'setStandard'}));
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
        document.getElementById('bBlueDCOn').addEventListener("click",(e) => 
        {
            websocket.send(JSON.stringify({'action':'dc_output','value':'on'}));
        });
        document.getElementById('bBlueDCOff').addEventListener("click",(e) => 
        {
            websocket.send(JSON.stringify({'action':'dc_output','value':'off'}));
				}); 
        //automatisches einspeisen bluetti an/aus
        document.getElementById('bAutoBlueInverterOn').addEventListener("click",() => 
        { 
            websocket.send(JSON.stringify({'action':'autoBlueInverter','value':'on'}));
        });
        document.getElementById('bAutoBlueInverterOff').addEventListener("click",() => 
        { 
            websocket.send(JSON.stringify({'action':'autoBlueInverter','value':'off'}));
        });       
				//anpassen der Leistung von Bluetti ins Hausnetz
        document.getElementById('bAdjustBluetti').addEventListener("click",() => 
        { 
            websocket.send(JSON.stringify({'action':'adjustBluetti','value':'start'}));
        });
        document.getElementById('bAdjustBluettiStop').addEventListener("click",() => 
        { 
            websocket.send(JSON.stringify({'action':'adjustBluetti','value':'stop'}));
        });
        //Ladeverhalten, Relais schalten
        document.getElementById('bBluettiOnly').addEventListener("click",() => 
        { 
            websocket.send(JSON.stringify({'action':'changeCharge','value':'%bluettiOnly%'}));
        });
        document.getElementById('bDeyeOnly').addEventListener("click",() => 
        { 
            websocket.send(JSON.stringify({'action':'changeCharge','value':'%deyeOnly%'}));
        });
        document.getElementById('bBluettiDeye').addEventListener("click",() => 
        { 
            websocket.send(JSON.stringify({'action':'changeCharge','value':'%bluettiDeye%'}));
        });
        document.getElementById('bAutoChargeOn').addEventListener("click",() => 
        { 
            websocket.send(JSON.stringify({'action':'autoCharge','value':'on'}));
        });
        document.getElementById('bAutoChargeOff').addEventListener("click",() => 
        { 
            websocket.send(JSON.stringify({'action':'autoCharge','value':'off'}));
        });
        document.getElementById('bAutoAdjustBlueOn').addEventListener("click",() => 
        { 
            websocket.send(JSON.stringify({'action':'autoAdjustBlue','value':'on'}));
        });
        document.getElementById('bAutoAdjustBlueOff').addEventListener("click",() => 
        { 
            websocket.send(JSON.stringify({'action':'autoAdjustBlue','value':'off'}));
        });
        document.getElementById("intervalAutoCharge").addEventListener("change",evt =>
        {
            websocket.send(JSON.stringify({'action':'intervalAutoCharge','value':evt.target.value}));
        });
        document.getElementById("intervalAutoAdjust").addEventListener("change",evt =>
        {
            websocket.send(JSON.stringify({'action':'intervalAutoAdjust','value':evt.target.value}));
        });
        document.getElementById("maxPowerBlue").addEventListener("change",evt =>
        {
            websocket.send(JSON.stringify({'action':'maxPowerBlue','value':evt.target.value}));
        });
        document.getElementById("minPercentBlue").addEventListener("change",evt =>
        {
            websocket.send(JSON.stringify({'action':'minPercentBlue','value':evt.target.value}));
        });
        let fButtons = document.querySelectorAll("button");  //allen buttons hinzufügen
        fButtons.forEach( element => 
        {
           //console.log("add listener to " + element.id);
           element.addEventListener("click", evt =>
           {
             console.log("click on event listener of Button " + evt.target.id);
             evt.target.classList.add("oneTouch");
             setTimeout( () =>  evt.target.classList.remove("oneTouch"),500);
           });
        });  
        document.getElementById("bHideInfo").addEventListener("click", evt =>
        {
          document.getElementById("dInfo").classList.toggle("displayNone");
          document.getElementById("dInfoS").classList.toggle("displayNone");
        });      
        document.getElementById("bHideInfoS").addEventListener("click", evt =>
        {
          document.getElementById("dInfo").classList.toggle("displayNone");
          document.getElementById("dInfoS").classList.toggle("displayNone");
        });
        document.getElementById("seGridMinCharge").addEventListener("change",evt =>
        {
            websocket.send(JSON.stringify({'action':'seGridMinCharge','value':evt.target.value}));
        });
        document.getElementById("seGridBothCharge").addEventListener("change",evt =>
        {
            websocket.send(JSON.stringify({'action':'seGridBothCharge','value':evt.target.value}));
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
        }); */ 
      }); 
  </script>
  </head>
  <body style='font-family:Helvetica, sans-serif'>
    <div id="dSticky"> 
      <div id='controlContainer'>
          <h1>Solar</h1>
          <div>
          <button type="button" id="bReboot">reboot Esp</button>
          <button type="button" id="bStandard">Standard</button>
          </div>
      </div>
      <div id = "dInfoS" class="framed displayNone" >
        <div>P. Haus: <span id="valPowerHouseS">?</span></div>
        <div>P. Solar: <span id="valSolar">?</span></div>
        <button id="bHideInfoS">&oplus;</button>
      </div>
    </div>

    <div class="framed" id="dInfo">
      <h2>Information <span id="dateTime">?</span><span><button id="bHideInfo">&otimes;</button></span></h2>
      <div class="infoGrid">
        <table >
          <tr><th colspan="2" >Bluetti</th></tr>
          <tr><td>P. Haus</td>         <td id="valPowerHouse">?</td></tr>
          <tr><td>Deye Solar</td>      <td id="valSolarDeyePower">?</td></tr>
          <tr><td>Blue State</td>      <td id="valBluettiPercent">?</td></tr>
          <tr><td>Blue Solar</td>      <td id="valSolarBluePower">?</td></tr>
          <tr><td>Blue DC/AC</td>      <td><span id="valBluePowerDC">?</span> / <span id="valBluePowerAC">?</span></td></tr>
          <tr><td>Blue Inverter</td>   <td id="valBlueInvPower">?</td></tr>
        </table>
        <table style="border-collapse:collapse">
          <tr><th colspan="2">SolarEdge</th></tr>
          <tr><td>Netz</td>      <td id="valSEGrid">?</td></tr>
          <tr><td>AC</td>        <td id="valSEPowerAC">?</td></tr>
          <tr><td>Solar ges.</td><td id="valTotalSolar">?</td></tr>
          <tr><td>SE Solar</td>  <td id="valSESolar">?</td></tr>
          <tr><td>Batterie</td>  <td id="valSEPowerBat">?</td></tr>
          <tr><td>WR Verlust</td>    <td id="valSEInverterLoss">?</td></tr> 
          <tr><td>WR Verlust</td>      <td id="valSEInverterLossPct">?</td></tr>
          <tr><td>Ladestand</td> <td id="valSESoe">?</td></tr>
        </table>
      </div>
    </div>
  <div class="framed" id = "dManuell">
    <h2> manuelle Schalter und Information</h2>

    <p>Art der Solar-Einspeisung (einmalig) </p>
    <button id="bBluettiOnly"  type="button" >nur Bluetti</button>
    <button id="bDeyeOnly"     type="button" >nur Haus/Deye</button>
    <button id="bBluettiDeye"  type="button" >beide </button>

  	<p>Automatische Wahl der Solar-Einspeisung</p>
		<button type="button" id="bAutoChargeOn"  >an </button>
		<button type="button" id="bAutoChargeOff"  >aus </button>
    <label>I: <input type="number" size=6 id="intervalAutoCharge" >s</label>

    <p>Automatisches Ein/Ausschalten Bluetti Inverter</p>
    <button class="span3"type="button" id="bAutoBlueInverterOn">an</button>
    <button class="span3" type="button" id="bAutoBlueInverterOff">aus</button>

    <p>Leistung Bluetti / Hausverbrauch (einmalig): </p>
    <button class="span3" id="bAdjustBluetti"  type="button">BluettiOut anpassen</button>
    <button class="span3" id="bAdjustBluettiStop"  type="button">Anpassung abbrechen</button>

  	<p>Automatisches Anpassen der Leistung der Bluetti </p>
		<button type="button" id="bAutoAdjustBlueOn"  >an </button>
		<button type="button" id="bAutoAdjustBlueOff"  >aus </button>
    <label>I: <input type="number" size=6 id="intervalAutoAdjust">s</label>

    <p class="haelfte">Maximale Einspeisung Blue:</p>
    <label class="haelfte"><input type="number" size=6 id="maxPowerBlue"> W</label>
    <p class="haelfte">Minimal Status Blue:</p>
    <label class="haelfte"><input type="number" size=6 id="minPercentBlue"> %%</label>

    <p class="haelfte">Überschuss 1 Panel an Bluetti:</p>
    <label class="haelfte"><input type="number" size=6 id="seGridMinCharge"> W</label>
    <p class="haelfte">Überschuss beide Panels an Bluetti:</p>
    <label class="haelfte"><input type="number" size=6 id="seGridBothCharge"> W</label>

		<h3>Details</h3>

		<p>Bluetti DC Einspeisung</p>
		<button class="span3" type="button" id="bBlueDCOn"  >an </button>
		<button class="span3" type="button" id="bBlueDCOff"  > aus </button>
  
    <!-- Servo (testen) -->
		<p> Servo steuern (links / rechts / stop)  </p>
		<button type="button"  id="bServoLeft"> erhöhen</button>
		<button type="button"  id="bServoRight"> verringern</button>
		<button type="button"  id="bServoStop"> Stop</button>
    <small>Einspeisung durch Bluetti ändern oder Änderung stoppen (nur wenn Bluetti DC auch an ist)</small>    
  </div>

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
    <div id='divNachrichten'>
     Nachrichten <button id='bClear' type='button'>(clear)</button>: 
     <p id="iMessage">
     </p>
    </div>
    <div class="framed">
    <h2>Erl&auml;uterungen</h2>
    <p>Zeigt den Zustand der Solar-Geschichten an und ermöglicht teilweise das Schalten. </p>
    <p> Bei einem Click auf einen Button wird dies farblich (hellgrün) signalisiert. Bestätigt der Server die Aktion, so wird die Farbe erneut geändert (nun grün). Geht i.d.R. so schnell, dass man 
    nur die  Änderung der Farbe nicht sieht. Bei den Schalt-Prozessen, die die Bluetti betreffen springt die Anzeige ggf. auch nochmal zurück, da es eine Zeit dauert, 
    bis die Bluetti reagiert (Bluetooth?)</p>
    <p>Die Anpassung der Einspeisung der Solar-Energie dürfte selbsterklärend sein, natürlich geht nur eine der drei Optionen, die Leistung der Bluetti wird hier nicht 
    angepasst, es werden nur die Stromkreise per Relais geändert.<br> 
    Beim Umschalten auf &quot;nur Haus versorgen (Deye)&quot; dauert es einige Zeit, bis der Deye-Inverter merkt, dass er von den Solarzellen versorgt wird, 
    falls vorher nur die Bluetti geladen wurde.  
    </p>
    <p>BluettiOut anpassen ist umgesetzt. Hier wird nicht zwischen Bluetti laden, beide Laden und nur Deye versorgen 
    umgeschaltet, das ist (zunächst?) separat gehalten. Der  Servo-Motor wird angesteuert.</p>
    <p>Die Auto-Einstellungen haben ein Intervall, in dem Sie durchgeführt werden, Voreinstellung alle 120 Sekunden. </p>
    <p>Die maximale Leistung mit der die Bluetti einspeist kann eingestellt werden, da die Verluste bei höherer Leistung mehr als proportional anwachsen </p>
    <div class = "hinweis">Ein Update der Firmware / des Sketches kann &uuml;ber OTA erfolgen.
      <ol>
        <li>Bin-Datei erzeugen, kompilieren in code, landet im Ordner .pio/build/esp32dev/firmware.bin </li>
        <li>Upload der Firmware über %UPDATE_LINK% (findet sich im Sketchordner / Unterordner) <br>
             -- danach noch ein Reset nötig !?</li>
      </ol>
    </div>
    </div>
  </body></html>
)rawliteral";
