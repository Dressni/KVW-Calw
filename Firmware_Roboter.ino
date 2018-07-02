/*
   @author Nick Dressler
   @created on 15.01.2018
   @last change 07.05.2018
   @version 1.7
   @changes:
   -version 1.0: Grundgerüst, UDP, WiFi-AP, Lidar-PWM
   -version 1.1: Abfangen unerwarteter Messungen des Lidar(Timeout des PWM resultiert in 0m)
   -version 1.2: Thread für LED-Laufanimation implementiert.
   -version 1.3: Kanaloptimierung und 20MhZ-Bandbreite implementiert, Verbesserungen des Protokolls(App erfragt mode etc.)
   -version 1.4: neue Libary für LED, welche Thread unterstützt(Hardware RTC).
                 -direktere LED Ansteuerung
   -version 1.5: OTA über Webserver realisiert.
   -version 1.6: UDP Controller-App Paketverlust minimiert.
   -version 1.7: Logik für Relais angepasst, Webserver für APK-DL auf 192.168.4.1
   @Note: PWM is used to get the distance from the lidar
          Geschwindigkeit wird alle 500ms gemittelt
          Verhalten, wenn Laser ein Pulse der Länge 0 gibt, wird abgefangen.(out of range)
          -WiFi-Scan-Algorithmus wurde implementiert.
          -Robustness UDP
          -Firmware, bei welcher LIDAR im Roboter montiert ist.

*/

//Include Header-Dateien sowie Libaries
#include <Preferences.h>
#include "WS2812.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <ESP32WebServer.h>
#include <Wire.h>
#include <SSD1306.h>
#include "font.h"
#include "images.h"
#include "websites.h"
#include <SPI.h>
#include <SD.h>
#include <Update.h>
#include <esp_wps.h>
#include <soc/rtc.h>

//Max Size(bytes) of incoming data via UDP
#define PACKET_IN_SIZE 64

//DEBUG (Lidar-Emulator)
#define PIEZO 12
#define TONE_CHANNEL 0

//Serial-Debug aktivieren/deaktivierens
#define SERIAL_DEBUG false

//Temperatur der CPU
extern "C" {
  uint8_t temprature_sens_read();
}


//GPIO-Pins für Rechter/Linker Pneumatiktreiber
#define L_PIN 4
#define R_PIN 17
#define ENABLE_PIN 14


//LED-Strip
#define LED 2
#define NUM_PIXELS 82
#define CM_PER_SEC 120
int laengeLED;
int delayTimeLED;
int ledColor[] = {0, 255, 0};


//Prozentsatz der Hysterese
float resumeRatio = 1.2f;

//Maximale zugelassene, gültige Geschwindigkeit (km/h)
float vMax = 80.0f;
float maxDeltaS;

//Pulse-In-Pin für LIDAR
#define LIDAR_PWM_PIN 16


//WiFi-SoftAP credentials
#define ssid "KVW-Calw Roboter"
#define password "xxxxxxxx"
#define BROADCAST "192.168.4.255"

//Instanziierungen der Objekte
WS2812 strip = WS2812((gpio_num_t)LED, NUM_PIXELS, 0);
SSD1306 oled(0x3c, SDA, SCL);
TaskHandle_t Task1, Task2, Task3, Task4, Task5, TaskLED, TaskPWM, TaskAutoLED, TaskLEDInit;
SemaphoreHandle_t baton;
WiFiUDP Udp;
ESP32WebServer server(80);
Preferences preferences;

//Globale Variablen
boolean mode = false;
boolean dropped = false;
float triggerEntfernung = -1;
boolean hasFinishedArms = true;
boolean ledAnimation = false;
int state = 0;
String recev;

//Lidar-Größen
double velocity, distance, distanceOld;
boolean rangeFlag = false;

//Oled-Setup-Menü Größen
boolean keepPage;
unsigned long alteZeit;

//Util-Größen
uint8_t amountClients;
int8_t wifiPower = 0;

//Setup-Teil
void setup() {

  //Pin-Modes
  pinMode(L_PIN, OUTPUT);
  pinMode(R_PIN, OUTPUT);
  digitalWrite(L_PIN, HIGH);
  digitalWrite(R_PIN, HIGH);

  pinMode(LIDAR_PWM_PIN, INPUT);

  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, LOW);

  //Max Power
  rtc_clk_cpu_freq_set(RTC_CPU_FREQ_240M);

  //PWM-Emulator init
  ledcSetup(TONE_CHANNEL, 20, 10);
  ledcAttachPin(PIEZO, TONE_CHANNEL);
  digitalWrite(PIEZO, LOW);

  //LED-Rainbow-Test
  ledSetup();

  //Math-Stuff
  /*vMax = 80 km/h = 22.22 m/s
     dS = v*dt = 22.22 m/s * 0.02s = 0.44444
     Hier wird das hochste deltaS berechnet, bei welchem noch im Auto-Mode ausgelöst wird (Plausibilitätskontrolle)
  */
  maxDeltaS = (float) vMax / 3.6f * 0.02f;

  //Interrupt für Oled-Menü
  attachInterrupt(digitalPinToInterrupt(0), toggle, FALLING);

  //Begin der seriellen Kommunikation
  Serial.begin(115200);
  Serial.println();

  //WiFi-Verbindung wird auf Station gesetzt, ggf. Verbindung wird getrennt. Vorbereitung für Scan der WiFi-Umgebung.
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  //EEPROM-Speicher init
  preferences.begin("seminarkurs", false);
  triggerEntfernung = preferences.getFloat("entfernung", 20.5F);


  //OLED wird initialisiert, boot-Screen wird angezeigt.
  oled.init();
  oled.setFont(SansSerif_plain_14);
  oled.clear();
  oled.drawXbm(21, 0, kvw_width, kvw_height, kvw);
  oled.display();

  delay(500);

  //Unterroutine (WiFi-Scan, bester Kanal, UDP Setup)
  doScan();

  //Webserver wird initialisiert
  initWebserver();

  //OTA-Firmware init
  setupUpdater();

  //SD-Unterstützung für APK-Download
  if (SD.begin()) {
    Serial.println("SD init done!");
  }

  //"baton"(Thread-synchronisation) wird initialisiert
  baton = xSemaphoreCreateMutex();

  //6 Threads werden gestartet(Core 0 = !loop-Core, Core 1 = loop-Core)
  xTaskCreatePinnedToCore(&udpThread,             "udp",          4096, NULL, 1, &Task1,    0);
  xTaskCreatePinnedToCore(&loopThread,            "loop",         4096, NULL, 1, &Task2,    1);
  xTaskCreatePinnedToCore(&connectedClientsTask,  "ap",           4096, NULL, 1, &Task3,    0);
  xTaskCreatePinnedToCore(&lidarGettingThread,    "lidar",        4096, NULL, 1, &Task4,    0);
  xTaskCreatePinnedToCore(&debugThread,           "debug",        4096, NULL, 1, &Task5,    0);
  xTaskCreatePinnedToCore(&PWMThread,             "PWMEmulator",  4096, NULL, 1, &TaskPWM,  0);

  //Enable für Pneumatiktreiber wird auf 3.3V gesetzt.
  digitalWrite(ENABLE_PIN, HIGH);

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

/*
   Unterroutine für LED's
*/
void aufblitzen(int start, int stop) {
  for (byte b = 0; b < 3; b++) {
    for (int i = start; i < stop; i++) {
      strip.setPixel(i, 255, 255, 255);
    }
    strip.show();
    delay(10);
    for (int i = start; i < stop; i++) {
      strip.setPixel(i, 0, 0, 0);
    }
    strip.show();
    delay(50);
  }
}

/**
   Setup der LED's, berechnet laengeLED, sowie delayTimeLED und startet Rainbow-Test in einem Thread.
*/
void ledSetup() {
  aufblitzen(0, NUM_PIXELS);

  for (uint8_t i = 0; i < NUM_PIXELS; i++) {
    strip.setPixel(i, 0, 0, 0);
  }
  strip.show();
  laengeLED = (int) ((double)NUM_PIXELS * 1.75D);
  delayTimeLED = (int) ((1.75D / CM_PER_SEC) * 1000);
  xTaskCreatePinnedToCore(&LEDTestThread,             "LedTest",  4096, NULL, 1, &TaskLEDInit,  0);

}

/**
   Rainbow-Test Methode (R,G,B)
*/
void LEDTestThread(void *pvParameters) {
  while (1) {
    for (int i = 0; i < NUM_PIXELS; i++) {
      strip.setPixel(i, 255, 0, 0);
      strip.show();
    }
    for (int i = 0; i < NUM_PIXELS; i++) {
      strip.setPixel(i, 0, 255, 0);
      strip.show();
    }
    for (int i = 0; i < NUM_PIXELS; i++) {
      strip.setPixel(i, 0, 0, 255);
      strip.show();
    }
    for (int i = 0; i < NUM_PIXELS; i++) {
      strip.setPixel(i, 255, 255, 255);
      strip.show();
    }
    for (int i = 0; i < NUM_PIXELS; i++) {
      strip.setPixel(i, 0, 0, 0);
      strip.show();
    }
    vTaskDelete(TaskLED);
  }
}

/**
   Handle-Methode(notFound)
   wird aufgerufen, wenn die URI dem Webserver nicht bekannt ist.
   Gibt dem Client URI, Methode, Argumente sowie Argumentname als Plain-Text zurück.
*/
void handleNotFound() {
  if (loadFromSdCard(server.uri())) return;
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


/**
   Methode, welche aufgerufen wird, wenn ein Dateidownload von der SD-Karte stattfinden soll.
*/
bool loadFromSdCard(String path) {
  String dataType = "text/plain";
  if (path.endsWith("/")) path += "index.htm";

  if (path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if (path.endsWith(".htm")) dataType = "text/html";
  else if (path.endsWith(".css")) dataType = "text/css";
  else if (path.endsWith(".js")) dataType = "application/javascript";
  else if (path.endsWith(".png")) dataType = "image/png";
  else if (path.endsWith(".gif")) dataType = "image/gif";
  else if (path.endsWith(".jpg")) dataType = "image/jpeg";
  else if (path.endsWith(".ico")) dataType = "image/x-icon";
  else if (path.endsWith(".xml")) dataType = "text/xml";
  else if (path.endsWith(".pdf")) dataType = "application/pdf";
  else if (path.endsWith(".zip")) dataType = "application/zip";

  File dataFile = SD.open(path.c_str());
  if (dataFile.isDirectory()) {
    path += "/index.htm";
    dataType = "text/html";
    dataFile = SD.open(path.c_str());
  }

  if (!dataFile)
    return false;

  if (server.hasArg("download")) dataType = "application/octet-stream";

  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
    Serial.println("Sent less data than expected!");
  }

  dataFile.close();
  return true;
}

/**
   Handle-Methode(Debug)
   wird aufgerufen, wenn URI = /debug ist. Gibt globale Variablen etc als HTML-Dokument zurück.
*/
void handleDebug() {
  server.send(200, "text/html", "<head><meta http-equiv=\"refresh\"content=\"0.5\"/></head><h1>Test" + String(millis()) + "<br><p>" + String(getDebug()) + "</p></h1>");
}

/**
  Unterroutine, welche Webserver initialisiert.
*/
void initWebserver() {
  server.on("/", handleRoot);
  server.on("/debug", handleDebug);
  server.onNotFound(handleNotFound);
  server.begin();
}

/**
    Unterroutine setupWiFi(n : GZ)
    Erstellt AP und bindet UDP an Port 1234.
    Scannt WiFi-Umgebung und sucht besten Kanal. (Siehe hierzu Struktorgramm)
*/

void setupWiFi(int n) {
  boolean channels[13];
  int rssi[13];
  boolean allBlocked = false;

  for (int i = 0; i < sizeof(channels); i++) {
    channels[i] = false;
    rssi[i] = 0;
  }

  for (int i = 0; i < n; i++) {
    int ch = WiFi.channel(i);
    int rssii = WiFi.RSSI(i);
    String ssidd = WiFi.SSID(i);
    String bssidd = WiFi.BSSIDstr(i);
    if ((ch >= 1 && ch <= 13) || (rssii >= -110 && rssii < 0) || (ssidd.length() > 0) || (bssidd.length() > 0)) {
      channels[ch] = true;
      if (rssii > rssi[i] || rssi[i] == 0)
        rssi[i] = rssii;
    }
  }

  allBlocked = true;

  for (int i = 0; i < sizeof(channels); i++) {
    if (!channels[i]) {
      allBlocked = false;
      break;
    }
  }

  int best;

  if (allBlocked) {
    int rsssi = 0;
    for (int i = 0; i < sizeof(channels); i++) {
      if (rssi[i] < rsssi) {
        rsssi = rssi[i];
        best = (i + 1);
      }
    }
  } else {
    int gap = 0;
    int largestGap = 0;
    int bestChannel = 0;

    for (int i = 0; i < sizeof(channels); i++) {
      // Kanal frei, Lücke wird erhöht
      if (!channels[i]) {
        gap++;
        // Kanal belegt, bester Spot zwischen letztem freien und aktuellem i als Kanal
      } else {
        if (gap > largestGap) {
          bestChannel = i - gap + (gap / 2);
          largestGap = gap;
        }
        gap = 0;
      }
      if (gap > largestGap) {
        bestChannel = i - gap + (gap / 2);
        largestGap = gap;
      }
    }
    best = bestChannel + 1;
  }
  //AP-Mode aktivieren, AP wird mit ssid und password erstellt, Kanal ist best, SSID wird gebroadcasted und nur ein Client zugelassen.
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password, best, 0, 1);
  //Bandbreite des AP(802.11.n auf 20MhZ, sowie +20dBm=100mW Leistung)
  esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
  esp_wifi_set_max_tx_power(127);
  //UDP wird auf lokalen Port 1234 gebunden.
  Udp.begin(1234);
}

//interrupt-Routine des Oled-Menüs
void toggle() {
  //Entprellung des Tasters(250ms)
  if (millis() - alteZeit > 250) {
    alteZeit = millis();
    keepPage = false;
  }
}

//Unterroutine, welche alle AP's in der Nähe scannt und auf OLED ausgibt.
void doScan() {
  keepPage = true;
  oled.clear();
  oled.setTextAlignment(TEXT_ALIGN_CENTER);
  oled.drawString(64, 44, "Scanning...");
  oled.drawXbm(34, 5, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
  oled.display();
  int n = WiFi.scanNetworks(false, true, false, 1500);
  oled.clear();
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(5, 20, String(String(n) + " " + "Netzwerk" + String(n > 1 ? "e" : "")));
  oled.drawString(5, 35, "gefunden!");
  oled.display();

  //Übergabe Anzahl (n) der AP in der Nähe.
  setupWiFi(n);

  unsigned long oldTime = millis();
  while (keepPage && millis() - oldTime < 3000) {
    delay(10);
  }
  if (n > 0) {
    for (int i = 0; i < n; ++i) {
      keepPage = true;
      oled.clear();
      oled.drawString(5, 0, String(i + 1) + ": " + String(WiFi.SSID(i)));
      if ((WiFi.encryptionType(i) != WIFI_AUTH_OPEN)) {
        oled.drawXbm(128 - wifilock_width - 3, 3, wifilock_width, wifilock_height, wifilock);
      }
      oled.drawString(5, 19, String("Ch: " + String(WiFi.channel(i))));
      oled.drawString(5, 33, String("RSSI: " + String(WiFi.RSSI(i)) + "dBm"));
      oled.setFont(SansSerif_plain_10);
      oled.drawString(5, 50, String(WiFi.BSSIDstr(i)));
      oled.setFont(SansSerif_plain_14);
      oled.display();
      unsigned long oldTime = millis();
      while (keepPage && millis() - oldTime < 3000) {
        delay(10);
      }
    }
  }
}

/**
   Hilfsmethode, welche alle relevanten Variablen als String zurückgibt.
*/
String getDebug() {
  String debug = "\n";
  debug += "Mode: ";
  if (mode) debug += "Auto";
  else debug += "Manual";
  debug += "\nTriggerEntfernung: " + String(triggerEntfernung);
  debug += "\nv= " + String(velocity);
  debug += "\ns= " + String(distance) + " | " + String(distanceOld);
  debug += "\nLaenge der LEDs (cm): " + String(laengeLED);
  debug += "\nDelay-Zeit der LED's(ms): " + String(delayTimeLED);
  return debug;
}

/**
   Debug-Thread.
   Überprüft WiFi-Leistung alle 500ms
   Gibt ggf. den Wert an die serielle Schnittstelle aus, wenn GPIO0(Button) gedrückt ist und SERIAL_DEBUG-Flag gesetzt ist.
*/
void debugThread(void *pvParameters) {
  while (1) {
    if (digitalRead(0) && SERIAL_DEBUG) {
      Serial.println(getDebug());
      esp_err_t success = esp_wifi_get_max_tx_power(&wifiPower);
      Serial.println("WiFi-Power: " + String(success) + " (" + String(wifiPower));
    }
    //esp_wifi_set_max_tx_power(127);
    esp_wifi_get_max_tx_power(&wifiPower);
    delay(500);
  }
}

/**
   PWM-Emulator-Thread
   Mittelt analogen Wert an A0 (1000x) und erzeugt Testsignal als LIDAR-Ersatz.
   Nur für Testzwecke. Wird für den eigentlichen Betrieb nicht benötigt.
*/
void PWMThread(void *pvParameters) {
  int c = 0;
  while (1) {
    double sum = 0.0D;
    for (int i = 0; i < 1000; i++)
      sum += analogRead(A0);
    int avg = round((double) sum / 1000);
    ledcWrite(TONE_CHANNEL, map(avg, 0, 4095, 1, 1023));
    delay(90);
  }
}

/**
   Thread, welcher die Mittlere Geschwindigkeit des LIDAR über 250ms berechnet.
*/
void lidarGettingThread(void *pvParameters) {
  while (1) {
    // v = dS / dT
    //Mittlere Geschwindigkeit über 250ms
    velocity = (double) (distance - distanceOld) * 4;
    distanceOld = distance;
    delay(250);
  }
}

/**
   Methode für Thread, welcher die Anzahl der Verbundenen WiFi-Clients überprüft.
   Des weiteren wird dier der Webserver "gefüttert". Sodass ein Client versorgt wird.
*/
void connectedClientsTask(void *pvParameters) {
  while (1) {
    amountClients = WiFi.softAPgetStationNum();
    server.handleClient();
    delay(20);
  }
}

/**
   Methode für loop-Thread, welcher PWM des LIDAR ließt.
*/
void loopThread(void *pvParameters) {
  while (1) {
    //val = 0 ... 40000
    //HIGH-Zeit des PWM-Signals
    int pulse = pulseIn(LIDAR_PWM_PIN, HIGH, 100000);
    double distanceAlt = distance;
    //Wenn Entfernung in Messbereich erfasst wurde.
    if (pulse > 0) {
      //Aktuelle Entfernung in m
      distance = (double) pulse / 1000;
      rangeFlag = true;
    } else if (pulse == 0) {
      distance = 0;
      rangeFlag = false;
    }
    //Handle auto-mode, pulse > 80 um Nahbereich auszublenden.
    if (mode && pulse > 80) {
      /*vMax = 80 km/h = 22.22 m/s
         dS = v*dt = 22.22 m/s * 0.02s = 0.44444
      */
      /*
         Plausibilitätsprüfung, automatisch wird nur Ausgelöst,
         wenn die Distanz kleiner der triggerEntferung ist und die (momentane)Geschwindigkeit nicht größer als vMax.
      */
      if (distance <= triggerEntfernung && abs(distance - distanceAlt) <= maxDeltaS) {
        handleHand(0);
      }
    }
    delay(20);
  }
}


//UDP-Thread, welcher sich um UDP Rx,Tx kümmert.
void udpThread(void *pvParameters) {
  while (1) {
    //xSemaphoreTake( baton, portMAX_DELAY );
    char incomingPacket[PACKET_IN_SIZE];
    int packetSize = Udp.parsePacket();
    if (packetSize)
    {
      int len = Udp.read(incomingPacket, PACKET_IN_SIZE);
      if (len > 0)
        incomingPacket[len] = 0;
      recev = String(incomingPacket);

      if (recev.startsWith("DROP")) {
        dropped = true;
        continue;
      }
      if (recev.startsWith("SETMODE=auto")) {
        mode = true;
        Serial.println("Mode auf auto gesetzt!");
      } else if (recev.startsWith("SETMODE=manual")) {
        mode = false;
        Serial.println("Mode auf manual gesetzt!");
      } else if (recev.startsWith("SETTRIGGER=")) {
        String distTrigger = recev.substring(11);
        triggerEntfernung = distTrigger.toFloat();
        preferences.putFloat("entfernung", triggerEntfernung);
        Serial.println("Neue Auslöseentfernung: " + String(triggerEntfernung));
      } else if (recev.startsWith("TRIGGER=") && !mode) {
        String triggerHand = recev.substring(8);
        int hand = triggerHand.toInt();
        handleHand(hand);
        Serial.println("Trigger(Hand): " + String(hand));
      } else if (recev.startsWith("REQUESTTRIGGER?")) {
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        Udp.print(String("RESPONSETRIGGER&") + String(triggerEntfernung));
        Udp.endPacket();
        delay(25);
        byte ct = 0;
        while (dropped && ++ct < 20) {
          Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
          Udp.print(String("RESPONSETRIGGER&") + String(triggerEntfernung));
          Udp.endPacket();
          delay(5);
        }
      } else if (recev.startsWith("REQUESTMODE?")) {
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        String toPrint = String(String("RESPONSEMODE&") + (mode ? String("auto") : String("manual")));
        Udp.print(toPrint);
        Udp.endPacket();
        delay(25);
        byte ct = 0;
        while (dropped && ++ct < 20) {
          Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
          String toPrint = String(String("RESPONSEMODE&") + (mode ? String("auto") : String("manual")));
          Udp.print(toPrint);
          Udp.endPacket();
          delay(5);
        }
      }
      if (!digitalRead(0))Serial.println("Rx: " + String(recev));
    }
    Udp.beginPacket(BROADCAST, 6565);

    //Struktur: [Entfernung in m],[Geschwindigkeit in m/s]
    String tx = String(distance) + "," + String(velocity);
    if (!digitalRead(0))Serial.println("Tx: " + String(tx));
    Udp.print(tx);
    Udp.endPacket();
    //xSemaphoreGive( baton );
    delay(20);
  }
}

/**
   Thread für heben des Armes und LED-Animation
*/
void ledArmThread(void *pvParameters) {
  //Links
  if (state == 1) {
    Links();
    //Rechts
  } else if (state == 2) {
    Rechts();
    //Keine
  } else if (state == 3) {
    Keine();
    //Beide
  } else if (state == 4) {
    Beide();
    //Zufall
  } else if (state == 0) {
    boolean richtung = getRandomBoolean();
    //Rechts
    if (richtung) {
      state = 2;
      sendDatagramBroadcast("RESPONSEAUTOTRIGGER=R");
      delay(25);
      byte ct = 0;
      while (dropped && ++ct < 20) {
        sendDatagramBroadcast("RESPONSEAUTOTRIGGER=R");
        delay(5);
      }
      Rechts();
      //Links
    } else {
      state = 1;
      sendDatagramBroadcast("RESPONSEAUTOTRIGGER=L");
      delay(25);
      byte ct = 0;
      while (dropped && ++ct < 20) {
        sendDatagramBroadcast("RESPONSEAUTOTRIGGER=L");
        delay(5);
      }
      Links();
    }

    int resumeDist = triggerEntfernung * resumeRatio;
    //Abbruchbedingung
    while (distance <= resumeDist && mode && rangeFlag) {
      delay(10);
    }

    //Wenn Abbruchbedingung eingetreten ist, wird 2s verzögert als Puffer.
    unsigned long startTime = millis();
    while (mode && (millis() - startTime < 2000)) {
      delay(10);
    }

    sendDatagramBroadcast("RESPONSEAUTOTRIGGER=NONE");
    delay(25);
    byte ct = 0;
    while (dropped && ++ct < 20) {
      sendDatagramBroadcast("RESPONSEAUTOTRIGGER=NONE");
      delay(5);
    }

    Keine();
  }

  hasFinishedArms = true;
  vTaskDelete(TaskLED);

}

/**
   Hilfsmethode, welche msg an Broadcastadresse per UDP sendet.
*/
void sendDatagramBroadcast(String msg) {
  Udp.beginPacket(BROADCAST, 6565);
  Udp.print(msg);
  Udp.endPacket();
}
/**
   Unterroutine, welche LED-Animation startet, sofern die Animation nicht schon läuft.
*/
void startLedAnimation() {
  if (ledAnimation) return;
  ledAnimation = true;
  xTaskCreatePinnedToCore(&ledAnimationThread, "LedAnimationThread", 4096, NULL, 1, &TaskAutoLED, 0);
}
/**
   Unterroutine, welche die LED-Animation stoppt.
*/
void stopLedAnimation() {
  if (ledAnimation) {
    ledAnimation = false;
    vTaskDelete(TaskAutoLED);
    //LEDs aus
    for (int i = 0; i < NUM_PIXELS; i++) {
      strip.setPixel(i, 0, 0, 0);
    }
    strip.show();
    //digitalWrite(LED, LOW);
  }
}

/**
   Thread, welcher sich nur um die Animation der LED's kümmert.
*/
void ledAnimationThread(void *pvParameters) {
  while (ledAnimation) {
    //Links
    if (state == 1) {
      //aufblitzen(0, NUM_PIXELS / 2);
      for (int i = 0; i < NUM_PIXELS / 2; i++) {
        strip.setPixel(i, ledColor[0], ledColor[1], ledColor[2]);
        strip.show();
        delay(delayTimeLED);
      }
      //aufblitzen(0, NUM_PIXELS / 2);
      for (int i = 0; i < NUM_PIXELS; i++) {
        strip.setPixel(i, 0, 0, 0);
      }
      strip.show();
      //Rechts
    } else if (state == 2) {
      //aufblitzen(NUM_PIXELS / 2, NUM_PIXELS);
      for (int i = NUM_PIXELS / 2; i < NUM_PIXELS; i++) {
        strip.setPixel(i, ledColor[0], ledColor[1], ledColor[2]);
        strip.show();
        delay(delayTimeLED);
      }
      //aufblitzen(NUM_PIXELS / 2, NUM_PIXELS);
      for (int i = 0; i < NUM_PIXELS; i++) {
        strip.setPixel(i, 0, 0, 0);
      }
      strip.show();
      //Beide
    } else if (state == 4) {
      //aufblitzen(0, NUM_PIXELS);
      for (int i = 0; i < NUM_PIXELS / 2; i++) {
        strip.setPixel(i, ledColor[0], ledColor[1], ledColor[2]);
        strip.setPixel(i + NUM_PIXELS / 2, ledColor[0], ledColor[1], ledColor[2]);
        strip.show();
        delay(delayTimeLED);
      }
      //aufblitzen(0, NUM_PIXELS);
      for (int i = 0; i < NUM_PIXELS; i++) {
        strip.setPixel(i, 0, 0, 0);
      }
      strip.show();
    } else {
      delay(20);
    }
  }
}

/**
   Hilfsmethode, welche das Auslösen einer Hand handhabt und Thread startet, sofern das letzte Auslösen fertig ist.
*/
void handleHand(int hand) {
  if (!hasFinishedArms) return;
  hasFinishedArms = false;
  state = hand;
  xTaskCreatePinnedToCore(&ledArmThread, "ArmThread", 4096, NULL, 1, &TaskLED, 0);
}

//Rechter Arm wird gehoben, Animation wird gestartet.
void Rechts() {
  digitalWrite(L_PIN, HIGH);
  digitalWrite(R_PIN, LOW);
  startLedAnimation();
}

//Linker Arm wird gehoben, Animation wird gestartet.
void Links() {
  digitalWrite(L_PIN, LOW);
  digitalWrite(R_PIN, HIGH);
  startLedAnimation();
}

//Linker & Rechter Arm wird gehoben, Animation wird gestartet.
void Beide() {
  digitalWrite(L_PIN, LOW);
  digitalWrite(R_PIN, LOW);
  startLedAnimation();
}

//Linker & Rechter Arm wird gesenkt, Animation wird gestoppt.
void Keine() {
  digitalWrite(L_PIN, HIGH);
  digitalWrite(R_PIN, HIGH);
  stopLedAnimation();
}

//Hilfsmethode, welche einen boolean zufällig generiert. (Wird für Auto-Trigger benötigt)
boolean getRandomBoolean() {
  if (random(0, 2) == 1) {
    return true;
  }
  return false;
}


//Oled-Anzeigen im loop
String oldrx;
void loop() {
  //xSemaphoreTake( baton, portMAX_DELAY );
  oled.clear();
  oled.setFont(SansSerif_plain_14);
  oled.drawString(0, 0, String("Tx: " + String(distance) + "m ,"));
  oled.drawString(3, 15, String(velocity) + " m/s");
  if (!recev.startsWith("DROP")) {
    oldrx = recev;
  }
  oled.drawString(0, 35, String("Rx: " + String(oldrx)));
  oled.setFont(Lato_Thin_10);
  double temp = (temprature_sens_read() - 32) / 1.8;
  oled.drawString(0, 50, String("T: ") + String(temp) + String(", Clients: ") + String(amountClients) + "|" + String(wifiPower));

  //oled.drawProgressBar(0, 20, 127, 15, map(val, 0, 4096, 0, 100));
  //oled.drawProgressBar(0, 45, 127, 15, recev.toInt());
  oled.display();

  //xSemaphoreGive( baton );
  delay(20);
}

