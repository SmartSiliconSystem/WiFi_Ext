    // Entry point 
var language = window.navigator.userLanguage || window.navigator.language;    // Get initial values
var $ = function(s){return document.getElementById(s)};

    var flags = ["fr.png", "de.png", "en.png", "es.png", "it.png", "th.png", "ar.png"]

    var so1=""; var so2=""; var so3=""; var so4=""; var so5=""; var so6=""; var so7="";
    var bt1=""; var bt2=""; var bt3=""; var bt4=""; var bt4_1=""; var bt6=""; var bt6_1="";        
    var lt1=""; var lt2=""; var lt3=""; var lt4=""; var lt5=""; var lt6=""; var lt7=""; 
    var lt8=""; var lt9=""; var lt10=""; var lt11=""; var lt12=""; var lt13=""; var lt14="";
    var lt15=""; var lt16=""; var lt17=""; var lt18=""; var lt19="";
    var ls1=""; var ls1_1=""; var ls2=""; var ls2_1=""; var ls3=""; var ls3_1=""; var ls4="";

    var CAPABILITIES_BIT_ELECTRODES_VER =  (0x3FF); // 10 bits
    var CAPABILITIES_BIT_EXTSENSOR_VER =   (0xFFC00); // 10 bits
    var CAPABILITIES_BIT_WIFI_VER =        (0x3FF00000); // 10 bits

    var STATUS_BIT_POLARITY =              (1<<20); // Indicates that the polarity cycle is normal or reverse (set by the appliance)
    var STATUS_BIT_HIGH_CYCLE =            (1<<19); // Indicates that the current Copper cycle is high
    var STATUS_BIT_SLEEP_MODE =            (1<<18); // Set by the browser to turn appliance in sleep mode
    var STATUS_BIT_KEY3 =                  (1<<17); // Indicates that the license key #3 is valid
    var STATUS_BIT_KEY2 =                  (1<<16); // Indicates that the license key #2 is valid
    var STATUS_BIT_KEY1 =                  (1<<15); // Indicates that the license key #1 is valid
    var STATUS_BIT_PH_PROBE_UNCALIBRATED = (1<<12); // Indicates that the PH probe requires calibration
    var STATUS_BIT_PH_PROBE_CALIBRATED =   (1<<11); // Indicates that the PH probe is calibrated
    var STATUS_BIT_PH_PROBE_PRESENT =      (1<<4); // Indicates that the PH probe has been detected
    var STATUS_BIT_WATER_FLOW =            (1<<3); // Indicates that the water is flowing in the pipe
    var STATUS_BIT_COPPER_SHORTED =        (1<<2); // Indicates that the copper electrodes are either shorted or intensity threshold has been overpassed
    var STATUS_BIT_TITANIUM_SHORTED =      (1<<1); // Indicates that the titanium electrodes are either shorted or intensity threshold has been overpassed
    var STATUS_BIT_COPPER_TO_REPLACE =     (1<<0); // Indicates that the copper electrode need to be replaced

    var MyAppliance =  {"Capabilities": 0, 
                        "Status": 0, 
                        "WaterHardness": 0, 
                        "PH": 0, 
                        "Turbidity": 0, 
                        "WaterTemp": 0, 
                        "CopperReleased": 0,
                        "CopperElectrodeMass": 0,
                        "WaterFlow": 0,
                        "WaterVolume": 0,
                        "WaterColor": 0,
                        "ApplianceName": "",
                        "ContainerCuLevel": 0,
                        "key1": 0,
                        "key2": 0,
                        "key3": 0,
                        "OnTime": 0,
                        "ID": 0,      
                        "StationSSID": "",
                        "StationPwd": "",
                        "SoftAPSSID": "",
                        "SoftAPPwd": ""
    };

    function SetLanguage(language)
    {
      l = language.substring(0,2);
      // reflect language in the select box and flag image
      for (x=0;x<flags.length;x++)
      {
        if (flags[x].substring(0,2) == l){
          switchImage(x); 
          $("#Flag").value = x.toString();
          break;
        }
      }
      if (l=="en")
      {
        so1="French";
        so2="German";
        so3="English";
        so4="Spanish";
        so5="Italian";
        so6="Thailand";
        so7="Arab";
        bt1="Water properties";
        bt2="Electrodes";
        bt3="Settings";
        bt4="Enter sleep mode";
        bt4_1="Exit sleep mode";
        bt6="Turn High Cycle On";
        bt6_1="Turn High Cycle Off";        
        lt1="Copper test solution level:";
        lt2="Hardness:";
        lt3="PH:";
        lt4="Turbidity:";
        lt5="Copper level:";
        lt6="Water Temperature:";
        lt7="Water Flow:";
        lt8="Water Color:";
        lt9="Water volume:";
        lt10="Identification name:";
        lt11="key #1:";
        lt12="key #2:";
        lt13="key #3:";
        lt14="Powered for:";
        lt15="Consumption of the Copper electrode:"
        lt16="WiFi access point name:"
        lt17="Wifi access point password:"
        lt18="Standalone Access point name:"
        lt19="Standalone Access point password:"
        ls1="days";
        ls1_1="day";
        ls2="hours";
        ls2_1="hour";
        ls3="minutes";
        ls3_1="minute";
        ls4="seconds";
      }
      else if (l=="fr")
      {
        so1="Français";
        so2="Allemand";
        so3="Anglais";
        so4="Espagnol";
        so5="Italien";
        so6="Thailandais";
        so7="Arabe";
        bt1="Propriétés de l'Eau";
        bt2="Electrodes";
        bt3="Paramètres";
        bt4="Mise en veille";
        bt4_1="Sortir de la mise en veille";
        bt6="Cycle rapide";
        bt6_1="Cycle normal";        
        lt1="Niveau de solution test cuivre:";
        lt2="Dureté:";
        lt3="PH:";
        lt4="Turbidité:";
        lt5="Niveau de cuivre:";
        lt6="Température:";
        lt7="Débit:";
        lt8="Couleur:";
        lt9="Volume d'eau:";
        lt10="Identifiant:";
        lt11="Clé #1:";
        lt12="Clé #2:";
        lt13="Clé #3:";
        lt14="En service depuis:";
        lt15="Usure de l'électrode cuivre:"
        lt16="Nom du point d'accès WiFi:"
        lt17="Mot de passe du point d'accès WiFi:"
        lt18="Nom du point d'accès autonome:"
        lt19="Mot de passe du point d'accès autonome:"
        ls1="jours";
        ls1_1="jour";
        ls2="heures";
        ls2_1="heure";
        ls3="minutes";
        ls3_1="minute";
        ls4="secondes";
      }
      else if (l=="de")
      {
      }
      else if (l=="it")
      {

      }
      else if (l=="sa")
      {

      }
      else if (l=="es")
      {

      }
      else if (l=="th")
      {

      }
      else
      {
      }
      $("SO1").innerHTML = so1; $("SO2").innerHTML = so2; $("SO3").innerHTML = so3; 
      $("SO4").innerHTML = so4; $("SO5").innerHTML = so5; $("SO6").innerHTML = so6;
      $("SO7").innerHTML = so7;
      $("BT1").innerHTML = bt1; $("BT2").innerHTML = bt2; $("BT3").innerHTML = bt3;
      $("BT4").innerHTML = bt4; $("BT6").innerHTML = bt6; 
      $("LT1").innerHTML = lt1; $("LT2").innerHTML = lt2; $("LT3").innerHTML = lt3; 
      $("LT4").innerHTML = lt4; $("LT5").innerHTML = lt5; $("LT6").innerHTML = lt6;
      $("LT7").innerHTML = lt7; $("LT8").innerHTML = lt8; $("LT9").innerHTML = lt9;
      $("LT10").innerHTML = lt10; $("LT11").innerHTML = lt11; $("LT12").innerHTML = lt12;
      $("LT13").innerHTML = lt13; $("LT14").innerHTML = lt14; $("LT15").innerHTML = lt15;
      $("LT16").innerHTML = lt16;$("LT17").innerHTML = lt17;$("LT18").innerHTML = lt18;$("LT19").innerHTML = lt19;
    };

    function openPage(tab, PageName){
      var i, tabcontent, tablinks;
      tabcontent = document.getElementsByClassName("tabcontent");
      for (i = 0; i < tabcontent.length; i++) {
          tabcontent[i].style.display = "none";
      }
      tablinks = document.getElementsByClassName("tablinks");
      for (i = 0; i < tablinks.length; i++) {
          tablinks[i].className = tablinks[i].className.replace(" active", "");
      }
      $(PageName).style.display = "block";
      $(tab).className += " active";
    }

    function ToggleSleepMode(){
      if ($("BT4").innerHTML == bt4)
      {
        $("BT4").innerHTML = bt4_1; 
        MyAppliance["Status"] = MyAppliance["Status"] | STATUS_BIT_SLEEP_MODE;         
      }
      else
      {
        $("BT4").innerHTML = bt4; 
        MyAppliance["Status"] = MyAppliance["Status"] & (~STATUS_BIT_SLEEP_MODE); 
      }
    }; 
       
    function ToggleHighCycle(){
      if ($("BT6").innerHTML == bt6)
      {
        $("BT6").innerHTML = bt6_1;
        MyAppliance["Status"] = MyAppliance["Status"] | STATUS_BIT_HIGH_CYCLE; 
      }
      else
      {
        $("BT6").innerHTML = bt6; 
        MyAppliance["Status"] = MyAppliance["Status"] & (~STATUS_BIT_HIGH_CYCLE); 
      }
    };

    function CheckForValidKey(Key, value){
      Key.value = value;
      if (Key.id == "key1")
      {
        if (MyAppliance["Status"] & STATUS_BIT_KEY1)
        {
          $('#Key1Icon').src = "Valid.png";
          Key.disabled="disabled";
        }
        else
        {
          if (Key.value == "") $('#Key1Icon').src = "Warning.png";
          else $('#Key1Icon').src = "Error.png";          
        }
      } 
      else if (Key.id == "key2")
      {
        if (MyAppliance["Status"] & STATUS_BIT_KEY2)
        {
          $('#Key2Icon').src = "Valid.png";
          Key.disabled="disabled";
        }
        else
        {
          if (Key.value == "") $('#Key2Icon').src = "Warning.png";
          else $('#Key2Icon').src = "Error.png";          
        }
      } 
      else if (Key.id == "key3")
      {
        if (MyAppliance["Status"] & STATUS_BIT_KEY3)
        {
          $('#Key3Icon').src = "Valid.png";
          Key.disabled="disabled";
        }
        else
        {
          if (Key.value == "") $('#Key3Icon').src = "Warning.png";
          else $('#Key3Icon').src = "Error.png";          
        }
      }  
    };

    function setProgress(progressBar, value)
    {
      $('ContainerCuLevel').innerHTML = value + '%';
      progressBar.style.width = $('ContainerCuLevel').innerHTML;
      if (value<25) progressBar.style.backgroundColor = 100; //rgb(240,0,0);
      else if (value<50) progressBar.style.backgroundColor = 200; //rgb(240,100,25);
      else if (value<75) progressBar.style.backgroundColor = 300; //rgb(240,240,50);
      else progressBar.style.backgroundColor = 400; //rgb(43,194,83);
    };

    function onTimeToTime(seconds)
    {

      var sdays=""; var shours=""; var sminutes="";
      var numdays = Math.floor(seconds / 86400);
      var numhours = Math.floor((seconds % 86400) / 3600);
      var numminutes = Math.floor(((seconds % 86400) % 3600) / 60);
      var numseconds = ((seconds % 86400) % 3600) % 60;

      if (numdays>0) sdays = ls1; else sdays = ls1_1;
      if (numhours>0) shours = ls2; else shours = ls2_1;
      if (numminutes>0) sminutes = ls3; else sminutes = ls3_1;

      return (numdays + " " + sdays + ", " + numhours + " " + shours + ", " + numminutes + " " + sminutes + ", " + numseconds + " " + ls4);
    };

    function switchImage(imgNum){
      var x = parseInt(imgNum);
      var src = (flags[x]);
      $("#FlagImage").src = src;
      return true;
    }
