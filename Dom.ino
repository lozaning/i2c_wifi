#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <TinyGPS++.h>

const int numberOfSubUnits = 14;
const int subUnitAddresses[numberOfSubUnits] = {/* your sub-unit I2C addresses */};
const int chipSelect = /* your SD card CS pin */;
TinyGPSPlus gps;
HardwareSerial SerialGPS(1); // Assuming GPS is on Serial1

void setup() {
  Serial.begin(115200);
  SerialGPS.begin(9600, SERIAL_8N1, /* RX pin */, /* TX pin */); // Set these pins according to your GPS wiring
  Wire.begin(); // Start I2C as master

  if (!SD.begin(chipSelect)) {
    Serial.println("SD Card initialization failed!");
    return;
  }

  Serial.println("SD Card initialized.");
}

void loop() {
  while (SerialGPS.available() > 0) {
    gps.encode(SerialGPS.read());
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
    writeToSDCard(receivedData);
  }
}

void writeToSDCard(String data) {
  File dataFile = SD.open("WiFiData.csv", FILE_WRITE);

  if (dataFile) {
    String csvLine;
    String gpsData = getGPSData();

    while (data.length() > 0) {
      int newlineIndex = data.indexOf('\n');
      if (newlineIndex == -1) {
        csvLine = data;
        data = "";
      } else {
        csvLine = data.substring(0, newlineIndex);
        data.remove(0, newlineIndex + 1);
      }

      dataFile.println(csvLine + "," + gpsData);
    }
    dataFile.close();
  } else {
    Serial.println("Error opening SD card file.");
  }
}

String getGPSData() {
  String gpsInfo = "";
  if (gps.location.isValid()) {
    gpsInfo += String(gps.location.lat(), 6);
    gpsInfo += ",";
    gpsInfo += String(gps.location.lng(), 6);
    gpsInfo += ",";
    gpsInfo += String(gps.altitude.meters());
    gpsInfo += ",";
    gpsInfo += String(gps.date.value()); // Or format date as needed
    gpsInfo += ",";
    gpsInfo += String(gps.time.value()); // Or format time as needed
  } else {
    gpsInfo = "0,0,0,0,0"; // No GPS data available
  }
  return gpsInfo;
}
