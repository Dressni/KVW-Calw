#include <Wire.h>
#include <SSD1306.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <ESP32WebServer.h>
#include <Update.h>
#include <esp_wps.h>
#include "websites.h"

//WiFi-SoftAP credentials
#define ssid "KVW-Calw Roboter"
#define password "Seminarkurs20172018"
#define BROADCAST "192.168.4.255"
#define ESP_HOST "192.168.4.1"

#define LIDAR_PWM_PIN 16

boolean demo = false;
WiFiUDP Udp;
ESP32WebServer server(80);
SSD1306 display(0x3c, 5, 4);

void setup() {

  Serial.begin(115200);

  pinMode(LIDAR_PWM_PIN, INPUT);
  pinMode(0, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.clear();
  display.drawStringMaxWidth(0, 0, 128, "Boot" );
  display.display();

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    display.clear();
    display.drawStringMaxWidth(0, 0, 128, "Suche WiFi..." );
    display.display();
    delay(4000);
    display.clear();
    display.drawStringMaxWidth(0, 0, 128, "Kein WiFi gefunden!\nSuche erneut." );
    display.display();
    delay(1000);
    ESP.restart();
  }

  Udp.begin(1235);

  initWebserver();

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.print(WiFi.localIP());
  Serial.println("");
  display.clear();
  display.drawStringMaxWidth(0, 0, 128, "Connected!" );
  display.display();

}

unsigned long lastMillis = 0;

void loop() {

  if (millis() - lastMillis >= 5000) {
    lastMillis = millis();
    Serial.println(getWifiStatus());
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Attempting to reconnect");
      display.clear();
      delay(100);
      display.display();
      display.drawStringMaxWidth(0, 0, 128, "Reconnecting..." );
      display.display();
      WiFi.reconnect();
      delay(1000);
      return;
    }
  }
  double distanz = (double) pulseIn(LIDAR_PWM_PIN, HIGH, 100000) / 1000;
  Serial.println(distanz);
  if (!demo) {
    if (distanz >= 1.0D && distanz <= 10.0D || !digitalRead(0)) {
      Udp.beginPacket(ESP_HOST, 1234);
      Udp.print(String("DETECTAUTOTRIGGER"));
      Udp.endPacket();
    }
  } else {
    if (distanz >= 0.5D && distanz <= 2.5D) {
      Udp.beginPacket(ESP_HOST, 1234);
      Udp.print(String("DETECTAUTOTRIGGER"));
      Udp.endPacket();
    }
  }
  server.handleClient();

  display.clear();
  display.drawStringMaxWidth(0, 0, 128, String(String(WiFi.RSSI()) + String("dbm")));
  display.display();

  delay(20);

}

String getWifiStatus() {
  switch (WiFi.status()) {
    case 0:
      return String("IDLE_STATUS");
    case 1:
      return String("NO_SSID_AVAIL");
    case 2:
      return String("SCAN_COMPLETED");
    case 3:
      return String("CONNECTED");
    case 4:
      return String("CONNECT_FAILED");
    case 5:
      return String("CONNECTION_LOST");
    case 6:
      return String("DISCONNECTED");
    case 255:
      return String("NO_SHIELD");
  }
  return String("UNKNOWN");
}

/**
  Unterroutine, welche Webserver initialisiert.
*/
void initWebserver() {
  setupUpdater();
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
}

/*
   Unteroutine, welche OTA über Webserver handhabt.
*/
void setupUpdater() {
  server.on("/firmware", []() {
    server.send(200, "text/html", serverIndexFirmware);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    esp_wifi_wps_disable(); ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  Serial.println("OTA Ready!");
}

/**
   Handle-Methode(notFound)
   wird aufgerufen, wenn die URI dem Webserver nicht bekannt ist.
   Gibt dem Client URI, Methode, Argumente sowie Argumentname als Plain-Text zurück.
*/
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

/**
   Handle-Methode(root)
   wird aufgerufen, wenn die URI leer ist, also das Wurzelverzeichnis aufgerufen wird.
   Gibt dem Client das HTML-Dokument, welches in websites.h (serverIndex) steht als html zurück.
*/
void handleRoot() {
  server.send(200, "text/html", serverIndex);
}

