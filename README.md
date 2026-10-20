# RAIN_PIN_SHT_SDS_WD_3

## Development Environment

- **Microcontroller:** ESP32
- **Framework:** Arduino
- **ESP32 Arduino Core:** 3.3.4
- **IDE:** Arduino IDE

## Required Libraries

### ESP32 Arduino Core Libraries

The following libraries are included with the ESP32 Arduino Core and do not need to be installed separately.

- `WiFi.h`
- `HTTPClient.h`
- `WiFiClientSecure.h`
- `Update.h`
- `DNSServer.h`
- `esp_ota_ops.h`
- `Preferences.h`

### External Libraries

| Library | Version | Purpose |
|---|---|---|
| ArduinoJson | 7.4.2 | JSON data parsing and generation |
| ESPAsyncWebServer | 3.6.0 | Asynchronous web server |

Install the external libraries using Arduino IDE Library Manager or their official installation instructions. Install any additional dependencies required by ESPAsyncWebServer.

## Installation

1. Install Arduino IDE.
2. Install ESP32 by Espressif Systems version 3.3.4 through Boards Manager.
3. Install ArduinoJson version 7.4.2.
4. Install ESPAsyncWebServer version 3.6.0 and its required dependencies.
5. Open the firmware `.ino` file.
6. Select the appropriate ESP32 board and serial port.
7. Compile and upload the firmware.

## Notes

- The listed versions document the intended development environment.
- Verify that the firmware compiles successfully with these versions.
