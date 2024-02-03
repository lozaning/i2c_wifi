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

const int i2c_slave_address = 0x55;
const int LED_PIN = 27;
const int MAX_NETWORKS = 500;
NetworkInfo networks[MAX_NETWORKS];
int networkCount = 0;
int currentNetworkIndex = 0;
CRGB led;

void setup() {
    Serial.begin(115200);
    Serial.println("[SLAVE] Starting up");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    FastLED.addLeds<WS2812, LED_PIN, GRB>(&led, 1);
    led = CRGB::Black;
    FastLED.show();

    Wire.begin(i2c_slave_address);
    Wire.onRequest(requestEvent);
    Serial.println("[SLAVE] I2C initialized");
}

void loop() {
    if (networkCount < MAX_NETWORKS) {
        WiFi.scanNetworks(true);
        delay(5000);
        int n = WiFi.scanComplete();
        if (n >= 0) {
            for (int i = 0; i < n; ++i) {
                addNetwork(WiFi.SSID(i), WiFi.BSSIDstr(i), WiFi.RSSI(i), WiFi.encryptionType(i), WiFi.channel(i));
            }
            WiFi.scanDelete();
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
    } else {
        Serial.println("[SLAVE] No new networks to send");
        currentNetworkIndex = 0;
        networkCount = 0;
    }
    blinkLED();
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

void blinkLED() {
    led = CRGB::White;
    FastLED.show();
    delay(100);
    led = CRGB::Black;
    FastLED.show();
}
