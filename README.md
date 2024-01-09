# WiFi Network Mapping with M5 Atom Devices

This repository contains the code for a project utilizing M5 Atom devices for scanning and logging WiFi network data. The system is composed of one Dominant (Dom) unit, an M5 Atom GPS unit, and multiple Subordinate (Sub) units, each being an M5 Atom Matrix unit. The Sub units continuously scan for WiFi networks and communicate their findings to the Dom unit, which logs the data with GPS coordinates onto an SD card in WiGLE CSV format.

## Repository Structure

- `DomESP32/`: Contains the code for the Dom M5 Atom GPS unit.
- `SubUnitESP32/`: Contains the code for the Sub M5 Atom Matrix units.
- `README.md`: Documentation detailing the project and setup instructions.

## Hardware Requirements

- M5 Atom GPS unit (for the Dom)
- Multiple M5 Atom Matrix units (for the Subs)
- SD Card module (for the Dom unit)
- Necessary cables and power supplies

## Software Requirements

- Arduino IDE
- ESP32 Board package for Arduino IDE
- Libraries: `M5Atom.h`, `SD.h`, `SPI.h`, `TinyGPS++.h`, `WiFi.h`

## Setup Instructions

1. **Hardware Setup**:
   - Attach the SD card module to the Dom M5 Atom GPS unit according to the SPI configuration specified in the code.
   - Ensure each Sub M5 Atom Matrix unit is set up for WiFi scanning and I2C communication.

2. **Software Setup**:
   - Install the Arduino IDE and add the ESP32 board package.
   - Install the required libraries mentioned above.

3. **Programming the M5 Atoms**:
   - Load the `DomESP32` code onto your Dom M5 Atom GPS unit.
   - Load the `SubUnitESP32` code onto each Sub M5 Atom Matrix unit.
   - Adjust the I2C addresses and other configuration parameters in the code to suit your specific setup.

4. **Operation**:
   - Power up the Dom and all Sub units.
   - The Sub units will begin scanning for WiFi networks and report the findings to the Dom.
   - The Dom unit will log all received data with GPS information to the SD card.

## Usage

- Upon start-up, the Dom M5 Atom GPS unit will create a new log file on the SD card.
- This log file is named using the date and time from the GPS module.
- WiFi network data from all Sub units is recorded in WiGLE CSV format.

## Contributing

If you're interested in contributing to this project, please fork the repository and submit a pull request with your proposed changes.

## License

This project is open source and available under the [MIT License](LICENSE).

## Acknowledgments

- Appreciation goes to the M5Stack community for their support and resources.
- A special thank you to everyone who has contributed to this project.

---

