// HTTPS template code.
// COMP-10184 - Mohawk College
//
//
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "wifi.h"
#include <CertStoreBearSSL.h>
#include <LittleFS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define NTP_SERVER "1.ca.pool.ntp.org"
#define CSUNIX_ENDPOINT "https://csunix.mohawkcollege.ca/~visser/COMP-10184/serverStats.php"

void updateServer();
void updateTime();

// HTTPS Client
WiFiClientSecure wifiClient;

BearSSL::CertStore certStore;

WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP, NTP_SERVER);

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nCOMP-10184 - IoT Security Lab");

  LittleFS.begin();

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


  // for lab use ONLY!
  // wifiClient.setInsecure();
  // wifiClient.setFingerprint("B5:19:6A:46:B8:5D:24:7F:17:14:07:9E:21:00:84:FA:66:2F:2E:5F");
}

void loop() {
  updateTime();
  updateServer();
  // don't use blocking delay()'s like this for Project #3!
  delay(10000);
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
  } else if (respCode > 0) {
    Serial.println(" Made a secure connection. Server or URL problems?");
  } else {
    Serial.println(" Can't make a secure connection to server :-(");
  }
  httpClient.end();
}

void updateTime(){
  if ( ntpClient.update() ) {
    // need to know the current time to validate a certificate.
    wifiClient.setX509Time(ntpClient.getEpochTime());
    Serial.println("\nTime set: " + ntpClient.getFormattedTime());
  }

}
