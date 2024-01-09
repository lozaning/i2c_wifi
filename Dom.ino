#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <TinyGPS++.h>

// Constants
const int numberOfSubUnits = 14;
const int subUnitAddresses[numberOfSubUnits] = {/* your sub-unit I2C addresses */};
const int chipSelect = /* your SD card CS pin */;

// GPS and File System
TinyGPSPlus gps;
char fileName[50];

void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");

  // Initialize SD Card
  SPI.begin(23, 33, 19, -1); // Adjust pin numbers as per your hardware setup
  while (!SD.begin(-1, SPI, 40000000)) {
    Serial.println("SD Card initialization failed! Retrying...");
    delay(500);
  }
  Serial.println("SD Card initialized.");

  // Initialize GPS
  Serial1.begin(9600, SERIAL_8N1, 22, -1); // Adjust pin numbers as per your GPS module
  Serial.println("GPS Serial initialized.");

  waitForGPSFix();
  initializeFile();
}

void loop() {
  while (Serial1.available() > 0) {
    gps.encode(Serial1.read());
  }

  for (int i = 0; i < numberOfSubUnits; i++) {
    requestWiFiDataFromSubUnit(subUnitAddresses[i]);
    delay(100); // Delay to allow time for receiving data
  }

  delay(5000); // Delay before starting the next round of requests
}

void requestWiFiDataFromSubUnit(int address) {
  Wire.requestFrom(address, 512); // Request data from sub-unit, adjust size as needed

  String receivedData = "";
  while (Wire.available()) {
    char c = Wire.read();
    receivedData += c;
  }

  if (!receivedData.equals("No new data")) {
    Serial.println("Data from Sub-unit " + String(address, HEX) + ":");
    Serial.println(receivedData);
    logData(receivedData);
  }
}

void waitForGPSFix() {
  Serial.println("Waiting for GPS fix...");
  while (!gps.location.isValid()) {
    if (Serial1.available() > 0) {
      gps.encode(Serial1.read());
    }
    delay(250);
  }
  Serial.println("GPS fix obtained.");
}

void initializeFile() {
  snprintf(fileName, sizeof(fileName), "/WiFiData-%04d%02d%02d-%02d%02d%02d.csv",
           gps.date.year(), gps.date.month(), gps.date.day(),
           gps.time.hour(), gps.time.minute(), gps.time.second());

  File dataFile = SD.open(fileName, FILE_WRITE);
  if (dataFile) {
    dataFile.println("MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,Latitude,Longitude,AltitudeMeters,AccuracyMeters,Type");
    dataFile.close();
    Serial.println("New file created: " + String(fileName));
  } else {
    Serial.println("Error creating new file: " + String(fileName));
  }
}

void logData(String data) {
  File dataFile = SD.open(fileName, FILE_APPEND);
  if (dataFile) {
    dataFile.println(data);
    dataFile.close();
  } else {
    Serial.println("Error writing to file: " + String(fileName));
  }
}
