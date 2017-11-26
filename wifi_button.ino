/** 
 *  Implementation of self releasing button for WiFi controlled Relay module
 *  Intended to use with ESP8266-01 on a LC Technology Realy module http://www.lctech-inc.com
 *  
 *  Update firmware to SDK 2.0.x before upload the sketch http://www.espressif.com/en/products/hardware/esp8266ex/resources
 */

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <memory>

boolean _debug = true;
//#define DEBUG_ESP_HTTP_SERVER

template <typename Generic> void DEBUG(Generic text){
  if (_debug) {
    Serial.print("*WB: ");
    Serial.println(text);
  }
}

class LCTechRelay {
  public:
    LCTechRelay() {forceState(RELAY_OPEN);}

    LCTechRelay(boolean state) {forceState(state);}
    
    boolean getState() {
      return _is_open;
    }

    boolean setState(boolean newState) {
      if (_is_open != newState) return forceState(newState); else return false;
    }

    boolean forceState(boolean newState) {
      newState ? sendOpen() : sendClose();
      return true;
    }
    
    boolean open() {return setState(RELAY_OPEN);}
    boolean close() {return setState(RELAY_CLOSED);}
    
    const boolean RELAY_OPEN = true;
    const boolean RELAY_CLOSED = false;
  
  private:
    boolean _is_open = RELAY_OPEN;
    
    void sendClose() {
      _is_open = RELAY_CLOSED;
      // send bytes A00101A2
      Serial.write(0xA0);
      Serial.write(0x01);
      Serial.write(0x01);
      Serial.write(0xA2);
      Serial.flush();
      Serial.println();
      delay(1);
      DEBUG("Relay: close");
    }
    
    void sendOpen(){
      _is_open = RELAY_OPEN;
      //send bytes A00100A1
      Serial.write(0xA0);
      Serial.write(0x01);
      Serial.write(0x00);
      Serial.write(0xA1);
      Serial.flush();
      Serial.println();
      delay(1);
      DEBUG("Relay: open");
    }
};

LCTechRelay Relay;

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  DEBUG(F("Entered config mode"));
  DEBUG(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  DEBUG(myWiFiManager->getConfigPortalSSID());
  //open relay while setting up or reconfiguring WiFi settings
  Relay.open();
}

//server to be used to control button
std::unique_ptr<ESP8266WebServer> server;

const char BUTTON_PAGE[] PROGMEM = "<!DOCTYPE html><html lang=\"en\"><head><title>Door</title><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css\"><script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js\"></script></head><body style=\"margin:0;padding:6em 0 0;text-align:center;height:100%;\"><button type=\"button\" class=\"btn-primary btn-lg center-block\">Open</button><script>function g(link){$.ajax({type:'GET',url:link})};function a(e){$(e.target).text(e.data.t);g(e.data.l)};$(function(){$('button').on('mousedown',{t:'Opened',l:'/on'},a).on('mouseup',{t:'Open',l:'/off'},a)});</script></body></html>";

void handleRoot() {
  DEBUG("Sending BUTTON_PAGE");
  String page = FPSTR(BUTTON_PAGE);
  server->sendHeader("Content-Length", String(page.length()));
  server->send(200, "text/html", page);
};

void handleOn(){
  Relay.close();
  handleRoot();
};

void handleOff(){
  Relay.open();
  handleRoot();
};

void setup() {
  // put your setup code here, to run once:
  // Relay module uses STC15F10W chip to control the relay, it works on 9600 baud serial speed
  Serial.begin(9600);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  // get SSID / password written to flash on WiFi.begin only if currently used values do not match what is already stored in flash
  WiFi.persistent (false);

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "ESP<chip_ID>"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect()) {
    DEBUG(F("failed to connect and hit timeout"));
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  //if you get here you have connected to the WiFi
  DEBUG(F("connected!"));
  
  // setting up button page on web server
  server.reset(new ESP8266WebServer(80));
  server->on("/", handleRoot);
  server->onNotFound(handleRoot);
  server->on("/on", handleOn);
  server->on("/off", handleOff);
  server->begin();
}

void loop() {
  server->handleClient();
}
