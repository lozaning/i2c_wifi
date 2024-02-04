#define DomServer
//use esp32 (2.0.13), 80, dio 80, 8mb
#define S3OLED

#include <WiFi.h>
#ifdef DomServer
#include <WebServer.h>
#endif
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <TinyGPS++.h>
#ifdef S3OLED
#define BTN 41
#define GPSSERIAL Serial2
#include <M5AtomS3.h>
int const blinkSize = 10;
int fg = WHITE;
int bg = BLACK;
#else
#define BTN 39
#include <FastLED.h>
#define GPSSERIAL Serial1
const int LED_PIN = 27;
CRGB led;
#endif

TinyGPSPlus gps;
#ifdef DomServer
WebServer server(80);
#endif
File dataFile;
String fileName;

#ifdef DomServer
const char* ssid = "ESP32_Dom_Network";
const char* password = "12345678";
#endif
const int i2c_slave_address = 0x55;
#define TCAADDR 0x70
#define NUM_PORTS 6
bool oled = true;

struct NetworkInfo {
  char ssid[32];
  char bssid[18];
  int32_t rssi;
  char security[20];
  uint8_t channel;
  char type;
};

NetworkInfo receivedNetworks[NUM_PORTS];
#ifdef DomServer
int totalNetworksSent[NUM_PORTS] = { 0 };
#endif
#ifdef S3OLED
// 0 is BLE
int countNetworks[15] = {0};
#endif

void setup() {
#ifdef S3OLED
  auto cfg = M5.config();
  AtomS3.begin(cfg);
  //  AtomS3.Display.setTextFont(&fonts::TomThumb);
  //  AtomS3.Display.setTextSize(2);
  AtomS3.Display.setTextFont(&fonts::FreeSerif9pt7b);
  AtomS3.Display.setTextSize(1);
  AtomS3.Display.fillScreen(bg);
  AtomS3.Display.setTextColor(fg);
#else
  FastLED.addLeds<WS2812, LED_PIN, GRB>(&led, 1);
  led = CRGB::Black;
  FastLED.show();
  Serial.begin(115200);
#endif
#ifdef DomServer
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.println("AP IP Address: " + IP.toString());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
#endif

#ifdef S3OLED
  Wire.begin(2, 1);  // SDA, SCL
#else
  Wire.begin(26, 32);  // SDA, SCL
#endif
  Serial.println("[MASTER] I2C Master Initialized");
#ifdef S3OLED
  SPI.begin(7, 8, 6, -1);  // Initialize SPI for SD card
#else
  SPI.begin(23, 33, 19, -1);  // Initialize SPI for SD card
#endif
  if (!SD.begin(-1, SPI, 40000000)) {
    Serial.println("SD Card initialization failed!");
    return;
  }
  Serial.println("SD Card initialized.");

#ifdef S3OLED
  GPSSERIAL.begin(9600, SERIAL_8N1, 5, -1);  // GPS Serial
#else
  GPSSERIAL.begin(9600, SERIAL_8N1, 22, -1);  // GPS Serial
#endif
  waitForGPSFix();

  initializeFile();
  Serial.println("File created.");
}
#ifdef S3OLED
long displayUpdateDelay = 15000;
long lastDisplayUpdate = -15000;

void updateScreen() {
  if (!oled) return;
  bool update = (displayUpdateDelay + lastDisplayUpdate) < millis();
  draw(update);
}
void draw() {
  if (!oled) return;
  draw(true);
}
void draw(bool update) {
  if (update) {
    AtomS3.Display.fillScreen(bg);
    AtomS3.Display.setTextColor(fg);
#define COLS 2
#define ROWS 8
    int col = 0;
    int row = 0;
    for (int i = 0; i < 15 ; i++) {
      //0 is ble
      AtomS3.Display.drawString(String(i) + " : " + String(countNetworks[i]),
                                col * (AtomS3.Display.width() / COLS),
                                row * (AtomS3.Display.height() / ROWS));

      col++;
      if (col > (COLS - 1)) {
        row++;
        col = col % COLS;
      }
    }
    lastDisplayUpdate = millis();
  }
}
#endif

void loop() {
#ifdef S3OLED
  AtomS3.update();

  if (AtomS3.BtnA.wasReleased()) {
    oled = !oled;
    if (!oled) {
      AtomS3.Display.clearDisplay();
    }
  }


#endif

#ifdef DomServer
  server.handleClient();
#endif
#ifdef S3OLED
  updateScreen();
#endif

  while (GPSSERIAL.available() > 0) {
    gps.encode(GPSSERIAL.read());
  }
  for (uint8_t port = 0; port < NUM_PORTS; port++) {
    tcaselect(port);
    blinkLEDWhite();
    if (requestNetworkData(port)) {
#ifdef DomServer
      totalNetworksSent[port]++;
#endif
#ifdef S3OLED
      countNetworks[receivedNetworks[port].channel]++;
#endif
      logData(receivedNetworks[port], port);
    }
  }
}

#ifdef DomServer
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>ESP32 Network Stats</title>";
  html += "<script>";
  html += "setInterval(function() { fetch('/data').then(response => response.text()).then(data => { document.getElementById('networkStats').innerHTML = data; }); }, 2000);";
  html += "</script></head><body>";
  html += "<h1>Network Statistics from Subs</h1>";
  html += "<div id='networkStats'></div>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleData() {
  String data;
  data += "<h2>GPS Data:</h2>";
  data += "<p>Latitude: " + String(gps.location.lat(), 6) + "</p>";
  data += "<p>Longitude: " + String(gps.location.lng(), 6) + "</p>";
  data += "<p>HDOP: " + String(gps.hdop.value()) + "</p>";
  data += "<p>Satellites: " + String(gps.satellites.value()) + "</p>";

  for (int i = 0; i < NUM_PORTS; ++i) {

    data += "<h2>Port " + String(i) + ":</h2>";
    data += "<p>SSID: " + String(receivedNetworks[i].ssid) + "</p>";
    data += "<p>BSSID: " + String(receivedNetworks[i].bssid) + "</p>";
    data += "<p>RSSI: " + String(receivedNetworks[i].rssi) + "</p>";
    data += "<p>Security: " + String(receivedNetworks[i].security) + "</p>";
    data += "<p>Channel: " + String(receivedNetworks[i].channel) + "</p>";
    data += "<p>Total Networks Sent: " + String(totalNetworksSent[i]) + "</p>";
  }
  server.send(200, "text/plain", data);
}

#endif
void tcaselect(uint8_t i) {
  if (i >= NUM_PORTS) return;
  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << i);
  Wire.endTransmission();
  Serial.println("[MASTER] Switched to I2C port: " + String(i));
}

bool requestNetworkData(uint8_t port) {
  Wire.requestFrom(i2c_slave_address, sizeof(NetworkInfo));
  blinkLEDWhite();
  if (Wire.available() < sizeof(NetworkInfo)) {
    return false;
  }
  Wire.readBytes((byte*)&receivedNetworks[port], sizeof(NetworkInfo));
  if (receivedNetworks[port].channel > 14) {
    //    Serial.println("[MASTER] Dropping network");
    return false;
  }
  return true;
}

void waitForGPSFix() {
  unsigned long lastBlink = 0;
  const unsigned long blinkInterval = 300;  // Time interval for LED blinking
  bool ledState = false;

  Serial.println("Waiting for GPS fix...");
  while (!gps.location.isValid()) {
    if (GPSSERIAL.available() > 0) {
      gps.encode(GPSSERIAL.read());
    }

    // Non-blocking LED blink
    if (millis() - lastBlink >= blinkInterval) {
      lastBlink = millis();
      ledState = !ledState;
#ifdef S3OLED
      AtomS3.Display.fillRect((AtomS3.Display.width() - blinkSize), (AtomS3.Display.height() - blinkSize) , blinkSize , blinkSize , ledState ? MAGENTA : BLACK);
#else
      led = led ? CRGB::Green : CRGB::Black;
      FastLED.show();
#endif
    }
  }

  blinkLEDGreen();
  Serial.println("GPS fix obtained.");
}

void initializeFile() {
  int fileNumber = 0;
  bool isNewFile = false;

  // create a date stamp for the filename
  char fileDateStamp[16];
  sprintf(fileDateStamp, "%04d-%02d-%02d-",
          gps.date.year(), gps.date.month(), gps.date.day());

  do {
    fileName = "/wifi-scans-" + String(fileDateStamp) + String(fileNumber) + ".csv";
    isNewFile = !SD.exists(fileName);
    fileNumber++;
  } while (!isNewFile);

  if (isNewFile) {
    File dataFile = SD.open(fileName, FILE_WRITE);
    if (dataFile) {
      dataFile.println("WigleWifi-1.4,appRelease=1.300000,model=GPS Kit,release=1.100000F+00,device=M5ATOMHydra,display=NONE,board=ESP32,brand=M5");
      dataFile.println("MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type");
      dataFile.close();
      Serial.println("New file created: " + fileName);
    }
  } else {
    Serial.println("Using existing file: " + fileName);
  }
}

void logData(const NetworkInfo& network, uint8_t port) {
#ifdef DomServer
  if (strcmp(ssid, network.ssid) == 0) {
    Serial.println("Skip");
    return;
  }
#endif
  if (gps.location.isValid()) {
    String utc = String(gps.date.year()) + "-" + gps.date.month() + "-" + gps.date.day() + " " + gps.time.hour() + ":" + gps.time.minute() + ":" + gps.time.second();
    String dataString = String(network.bssid) + "," + "\"" + network.ssid + "\"" + "," + network.security + "," + utc + "," + String(network.channel) + "," + String(network.rssi) + "," + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6) + "," + String(gps.altitude.meters(), 2) + "," + String(gps.hdop.hdop(), 2);
    if (network.type == 'w' ) {
      dataString += ",WIFI";
    } else if (network.type == 'b' ) {
      dataString += ",BLE";
    }

    File dataFile = SD.open(fileName, FILE_APPEND);
    if (dataFile) {
      dataFile.println(dataString);
      dataFile.close();
      Serial.println("Data written: " + dataString);
      blinkLEDGreen();
    } else {
      Serial.println("Error opening " + fileName);
      blinkLEDRed();
    }
  } else {
    blinkLEDPurple();
  }
}

const char* getAuthType(uint8_t wifiAuth) {
  switch (wifiAuth) {
    case WIFI_AUTH_OPEN:
      return "[OPEN]";
    case WIFI_AUTH_WEP:
      return "[WEP]";
    case WIFI_AUTH_WPA_PSK:
      return "[WPA_PSK]";
    case WIFI_AUTH_WPA2_PSK:
      return "[WPA2_PSK]";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "[WPA_WPA2_PSK]";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "[WPA2_ENTERPRISE]";
    case WIFI_AUTH_WPA3_PSK:
      return "[WPA3_PSK]";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "[WPA2_WPA3_PSK]";
    case WIFI_AUTH_WAPI_PSK:
      return "[WAPI_PSK]";
    default:
      return "[UNDEFINED]";
  }
}

void blinkLEDWhite() {
#ifdef S3OLED
#else
  led = CRGB::White;
#endif
  blinkLED();
#ifdef S3OLED
  fg = WHITE;
#endif
}
void blinkLEDRed() {
#ifdef S3OLED
  fg = RED;
#else
  led = CRGB::Red;
#endif
  blinkLED();
#ifdef S3OLED
  fg = WHITE;
#endif
}
void blinkLEDGreen() {
#ifdef S3OLED
  fg = GREEN;
#else
  led = CRGB::Green;
#endif
  blinkLED();
#ifdef S3OLED
  fg = WHITE;
#endif
}
void blinkLEDPurple() {
#ifdef S3OLED
  fg = MAGENTA;
#else
  led = CRGB::Purple;
#endif
  blinkLED();
#ifdef S3OLED
  fg = WHITE;
#endif
}

void blinkLED() {
  if (!oled)
    return;
#ifdef S3OLED
  AtomS3.Display.fillRect((AtomS3.Display.width() - blinkSize), (AtomS3.Display.height() - blinkSize) , blinkSize , blinkSize , fg);
#else
  FastLED.show();
#endif
  delay(50);
#ifdef S3OLED
  AtomS3.Display.fillRect((AtomS3.Display.width() - blinkSize), (AtomS3.Display.height() - blinkSize) , blinkSize , blinkSize , bg);
#else
  led = CRGB::Black;
  FastLED.show();
#endif

}
