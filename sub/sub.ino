#include <Wire.h>
#include <WiFi.h>
#include <FastLED.h>

struct NetworkInfo {
  char ssid[32];
  char bssid[18];
  int32_t rssi;
  char security[20];
  uint8_t channel;
};

//different i2c pins, same rgb led
//#define PICO

//use m5stack board conf (2.1.0), not esp32 (2.0.11)
//different i2c pins, different rgb led
//#define ATOMS3

// 1..4
#define NODEID 1
#if NODEID==1
const int channels[] = {1, 5, 9, 13};
#elif NODEID==2
const int channels[] = {2, 6, 10, 14};
#elif NODEID==3
const int channels[] = {3, 7, 11};
#elif NODEID==4
const int channels[] = {4, 8, 12};
#endif

const int i2c_slave_address = 0x55;
#ifdef ATOMS3
const int LED_PIN = 35;
#else
const int LED_PIN = 27;
#endif
const int MAX_NETWORKS = 500;
NetworkInfo networks[MAX_NETWORKS];
int networkCount = 0;
int currentNetworkIndex = 0;
CRGB led;

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



void setup() {
  Serial.begin(115200);
  Serial.println("[SLAVE] Starting up");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  FastLED.addLeds<WS2812, LED_PIN, GRB>(&led, 1);
  led = CRGB::Black;
  FastLED.show();

#ifdef PICO
  //stamp-pico-d4
  Wire.begin(i2c_slave_address, 32, 33);
#else
#ifdef ATOMS3
  Wire.begin(i2c_slave_address, 2, 1);
#endif
  Wire.begin(i2c_slave_address);
#endif
  Wire.onRequest(requestEvent);
  Serial.println("[SLAVE] I2C initialized");
  blinkLEDGreen();
}

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
int timePerChannel[14] = { 300, 200, 200, 200, 200, 300, 200, 200, 200, 200, 300, 200, 200, 200 };
int incrementPerChannel[14] = {POP_INC, STD_INC, STD_INC, STD_INC, STD_INC, POP_INC, STD_INC, STD_INC, STD_INC, STD_INC, POP_INC, RARE_INC, RARE_INC, RARE_INC};



int count = 0;

void loop() {
  if (networkCount < MAX_NETWORKS) {
    //int numNetworks = WiFi.scanNetworks(false, true, false, timePerChannel[channel - 1], channel);
    //WiFi.scanNetworks(true);
    for (int channelSelect = 0; channelSelect < channelCount; channelSelect++ ) {
      Serial.print("[SLAVE] Scanning ch ");
      Serial.println(String(channels[channelSelect]));
      int n = WiFi.scanNetworks(false, true, false, timePerChannel[channels[channelSelect] - 1], channels[channelSelect]);
      if (n >= 0) {
        count = 0;
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
          addNetwork(WiFi.SSID(i), WiFi.BSSIDstr(i), WiFi.RSSI(i), WiFi.encryptionType(i), WiFi.channel(i));
          count++;
        }
        if (count > 0) {
          blinkLEDBlue();
        }
      }
      updateTimePerChannel(channels[channelSelect], n);
    }
  }
}

void addNetwork(const String& ssid, const String& bssid, int32_t rssi, wifi_auth_mode_t encryptionType, uint8_t channel) {
  if (!isNetworkInList(bssid) && networkCount < MAX_NETWORKS) {
    strncpy(networks[networkCount].ssid, ssid.c_str(), sizeof(networks[networkCount].ssid) - 1);
    strncpy(networks[networkCount].bssid, bssid.c_str(), sizeof(networks[networkCount].bssid) - 1);
    networks[networkCount].rssi = rssi;
    strncpy(networks[networkCount].security, getAuthType(encryptionType), sizeof(networks[networkCount].security) - 1);
    networks[networkCount].channel = channel;
    Serial.println("[SLAVE] Added network: SSID: " + ssid + ", BSSID: " + bssid + ", RSSI: " + String(rssi) + ", Security: " + getAuthType(encryptionType) + ", Channel: " + String(channel));
    networkCount++;
  }
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


void blinkLEDWhite() {
  led = CRGB::White;
  blinkLED();
}
void blinkLEDGreen() {
  led = CRGB::Green;
  blinkLED();
}
void blinkLEDBlue() {
  led = CRGB::Blue;
  blinkLED();
}

void blinkLED() {
  FastLED.show();
  delay(50);
  led = CRGB::Black;
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
  int timePerChannelOld = timePerChannel[channel - 1];
  timePerChannel[channel - 1] += timeIncrement;
  if (timePerChannel[channel - 1] > MAX_TIME) {
    timePerChannel[channel - 1] = MAX_TIME;
  } else if (timePerChannel[channel - 1] < MIN_TIME) {
    timePerChannel[channel - 1] = MIN_TIME;
  }
  if (timePerChannelOld != timePerChannel[channel - 1]) {
    Serial.print("Saw ");
    Serial.print(networksFound);
    Serial.print(", so I updated timePerChannel for channel ");
    Serial.print(channel);
    Serial.print(" by ");
    Serial.print(timeIncrement);
    Serial.print(" to ");
    Serial.println(timePerChannel[channel - 1]);
  }
}
