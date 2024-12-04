// COMP-10184 – Mohawk College
// IOT Webserver
//
// Description
//
// @author Eli Wood
// @id 000872273
//
// I created this work and I have not shared it with anyone else.
//

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "wifi.h"
#include "thingspeak.h"
#include <CertStoreBearSSL.h>
#include <LittleFS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <Adafruit_AHTX0.h>
#include <ArduinoJson.h>


#define NTP_SERVER "1.ca.pool.ntp.org"
#define CSUNIX_ENDPOINT "https://csunix.mohawkcollege.ca/~visser/COMP-10184/serverStats.php"

#define UPDATE_FREQUENCY_MS 5000

#define UPTIME_FIELD "field1"
#define MEMORY_FIELD "field2"
#define LOADING_FIELD "field3"
#define TEMPERATURE_FIELD "field4"

#define THINGSPEAK "https://api.thingspeak.com/update"

void updateServer();
void updateTime();
void handleNotFound();
void handleFileRequest(String);
bool sendFromLittleFS(String);
void updateSensor();
void handleData();
void updateData(String);
int uptimeToMinutes(JsonDocument);
void updateThingspeak(int, int, float, float);

unsigned long lastUpdate = 0;

// HTTPS Client
WiFiClientSecure wifiClient;

BearSSL::CertStore certStore;

WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP, NTP_SERVER);

ESP8266WebServer webServer(80);
Adafruit_AHTX0 aht;

JsonDocument data;

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nCOMP-10184 - Project 3");

  //setup sensors
  if (!aht.begin()) {
    Serial.println("Could not find AHT? Check wiring");
    while (1) delay(10);
  }
  Serial.println("AHT10 or AHT20 found");

  //setup file system
  if (!LittleFS.begin()) {
    Serial.println("Could not find LittleFS? Check wiring");
    while (1) delay(10);
  }
  Serial.println("LittleFs found");

  // add CA List Load here!
  unsigned long startTime = millis();
  int numCerts = certStore.initCertStore(LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
  Serial.printf("Read %d CA certs into store in %lu ms\n", numCerts, millis() - startTime);
  if (numCerts == 0) {
    Serial.println("No certs found! Did you upload certs.ar to LittleFS?");
    // no point in continuing..
    ESP.deepSleep(0);
  }

  wifiClient.setCertStore(&certStore);


  // ensure we are using TLS version 1.2 (versions 1.0 and 1.1 have known
  // vulnerabilities)
  wifiClient.setSSLVersion(BR_TLS12, BR_TLS12);

  // make wifi connection
  Serial.println("Connecting to: " + String(ssid));
  WiFi.begin(ssid, pass);

  while ( WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connection established");
  Serial.print("IP Address:\t");
  Serial.println(WiFi.localIP());


  //create handlers
  webServer.on("/", [](){handleFileRequest("index.html");});
  webServer.on("/index.html", [](){handleFileRequest("index.html");});
  webServer.on("/style.css", [](){handleFileRequest("style.css");});
  webServer.on("/script.js", [](){handleFileRequest("script.js");});
  webServer.on("/data", handleData);

  webServer.onNotFound(handleNotFound);

  webServer.begin();
  Serial.printf("Web server started, open %s in a web browser\n", WiFi.localIP().toString().c_str());
}

void loop() {
  //run on first loop and every UPDATE_FREQUENCY_MS milliseconds
  if(lastUpdate == 0 || lastUpdate + UPDATE_FREQUENCY_MS < millis()){
    updateTime();
    updateServer();
    updateSensor();
    lastUpdate = millis();

    // serializeJsonPretty(data, Serial);
    JsonDocument uptime;

    DeserializationError uptimeError = deserializeJson(uptime, data["uptime"].as<String>());

    // Test if parsing succeeds
    if (uptimeError) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(uptimeError.f_str());
      return;
    }

    int minutes = uptimeToMinutes(uptime);

    JsonDocument memory;

    DeserializationError memoryError = deserializeJson(memory, data["memory"].as<String>());

    // Test if parsing succeeds
    if (memoryError) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(memoryError.f_str());
      return;
    }

    int available = atoi( memory["available"].as<String>().substring(0, memory["available"].as<String>().length()-2).c_str() );

    JsonDocument loading;

    DeserializationError loadingError = deserializeJson(loading, data["cpu_loading"].as<String>());

    // Test if parsing succeeds
    if (loadingError) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(loadingError.f_str());
      return;
    }

    updateThingspeak(minutes, available, loading["last_minute"], data["temperature"]);

    // serializeJsonPretty(data, Serial);
  }
  
  webServer.handleClient();
}

void updateThingspeak(int uptime, int memory, float loading, float temp){
  // Make an HTTPS GET request...
  HTTPClient httpClient;
  httpClient.begin(wifiClient, THINGSPEAK + 
    String("?api_key=") + WRITE_API_KEY + String("&") + 
    UPTIME_FIELD + String("=") + uptime + String("&") + 
    MEMORY_FIELD + String("=") + memory + String("&") + 
    LOADING_FIELD + String("=") + loading + String("&") +
    TEMPERATURE_FIELD + String("=") + temp
  );

  // tell user where we are going
  Serial.println("\nContacting Thingspeak");

  // make an HTTP GET request
  int respCode = httpClient.GET();

  // print HTTP response
  Serial.printf("HTTP Response Code: %d\n", respCode);

  if ( respCode == HTTP_CODE_OK ) {
    String serverResponse = httpClient.getString();
    Serial.println(" Server payload: " + serverResponse);
    // updateData(serverResponse);
  } else if (respCode > 0) {
    Serial.println(" Made a secure connection. Server or URL problems?");
  } else {
    Serial.println(" Can't make a secure connection to server :-(");
  }
  httpClient.end();
}

int uptimeToMinutes(JsonDocument uptime){
  int days = uptime["days"];
  int hours = uptime["hours"];
  int minutes = uptime["minutes"];

  hours += days * 24;
  minutes += hours * 60;

  return minutes;
}

void updateSensor(){
  sensors_event_t humiditySource, tempSource;
  aht.getEvent(&humiditySource, &tempSource);
  data["temperature"] = tempSource.temperature;
  data["humidity"] = humiditySource.relative_humidity;
}

void updateServer() {
  // Make an HTTPS GET request...
  HTTPClient httpClient;
  httpClient.begin(wifiClient, CSUNIX_ENDPOINT);

  // tell user where we are going
  Serial.println("\nContacting server at: " + String(CSUNIX_ENDPOINT));

  // make an HTTP GET request
  int respCode = httpClient.GET();

  // print HTTP response
  Serial.printf("HTTP Response Code: %d\n", respCode);

  if ( respCode == HTTP_CODE_OK ) {
    String serverResponse = httpClient.getString();
    Serial.println(" Server payload: " + serverResponse);
    updateData(serverResponse);
  } else if (respCode > 0) {
    Serial.println(" Made a secure connection. Server or URL problems?");
  } else {
    Serial.println(" Can't make a secure connection to server :-(");
  }
  httpClient.end();
}

void updateData(String newData){
  //parse input json and populate data struct
  DeserializationError error = deserializeJson(data, newData);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
  }
}

void updateTime(){
  if ( ntpClient.update() ) {
    // need to know the current time to validate a certificate.
    wifiClient.setX509Time(ntpClient.getEpochTime());
    Serial.println("\nTime set: " + ntpClient.getFormattedTime());
  }

}

/**
 * handles when a file or resource is not found
 */
void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += webServer.uri();
  message += "\nMethod: ";
  message += (webServer.method() == HTTP_GET) ? "GET":"POST";
  message += "\nArguments: ";
  message += webServer.args();
  message += "\n";
  for (uint8_t i=0; i<webServer.args(); i++){
    message += " NAME:"+webServer.argName(i) + "\n VALUE:" + webServer.arg(i) + "\n";
  }
  webServer.send(404, "text/plain", message);
  Serial.println(message);
}

/**
 * handles a request for a file
 * @param path the path to the requested file
 */
void handleFileRequest(String path){
  if(!sendFromLittleFS(path)){
    handleNotFound();
  }
}

/**
 * this function examines the URL from the client and based on the extension
 * determines the type of response to send.
 * @param path the path to the file being sent
 * @return true if the file was sent successfully
 */
bool sendFromLittleFS(String path) {
  bool bStatus;
  String contentType;
  
  // set bStatus to false assuming this will not work.
  bStatus = false;

  // assume this will be the content type returned, unless path extension 
  // indicates something else
  contentType = "text/plain";

  // DEBUG:  print request URI to user:
  Serial.print("Requested URI: ");
  Serial.println(path.c_str());

  // if no path extension is given, assume index.html is requested.
  if(path.endsWith("/")) path += "index.html";
 
  // look at the URI extension to determine what kind of data to 
  // send to client.
  if (path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if (path.endsWith(".html")) contentType = "text/html";
  else if (path.endsWith(".htm"))  contentType = "text/html";
  else if (path.endsWith(".css"))  contentType = "text/css";
  else if (path.endsWith(".js"))   contentType = "application/javascript";
  else if (path.endsWith(".json")) contentType = "application/json";
  else if (path.endsWith(".png"))  contentType = "image/png";
  else if (path.endsWith(".gif"))  contentType = "image/gif";
  else if (path.endsWith(".jpg"))  contentType = "image/jpeg";
  else if (path.endsWith(".ico"))  contentType = "image/x-icon";
  else if (path.endsWith(".xml"))  contentType = "text/xml";
  else if (path.endsWith(".pdf"))  contentType = "application/pdf";
  else if (path.endsWith(".zip"))  contentType = "application/zip";

  // try to open file in LittleFS filesystem
  File dataFile = LittleFS.open(path.c_str(), "r");
  // if dataFile != 0, then it was opened successfully.
  if ( dataFile ) {
    if (webServer.hasArg("download")) contentType = "application/octet-stream";
    // stream the file to the client.  check that it was completely sent.
    if (webServer.streamFile(dataFile, contentType) != dataFile.size()) {
      Serial.println("Error streaming file: " + String(path.c_str()));
    }
    // close the file
    dataFile.close();
    // indicate success
    bStatus = true;
  }
 
  return bStatus;
}

void handleData(){
  String outputJSON;
  serializeJson(data, outputJSON);

  Serial.println("to send: " + outputJSON);
  //send the response
  webServer.send(200, "application/json", outputJSON);
}