#include <Arduino.h>

/*
This code runs on an ESP8266 wifi module, compiled/programmed through the Arduino IDE.
The specific hardware in this case is a NodeMCU v0.9 module

The ESP hosts a webserver on port 80, with event listeners for access on / and /unlock,
it also listens on UPD port 1337 for the (newline terminated) string "SESAME"
it broadcasts itself as "ESPrelay" on Mdns, so that devices on LAN can access it via http://ESPrelay.local (unless it's an android device, because fuck that.)
Hand coded http response and body are in the handleroot() function.

Door unlock is physically handled with a relay, so all this code needs to do is toggle I/O pins
*/

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

//OTA:
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>

//Function declarations:
void handleRoot();
void rootRedirect();
void handleUnlock();
void handleNotFound();

void UDPrx();
void unlock();

// Initialize the wifi Udp library
WiFiUDP Udp;
//init http server on port 80:
ESP8266WebServer server(80);

//hardware stuff:
const unsigned int lockPin = D3;
const unsigned int unlockDuration=500;

//UDP stuff:
const unsigned int localPort = 1337;    // local port to listen for UDP packets
const int UDP_PACKET_SIZE = 16; 		// NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ UDP_PACKET_SIZE ]; //buffer to hold incoming and outgoing packets


void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println("AP: " + myWiFiManager->getConfigPortalSSID());
  Serial.println("IP: " + WiFi.softAPIP().toString());
}


void setup() {
  Serial.begin(115200);
  
  WiFi.hostname("ESPrelay");
  

  pinMode(lockPin,OUTPUT);

  //Serial.println("1500 millisecond unlock...");
  //digitalWrite(lockPin,HIGH);
  //delay(1500);
  digitalWrite(lockPin,LOW);
  Serial.println("Lock.");


  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  Serial.println("Connecting to wifi..");
  wifiManager.setAPCallback(configModeCallback); //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setConnectTimeout(30); //try to connect to known wifis for a long time before defaulting to AP mode
  
  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "ESPNFC"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("ESPrelay")) {
    Serial.println("failed to connect and hit timeout");
    ESP.restart(); //reset and try again, or maybe put it to deep sleep 
  }

  //OTA:
   // Port defaults to 8266
  // ArduinoOTA.setPort(8266);
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("ESPrelay");
  // No authentication by default
  ArduinoOTA.setPassword((const char *)"1804020311");
  //ArduinoOTA.setPasswordHash((const char *)"77ca9ed101ac99e43b6842c169c20fda");
  ArduinoOTA.onStart([]() { Serial.println("OTA start"); });
  ArduinoOTA.onEnd([]() { Serial.println("OTA End"); ESP.restart(); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });

  ArduinoOTA.onError([](ota_error_t error) {
    String buffer=String("Error[") + String(error) + String("]: ");
    if (error == OTA_AUTH_ERROR) buffer+=String("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) buffer+=String("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) buffer+=String("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) buffer+=String("Receive Failed");
    else if (error == OTA_END_ERROR) buffer+=String("End Failed");
    Serial.println(buffer);
  });

  ArduinoOTA.begin();

  server.on("/", handleRoot);
  server.on("/unlock", handleUnlock);

  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");

  if (MDNS.begin("ESPrelay")) {
    Serial.println("MDNS responder started, see you at http://ESPrelay.local");
  }
  //BUG: only services named "esp" are found... https://github.com/esp8266/Arduino/issues/2151
  MDNS.addService("esp", "udp", localPort); //workaround:   if(MDNS.hostname(i)=="esprelay"){    
  //MDNS.addService("ESPrelayhttp", "tcp", 80);
  

  Udp.begin(localPort);

}


void loop() {

if (Udp.parsePacket()) UDPrx();

server.handleClient();

ArduinoOTA.handle();

yield();
//delay(500);

}

//http stuff:
void handleRoot() {
  Serial.println("client trying to access /");
  //server.send(200, "text/plain", "hello from esp8266!");
  server.sendContent("HTTP/1.1 200 OK\r\n"); //send new p\r\nage
  server.sendContent("Content-Type: text/html\r\n");
  server.sendContent("\r\n");
  server.sendContent("<HTML>\r\n");
  server.sendContent("<HEAD>\r\n");
  server.sendContent("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"); //zoom to width of window
  server.sendContent("<meta name='apple-mobile-web-app-capable' content='yes' />\r\n");
  server.sendContent("<meta name='apple-mobile-web-app-status-bar-style' content='black-translucent' />\r\n");
  server.sendContent("<link rel='stylesheet' type='text/css' href='https://moore.dk/doorcss.css' />\r\n"); //External CSS
  server.sendContent("<TITLE>Open Sesame.</TITLE>\r\n");
  server.sendContent("</HEAD>\r\n");
  server.sendContent("<BODY>\r\n");
  server.sendContent("<H1>doorHack</H1>\r\n");
  server.sendContent("<hr />\r\n");
  server.sendContent("<br />\r\n");
  server.sendContent("<a href=\"/unlock\"\">Unlock for 3 seconds...</a>\r\n");
  server.sendContent("<br />\r\n");
  server.sendContent("</BODY>\r\n");
  server.sendContent("</HTML>\r\n");


}

void rootRedirect(){
  server.sendContent("HTTP/1.1 303 See Other\r\n");
  server.sendContent("Location: /\r\n");
  server.sendContent("\r\n"); //EOT
}

void handleUnlock(){
	Serial.println("unlocking..");
	rootRedirect();
	unlock(); //halts for 3000ms
}

void handleNotFound(){
  Serial.println("client getting 404");
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void UDPrx()
{
// We've received a packet, read the data from it
	Serial.println("udpRX!");
    memset(packetBuffer, 0, UDP_PACKET_SIZE); //reset packet buffer
    int read_bytes=Udp.read(packetBuffer,UDP_PACKET_SIZE);  // read the packet into the buffer

	if (packetBuffer[0]=='S' &&
		packetBuffer[1]=='E' &&
		packetBuffer[2]=='S' &&
		packetBuffer[3]=='A' &&
		packetBuffer[4]=='M' &&
		packetBuffer[5]=='E')
			{
				unlock();
			}
	else Serial.println("UDP Parsing failed.");
}

void unlock(){
				Serial.println("open!");
				digitalWrite(lockPin,HIGH);
				delay(unlockDuration);
				digitalWrite(lockPin,LOW);
				Serial.println("close..");
				Serial.println();
}