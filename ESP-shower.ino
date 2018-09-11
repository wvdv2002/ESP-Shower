#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <SonoffDual.h>
#include <ESP8266WiFi.h>
//needed for WifiManager library
#include <EEPROM.h>
#include <DNSServer.h>
//#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <ESPUI.h>
#include "states.h"

#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>         //https://github.com/tzapu/WiFiManager

AsyncWebServer server(80);
DNSServer dns;

const char* ssid = "ruinelan";
const char* password = "Lanzarote";


const int FLOWCOUNTERBACKUP_TIME = (3600*1000);
const float FLOWCOUNTSTOL = 300.0;
const float FLOWMSTOL = 200.0;

const char RESERVED_PINS[3] = {0,2};
const char FLOW_PIN = 14; //Flowmeter
const char PRESSURESW_PIN = 4; //Pressure switch


unsigned int slowLoopTimeout = millis();
unsigned int espuiLoopTimeout = millis();
unsigned int lastBackupFlowCounter = millis();
unsigned int stTimeout = millis();
unsigned int pumpTimeout = millis();

volatile unsigned int isrFlowCounter = 0;
unsigned int flowCounter = 0;
unsigned int flowL = 0;
unsigned int lastFlowCounterUpdate = 0;
unsigned int previousFlowCounterUpdate = 0;
int enoughPressure = 0;
int relayUV = 0;
int relayPump = 0;

showerState_t showerState = SHOWER_IDLE;

void setup() {
  EEPROM.begin(64);
  pinSetup();
  SonoffDual.setup();
  SonoffDual.setLed(1);
  Serial.println("Booted");
//  Serial.begin(115200);
  AsyncWiFiManager wifiManager(&server,&dns);

  wifiManager.autoConnect("ESPShower");
  wifiManager.setTimeout(180);
  //WiFi.mode(WIFI_STA);
  //WiFi.setHostname(ssid);  
  //WiFi.begin(ssid, password);
  EEPROM.get(0,flowCounter);
  if(flowCounter>2147483647){
    flowCounter = 0;
  }
  isrFlowCounter = flowCounter;
    while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  otaSetup();
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  attachInterrupt(FLOW_PIN,isrFlowCount,FALLING);
  espuiSetup();
  setRelays(0,0);
  delay(100);
  setRelays(0,0);
}

void isrFlowCount(void){
  isrFlowCounter++;
  previousFlowCounterUpdate = lastFlowCounterUpdate;
  lastFlowCounterUpdate = millis();
}

void loop() {
  ArduinoOTA.handle();
  if(millis()-slowLoopTimeout>25){
    slowLoopTimeout = millis();
    espuiTask();
    flowTask();
    espShowerTask();
    if((relayPump || relayUV) && millis()-pumpTimeout>12*60*1000){
      setRelays(0,0);
    }
  }
}

void flowTask(void){
  flowCounter = isrFlowCounter;
  unsigned int temp;
  if((millis()-lastBackupFlowCounter)>FLOWCOUNTERBACKUP_TIME){
    lastBackupFlowCounter = millis();
    EEPROM.get(0,temp);
    if(temp!=flowCounter){
      EEPROM.put(0,flowCounter);
      EEPROM.commit();
    }
  }
}

float getTotalFlow(void){
  return (float)flowCounter/FLOWCOUNTSTOL;
}

float getFlowL(void){
  unsigned lastFlowUpdate =  lastFlowCounterUpdate;
  int temp = lastFlowUpdate -previousFlowCounterUpdate;
  if((millis()-lastFlowUpdate)>temp){temp = millis()-lastFlowUpdate;}
  float flow = 0.0;
  if(temp>0){
    flow = FLOWMSTOL/temp;
  }  
  return flow; 
}

void espShowerTask(void){
  enoughPressure = digitalRead(PRESSURESW_PIN);
  switch(showerState){
      case SHOWER_STARTUV:
        if(millis()-stTimeout>1000){
          stTimeout = millis();
          showerState = SHOWER_STARTPUMP;
          setPumpRelay(1);
        }
      break;    
      case SHOWER_STARTPUMP:
        if(millis()-stTimeout>2000){
          stTimeout = millis();
          showerState = SHOWER_SHOWERING; 
        }
      break;
      case SHOWER_SHOWERING:
        if(millis()-stTimeout>1000){
          stTimeout = millis();
          if(getFlowL()<1.0){
            showerState = SHOWER_TOPUV;
          }
        }
      break;
      case SHOWER_TOPUV:
        if(millis()-stTimeout>1000){
          stTimeout = millis();
          showerState = SHOWER_TOPPRESSURE;
        }
      break;      
      case SHOWER_TOPPRESSURE:
          if(enoughPressure){
        if(millis()-stTimeout>4000){
          stTimeout = millis();

            showerState = SHOWER_WAITING;
            setUvRelay(0);
            setPumpRelay(0);
          }
        }else{
          stTimeout = millis();
        }
      break;
      case SHOWER_WAITING:
        if(millis()-stTimeout>1000){
          stTimeout = millis();
          if(getFlowL()>1.0){
            setUvRelay(1);
            showerState = SHOWER_STARTUV;
          }
          if(!enoughPressure){
            showerState = SHOWER_TOPUV;
            setRelays(1,1);
          }
        }
      break;
    case SHOWER_IDLE:
      stTimeout = millis();
      showerState = SHOWER_WAITING;
    break;
    default:
      showerState = SHOWER_IDLE;
    break;
    
  }
}

void setUvRelay(bool uv){
  setRelays(uv,relayPump);
}

void setPumpRelay(bool pump){
  setRelays(relayUV,pump);
}

void setRelays(bool uv, bool pump){
  relayUV = uv;
  relayPump = pump;
  if(pump || uv){
    pumpTimeout = millis();
  }
  SonoffDual.setRelays(relayUV, relayPump);
}

void espuiTask(){
  if (millis() - espuiLoopTimeout > 2000) {
    ESPUI.print("Total (L):", String(getTotalFlow()));
    ESPUI.print("Flow (L):", String(getFlowL()));
    ESPUI.print("enoughPr:", String(enoughPressure));
    ESPUI.print("UVlight:", String(relayUV));
    ESPUI.print("Pump:", String(relayPump));
    ESPUI.print("showerSt:", showerState_str[showerState]);
    espuiLoopTimeout = millis();
  }
}

void espuiSetup(){
  ESPUI.label("Flow (L):", COLOR_TURQUOISE, "0.0");
  ESPUI.label("Total (L):", COLOR_TURQUOISE, "0.0");
  ESPUI.label("enoughPr:", COLOR_TURQUOISE, "0");
  ESPUI.label("UVlight:", COLOR_TURQUOISE, "0");
  ESPUI.label("Pump:", COLOR_TURQUOISE, "0");
  ESPUI.label("showerSt:", COLOR_TURQUOISE, "0");
  ESPUI.button("flowReset", &flowReset, COLOR_PETERRIVER);
  ESPUI.button("toggle Pump", &togglePump, COLOR_PETERRIVER);
  ESPUI.button("toggle UV", &toggleUV, COLOR_PETERRIVER);  
  ESPUI.begin("ESP32 Control");
}


void togglePump(Control sender, int type){
  switch(type){
    case B_DOWN:
      setPumpRelay(!relayPump); 
    break;
    default:
    break;
  }
}

void toggleUV(Control sender, int type){
  switch(type){
    case B_DOWN:
      setUvRelay(!relayUV); 
    break;
    default:
    break;
  }
}


void flowReset(Control sender, int type){
  switch(type){
  case B_DOWN:
    flowCounter = 0;
    isrFlowCounter = 0;
    lastBackupFlowCounter = millis()-FLOWCOUNTERBACKUP_TIME;
    break;
  case B_UP:
//    movePlatform(0);
    break;
  }    
}



void pinSetup(){
  pinMode(FLOW_PIN,INPUT_PULLUP);
  pinMode(PRESSURESW_PIN,INPUT_PULLUP);
}

void otaSetup(){
    ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
//    Serial.println("Start updating " + type);
  });
  
  ArduinoOTA.onEnd([]() {
//    Serial.println("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
//    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
//  Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  ArduinoOTA.begin();
//  Serial.println("OTA Ready ");
}

