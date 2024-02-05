#include <Wire.h>
#include <WiFi.h>
#include <FastLED.h>

struct NetworkInfo {
  char ssid[32];
  char bssid[18];
  int32_t rssi;
  char security[20];
  uint8_t channel;
  char type;
};

//different i2c pins, same rgb led
//#define PICO


// 1..4
#define NODEID 5
#if NODEID==1
const int channels[] = {1, 12};
#elif NODEID==2
const int channels[] = {2, 3, 4};
#elif NODEID==3
const int channels[] = {6, 13};
#elif NODEID==4
//use m5stack board conf (2.1.0), not esp32 (2.0.11)
//different i2c pins, different rgb led
#define ATOMS3
const int channels[] = {5, 7, 8};
#elif NODEID==5
const int channels[] = {9, 14};
//since 14 unused in eu
#define enableBLE
#define MATRIX
#elif NODEID==6
const int channels[] = {10, 11};
#endif

volatile bool scan = false;
const int i2c_slave_address = 0x55;
#ifdef ATOMS3
#define LED_PIN 35
#define SUB_SDA 2
#define SUB_SCL 1
#else
#define LED_PIN 27
#ifdef PICO
#define SUB_SDA 32
#define SUB_SCL 33
#endif
#endif
const int MAX_NETWORKS = 500;
NetworkInfo networks[MAX_NETWORKS];
int networkCount = 0;
int currentNetworkIndex = 0;
#ifdef MATRIX
const int numLeds = 25;
#else
const int numLeds = 1;
#endif
CRGB led[numLeds];

const int channelCount = (sizeof(channels) / sizeof(channels[0]));

const int maxMACs = channelCount * MAX_NETWORKS;
String macAddressArray[maxMACs];
int macArrayIndex = 0;
bool overFlow = false;

bool isMACSeen(const String& mac) {
  for (int i = 0; i < (overFlow ? maxMACs : macArrayIndex); i++) {
    if (macAddressArray[i] == mac) {
      return true;
    }
  }
  return false;
}

#ifdef enableBLE
//copied from j.hewitt rev3

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

BLEScan* pBLEScan;

#define mac_history_len 256

struct mac_addr {
  unsigned char bytes[6];
};

struct mac_addr mac_history[mac_history_len];
unsigned int mac_history_cursor = 0;

void save_mac(unsigned char* mac) {
  if (mac_history_cursor >= mac_history_len) {
    mac_history_cursor = 0;
  }
  struct mac_addr tmp;
  for (int x = 0; x < 6 ; x++) {
    tmp.bytes[x] = mac[x];
  }

  mac_history[mac_history_cursor] = tmp;
  mac_history_cursor++;
}

boolean seen_mac(unsigned char* mac) {

  struct mac_addr tmp;
  for (int x = 0; x < 6 ; x++) {
    tmp.bytes[x] = mac[x];
  }

  for (int x = 0; x < mac_history_len; x++) {
    if (mac_cmp(tmp, mac_history[x])) {
      return true;
    }
  }
  return false;
}

void print_mac(struct mac_addr mac) {
  for (int x = 0; x < 6 ; x++) {
    Serial.print(mac.bytes[x], HEX);
    Serial.print(":");
  }
}

boolean mac_cmp(struct mac_addr addr1, struct mac_addr addr2) {
  for (int y = 0; y < 6 ; y++) {
    if (addr1.bytes[y] != addr2.bytes[y]) {
      return false;
    }
  }
  return true;
}

void clear_mac_history() {
  struct mac_addr tmp;
  for (int x = 0; x < 6 ; x++) {
    tmp.bytes[x] = 0;
  }

  for (int x = 0; x < mac_history_len; x++) {
    mac_history[x] = tmp;
  }

  mac_history_cursor = 0;
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      unsigned char mac_bytes[6];
      int values[6];

      if (6 == sscanf(advertisedDevice.getAddress().toString().c_str(), "%x:%x:%x:%x:%x:%x%*c", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5])) {
        for (int i = 0; i < 6; ++i ) {
          mac_bytes[i] = (unsigned char) values[i];
        }

        if (!seen_mac(mac_bytes)) {
          save_mac(mac_bytes);
          addBleNetwork(advertisedDevice.getName().c_str(), advertisedDevice.getAddress().toString().c_str(), advertisedDevice.getRSSI());
        }
      }
    }
};
#endif

const int FEW_NETWORKS_THRESHOLD = 1;
const int MANY_NETWORKS_THRESHOLD = 8;
const int POP_INC = 75;   // Higher increment for popular channels
const int STD_INC = 50;  // Standard increment
const int RARE_INC = 30;      // Lower increment for rare channels
const int MAX_TIME = 500;
const int MIN_TIME = 50;

const int popularChannels[] = { 1, 6, 11 };
const int standardChannels[] = { 2, 3, 4, 5, 7, 8, 9, 10 };
const int rareChannels[] = { 12, 13, 14 };  // Depending on region
const int ble = 50;
int timePerChannel[15] = { ble, 300, 200, 200, 200, 200, 300, 200, 200, 200, 200, 300, 200, 200, 200 };
int incrementPerChannel[15] = {0, POP_INC, STD_INC, STD_INC, STD_INC, STD_INC, POP_INC, STD_INC, STD_INC, STD_INC, STD_INC, POP_INC, RARE_INC, RARE_INC, RARE_INC};

void setup() {
  Serial.begin(115200);
  Serial.println("[SLAVE] Starting up");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  FastLED.addLeds<WS2812, LED_PIN, GRB>(led, numLeds);
#ifdef MATRIX
  FastLED.setBrightness(32);
#endif
  setLed(CRGB::Black);
  FastLED.show();

#if defined(PICO) || defined(ATOMS3)
  Wire.begin(i2c_slave_address, SUB_SDA, SUB_SCL);
#else
  Wire.begin(i2c_slave_address);
#endif
  Wire.onRequest(requestEvent);
  Serial.println("[SLAVE] I2C initialized");
#ifdef enableBLE
  Serial.println("Setting up Bluetooth scanning");
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(false); //active scan uses more power, but get results faster
  pBLEScan->setInterval(timePerChannel[0]);
  pBLEScan->setWindow(40);  // less or equal setInterval value
#endif
  Serial.println("Hydrahead " + String(NODEID) + " started!");
}

void loop() {
  if (!scan) {
    return;
  }
  int savedNetworks = 0;
  if (networkCount < MAX_NETWORKS) {
#ifdef enableBLE
    BLEScanResults foundDevices = pBLEScan->start(2.5, false);
    Serial.print("Devices found: ");
    Serial.println(mac_history_cursor);
    savedNetworks += mac_history_cursor;
    Serial.println("Scan done!");
    pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
#endif
    for (int channelSelect = 0; channelSelect < channelCount; channelSelect++ ) {
      Serial.print("[SLAVE] Scanning ch ");
      Serial.println(String(channels[channelSelect]));
      int n = WiFi.scanNetworks(false, true, false, timePerChannel[channels[channelSelect]], channels[channelSelect]);
      if (n >= 0) {
        for (int i = 0; i < n; ++i) {

          String currentMAC = WiFi.BSSIDstr(i);
          if (isMACSeen(currentMAC)) {
            continue;
          }
          macAddressArray[macArrayIndex++] = currentMAC;
          if (macArrayIndex >= maxMACs) {
            macArrayIndex = 0;
            overFlow = true;
          }
          addWifiNetwork(WiFi.SSID(i), WiFi.BSSIDstr(i), WiFi.RSSI(i), WiFi.encryptionType(i), WiFi.channel(i));
          savedNetworks++;
        }
      }
      updateTimePerChannel(channels[channelSelect], n);
    }
  }
  if (savedNetworks > 0) {
    blinkLEDBlue();
  }
}

void addBleNetwork(const String& ssid, const String& bssid, int32_t rssi) {
  if (addNetwork(ssid, bssid, rssi, "[BLE]", 0, 'b'))
    Serial.println("[SLAVE] Added BLE device: SSID: " + ssid + ", BSSID: " + bssid + ", RSSI: " + String(rssi) + ", Security: {BLE], Channel: 0");
}

void addWifiNetwork(const String& ssid, const String& bssid, int32_t rssi, wifi_auth_mode_t encryptionType, uint8_t channel) {
  if (addNetwork(ssid, bssid, rssi, getAuthType(encryptionType), channel, 'w'))
    Serial.println("[SLAVE] Added wifi network: SSID: " + ssid + ", BSSID: " + bssid + ", RSSI: " + String(rssi) + ", Security: " + encryptionType + ", Channel: " + String(channel));
}

bool addNetwork(const String& ssid, const String& bssid, int32_t rssi, const String& encryptionType, uint8_t channel, char type) {
  if (!isNetworkInList(bssid) && networkCount < MAX_NETWORKS) {
    strncpy(networks[networkCount].ssid, ssid.c_str(), sizeof(networks[networkCount].ssid) - 1);
    strncpy(networks[networkCount].bssid, bssid.c_str(), sizeof(networks[networkCount].bssid) - 1);
    networks[networkCount].rssi = rssi;
    strncpy(networks[networkCount].security, encryptionType.c_str(), sizeof(networks[networkCount].security) - 1);
    networks[networkCount].channel = channel;
    networks[networkCount].type = type;
    networkCount++;
    return true;
  }
  return false;
}

bool isNetworkInList(const String& bssid) {
  for (int i = 0; i < networkCount; ++i) {
    if (strcmp(networks[i].bssid, bssid.c_str()) == 0) {
      return true;
    }
  }
  return false;
}

void requestEvent() {
  if (!scan) {
    //only start scanning if dom is ready
    scan = true;

  }
  if (currentNetworkIndex < networkCount) {
    Wire.write((byte*)&networks[currentNetworkIndex], sizeof(NetworkInfo));
    Serial.println("[SLAVE] Sending network: " + String(networks[currentNetworkIndex].ssid));
    currentNetworkIndex++;
    blinkLEDWhite();
  } else {
    Serial.println("[SLAVE] No new networks to send");
    currentNetworkIndex = 0;
    networkCount = 0;
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

void setLed(CRGB c) {
  for (int i = 0; i < numLeds; i++) {
    led[i] = c;
  }
}

void blinkLEDWhite() {
  setLed(CRGB::White);
  blinkLED();
}
void blinkLEDGreen() {
  setLed(CRGB::Green);
  blinkLED();
}
void blinkLEDBlue() {
  setLed(CRGB::Blue);
  blinkLED();
}

//blibk on matrix takes 3 times the blink on lite
void blinkLED() {
  int d = 50;
#ifdef MATRIX
  CRGB c = led[0];
//  for (int i = 0; i < 3; i++) {
  for (int i = 2; i > -1; i--) {
    if (i == 0) {
      //outer ring
      CRGB c2 = CRGB::Black;
      setLed(c);
      led[6] = c2;
      led[7] = c2;
      led[8] = c2;
      led[11] = c2;
      led[12] = c2;
      led[13] = c2;
      led[16] = c2;
      led[17] = c2;
      led[18] = c2;
    }
    if (i == 1) {
      setLed(CRGB::Black);
      //middle ring
      led[6] = c;
      led[7] = c;
      led[8] = c;
      led[11] = c;
      led[13] = c;
      led[1] = c;
      led[17] = c;
      led[18] = c;
    }
    if (i == 2) {
      setLed(CRGB::Black);
      //center point
      led[12] = c;
    }
    FastLED.show();
    delay(d);
    setLed(CRGB::Black);
  }
#else
  FastLED.show();
  delay(d);
  setLed(CRGB::Black);
#endif
  FastLED.show();
}
void updateTimePerChannel(int channel, int networksFound) {
  int timeIncrement = 0;
  // Adjust the time per channel based on the number of networks found
  if (networksFound >= MANY_NETWORKS_THRESHOLD) {
    timeIncrement = incrementPerChannel[channel];
  } else if (networksFound <= FEW_NETWORKS_THRESHOLD) {
    timeIncrement = - incrementPerChannel[channel];
  }
  int timePerChannelOld = timePerChannel[channel];
  timePerChannel[channel] += timeIncrement;
  if (timePerChannel[channel] > MAX_TIME) {
    timePerChannel[channel] = MAX_TIME;
  } else if (timePerChannel[channel] < MIN_TIME) {
    timePerChannel[channel] = MIN_TIME;
  }
  if (timePerChannelOld != timePerChannel[channel]) {
    Serial.print("Saw ");
    Serial.print(networksFound);
    Serial.print(", so I updated timePerChannel for channel ");
    Serial.print(channel);
    Serial.print(" by ");
    Serial.print(timeIncrement);
    Serial.print(" to ");
    Serial.println(timePerChannel[channel]);
  }
}
