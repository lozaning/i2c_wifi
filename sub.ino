#include <Wire.h>
#include <WiFi.h>
#include <vector>

const int i2cAddress = 0xXX; // Replace XX with a unique I2C address for this sub-unit
const int wifiChannel = 1;   // Set your WiFi channel here
std::vector<String> newNetworks;
bool newDataAvailable = false;

void setup() {
  Serial.begin(115200);
  Wire.begin(i2cAddress);
  Wire.onRequest(requestEvent);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  startWifiScan();
}

void loop() {
  if (WiFi.scanComplete() > 0) {
    storeNewNetworks();
    startWifiScan(); // Start a new scan
  }
  delay(10); // Small delay for stability
}

void startWifiScan() {
  WiFi.scanNetworks(true, false, wifiChannel); // Async scan on specified channel
}

void storeNewNetworks() {
  int n = WiFi.scanComplete();
  for (int i = 0; i < n; ++i) {
    String ssid = WiFi.SSID(i);
    String bssid = WiFi.BSSIDstr(i);
    String networkDetails = ssid + "," + bssid + "," + getCapabilities(i) + "," + 
                            WiFi.RSSI(i) + "," + WiFi.channel(i) + "," + millis();
    if (!isNetworkKnown(bssid)) {
      newNetworks.push_back(networkDetails);
      newDataAvailable = true;
    }
  }
  WiFi.scanDelete();
}

bool isNetworkKnown(const String& bssid) {
  for (const auto& network : newNetworks) {
    if (network.startsWith(bssid + ",")) {
      return true;
    }
  }
  return false;
}

String getCapabilities(int networkIndex) {
  return WiFi.encryptionType(networkIndex) == WIFI_AUTH_OPEN ? "Open" : "Secured";
}

void requestEvent() {
  if (newDataAvailable) {
    String dataToSend = "";
    for (const auto& network : newNetworks) {
      dataToSend += network + "\n"; // Each network's data is separated by a newline
    }
    Wire.write(dataToSend.c_str());
    newNetworks.clear();
    newDataAvailable = false;
  } else {
    Wire.write("No new data");
  }
}
