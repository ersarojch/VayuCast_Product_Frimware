
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
//#include <EEPROM.h>
#include "esp_ota_ops.h"
#include <Preferences.h>

#define SDS_RX 33
#define SDS_TX 25
//HardwareSerial sdsSerial(2);
#define sdsSerial Serial1

const byte DNS_PORT = 53;
DNSServer dnsServer;
AsyncWebServer server(80);;
// Search for parameter in HTTP POST request
const char* PARAM_INPUT_1 = "deviceID";
const unsigned long AP_TIMEOUT = 2 * 60 * 1000;
unsigned long apStartTime = 0;
bool apModeActive = false;
//Variables to save values from HTML form
String deviceID;
bool restartPending = false;
unsigned long restartTime = 0;
// File paths to save input values permanently
const char* DEVICE_ID_KEY = "deviceID";


#define MODEM_RX 26
#define MODEM_TX 27
#define MODEM_BAUD 115200
#define modem Serial2
#define HTTP_FAIL_LIMIT 5
uint8_t httpFailCount = 0;

#define APN_NAME "ntnet"
// true  = Testing server
// false = Production server
#define USE_TEST_SERVER false

#if USE_TEST_SERVER
String MainDomain = "http://opensensormapx.dev.techcolab.org";
#else
String MainDomain = "http://opensensormap.dev.techcolab.org";
#endif

// ===== Paths =====
String postPath       = "/api/v1/sensors/push-data/";

String OTA_CheckPath = "/api/v1/displays/firmware/?version_check=";
// ===== Final URLs =====
String HTTP_URL_POST       = MainDomain + postPath;
String checkFirmwareURL    = MainDomain + OTA_CheckPath;

// WiFi if needed
const char* WIFI_SSID     = "abcde";
const char* WIFI_PASSWORD = "88888888";
unsigned long loopMillis = 0;
const unsigned long LOOP_INTERVAL = 1000 * 60 * 5;
//const unsigned long LOOP_INTERVAL = 1000 * 60;


bool taskRunning = false;
bool firmwareCheckPending = false;
unsigned long lastFirmwareCheck = 0;
const unsigned long FIRMWARE_CHECK_INTERVAL = 1000 * 60 * 60;

bool useSSL = false;

const unsigned long SYSTEM_RESET_INTERVAL =
  7UL * 24UL * 60UL * 60UL * 1000UL;  // 7 days

unsigned long lastSystemReset = 0;

bool requestHardPowerReset(const char *reason);

String sendAT(const char *cmd, uint32_t timeout = 3000);
bool httpInit(bool useSSL);
const char* httpStatusText(int status);
bool httpParseAction(const String &resp, int &status, int &dataLen);
bool waitHTTPAction(int &status, int &dataLen, uint32_t timeout);
bool waitForModemBoot(uint32_t timeoutMs = 20000);
bool setupGsmNetwork(uint32_t timeoutMs = 120000);

bool httpPost(const char* url,
              const char* body,
              const char* contentType,
              String& response,
              bool useSSL = false);


struct SensorData {
  float h;
  float t;
  float p25;
  float p10;
  uint32_t runtimeSec;
  bool valid_dht;
  bool valid_pm;

};
SensorData lastSample;
bool hasSample = false; // flag to know if we already sampled

bool httpSuscess = false;

enum OtaMode {
  OTA_WIFI,
  OTA_GSM
};

const char* VERSION_KEY   = "fw_version";

bool otaPending = false;
unsigned long otaStartTime;
const unsigned long OTA_VERIFY_TIMEOUT = 1000 * 60 * 10;

#define GSM_POWER_PIN 23
#define HB_LED_PIN 2
#define Status_LED_PIN 14
volatile bool otaInProgress = false;



void setup()
{
  // ==========================================================
  // 1. Request GSM OFF immediately
  //
  // EXISTING CIRCUIT:
  // HIGH = PC817 ON  -> Q3 OFF -> GSM OFF
  // LOW  = PC817 OFF -> Q3 ON  -> GSM ON
  // ==========================================================
  pinMode(GSM_POWER_PIN, OUTPUT);
  digitalWrite(GSM_POWER_PIN, HIGH);
  // Do not actively drive modem UART while requesting GSM OFF.
  // External level-shifter backfeed still requires a hardware fix.
  modem.end();
  pinMode(MODEM_TX, INPUT);
  pinMode(MODEM_RX, INPUT);
  pinMode(HB_LED_PIN, OUTPUT);
  digitalWrite(HB_LED_PIN, LOW);
  Serial.begin(115200);
  Serial.println();
  Serial.println("[BOOT 1] GSM OFF requested");
  // ==========================================================
  // 2. Start application watchdog
  // ==========================================================
  setupWatchDog();
  feedWatchDog();
  Serial.println("[BOOT 2] Initial settling interval");
  startupDelay(2000);
  // ==========================================================
  // 3. Read OTA state and local configuration
  // ==========================================================
  checkOTAState();
  if (otaPending) {
    otaStartTime = millis();
  }
  // Keep this flash write after the initial settling interval.
  //  trackBootCount();

  initPreferences();
  // ==========================================================
  // TESTING ONLY - manually write ID and firmware version
  // Comment these lines after testing
  // ==========================================================
//    writePref(DEVICE_ID_KEY, "d157c841-ebf1-4737-b969-70037a82e8a1");
//    writePref(VERSION_KEY, "4.0.0");

  deviceID = readPref(DEVICE_ID_KEY);
  deviceID.trim();
  feedWatchDog();
  // ==========================================================
  // 4. Initialize LED task and sensors
  // ==========================================================
  BaseType_t ledTaskResult = xTaskCreatePinnedToCore(
                               LED_indicator,
                               "LED_indicator",
                               2048,       // Additional stack margin; verify usage on hardware
                               NULL,
                               1,
                               NULL,
                               0
                             );

  if (ledTaskResult != pdPASS) {
    Serial.println("[BOOT] WARNING: LED task creation failed");
  }
  sdsSerial.begin(
    9600,
    SERIAL_8N1,
    SDS_RX,
    SDS_TX
  );
  if (!beginSensor()) {
    Serial.println(
      "[BOOT] WARNING: SHT initialization failed; "
      "existing sensor-recovery logic will need to retry"
    );
  }
  feedWatchDog();
  // ==========================================================
  // 5. Configuration mode when device ID is missing
  //
  // GSM is still requested OFF here.
  // ==========================================================
  if (deviceID.isEmpty()) {
    Serial.println("[BOOT] No device ID; starting configuration AP");
    apModeActive = true;
    apStartTime = millis();
    AP_Mode();
    feedWatchDog();
    return;
  }
  Serial.print("[BOOT] Device ID: ");
  Serial.println(deviceID);

  // Build from the base each time, rather than appending to
  // an already assembled URL.
  checkFirmwareURL = MainDomain + OTA_CheckPath;
  checkFirmwareURL += readRawVersion();
  checkFirmwareURL += "&uid=";
  checkFirmwareURL += deviceID;

  Serial.println(checkFirmwareURL);
  // ==========================================================
  // 6. Configure RX buffer BEFORE any modem.begin()
  // ==========================================================
  if (modem.setRxBufferSize(4096) == 0) {
    Serial.println("[BOOT] WARNING: Could not set modem RX buffer");
  }
  // ==========================================================
  // 7. Turn GSM ON
  // ==========================================================
  Serial.println("[BOOT 3] GSM power ON");
  digitalWrite(GSM_POWER_PIN, LOW);
  const uint32_t gsmPowerOnAt = millis();
  // ==========================================================
  // 8. Perform Wi-Fi work while GSM boots
  //
  // Keep your existing Wi-Fi startup/checking function here.
  // Add the OTA pending-verification guard described below.
  // ==========================================================
  Serial.println("[BOOT 4] Wi-Fi check while GSM boots");
  feedWatchDog();
  if (connectWiFi(10000)) {
    feedWatchDog();
    checkFirmwareOverWiFi();
    feedWatchDog();
  }
  stopWiFi();
  feedWatchDog();

  // ==========================================================
  // 9. Ensure a minimum elapsed GSM startup interval
  //
  // 5000 ms is a starting value for YOUR startup sequence,
  // not a guarantee that every modem is ready in five seconds.
  // UART/AT readiness is checked below.
  // =========================================================
  const uint32_t minimumGsmBootMs = 5000;
  const uint32_t elapsedMs = millis() - gsmPowerOnAt;
  if (elapsedMs < minimumGsmBootMs) {
    startupDelay(minimumGsmBootMs - elapsedMs);
  }
  // ==========================================================
  // 10. Detect modem baud, with one additional attempt
  // ==========================================================
  Serial.println("[BOOT 5] Detecting modem baud");
  long detected = 0;
  for (uint8_t attempt = 0; attempt < 2; ++attempt) {
    feedWatchDog();
    detected = detectBaudRate();
    feedWatchDog();
    if (detected > 0) {
      break;
    }
    if (attempt == 0) {
      Serial.println("[GSM] No baud response; retrying shortly");
      startupDelay(2000);
    }
  }
  if (detected <= 0) {
    recoverStartupFailure("Could not detect modem baud");
    return;  // Never continue with detected == 0
  }
  Serial.printf("[GSM] Detected baud: %ld\n", detected);
  // ==========================================================
  // 11. Change baud only when necessary
  //
  // Your existing setter verifies AT communication at the
  // new baud before returning true. No ESP restart is needed
  // merely because that operation succeeded.
  // ==========================================================
  if (detected != MODEM_BAUD) {
    Serial.printf(
      "[GSM] Requesting target baud: %ld\n",
      (long)MODEM_BAUD
    );
    feedWatchDog();
    const bool changed =
      setModemBaudPersistent(detected, MODEM_BAUD);
    feedWatchDog();
    if (changed) {
      detected = MODEM_BAUD;
      Serial.println("[GSM] Target baud verified");
    } else {
      // The modem may still be at the old baud or may already
      // have changed. Re-detect instead of assuming either.
      Serial.println("[GSM] Baud change not verified; re-detecting");
      detected = detectBaudRate();
      feedWatchDog();
      if (detected <= 0) {
        recoverStartupFailure("UART lost after baud change");
        return;
      }
      Serial.printf(
        "[GSM] Continuing at responsive baud: %ld\n",
        detected
      );
    }
  }
  modem.begin(
    detected,
    SERIAL_8N1,
    MODEM_RX,
    MODEM_TX
  );
  startupDelay(100);
  // ==========================================================
  // 12. Confirm AT communication
  // ==========================================================
  Serial.println("[BOOT 6] Checking modem AT readiness");
  feedWatchDog();
  const bool modemReady = waitForModemBoot(20000);
  feedWatchDog();
  if (!modemReady) {
    recoverStartupFailure("Modem AT readiness failed");
    return;
  }
  // ==========================================================
  // 13. Check SIM BEFORE network setup
  //
  // Keep your existing SIM-check behavior, with retries to
  // avoid treating one early missing response as a missing SIM.
  // ==========================================================
  Serial.println("[BOOT 7] Checking SIM");
  bool simDetected = false;
  for (uint8_t attempt = 0; attempt < 3; ++attempt) {
    feedWatchDog();
    simDetected = isSimPresent();
    feedWatchDog();
    if (simDetected) {
      break;
    }
    if (attempt < 2) {
      startupDelay(1000);
    }
  }
  if (!simDetected) {
    Serial.println("[GSM] SIM not detected; starting configuration AP");
    apModeActive = true;
    apStartTime = millis();
    AP_Mode();
    feedWatchDog();
    return;
  }
  // ==========================================================
  // 14. Configure APN and register on network
  //
  // Your setupGsmNetwork() already sends AT+CNMP=2.
  // Do not send it again immediately afterward.
  // ==========================================================

  Serial.println("[BOOT 8] Configuring GSM network");
  feedWatchDog();
  const bool networkReady = setupGsmNetwork();
  feedWatchDog();
  if (!networkReady) {
    recoverStartupFailure("GSM network setup failed");
    return;
  }
  Serial.println("[GSM] Network ready");
  // ==========================================================
  // 15. First sample and data-upload attempt
  // ==========================================================
  Serial.println("[BOOT 9] Sampling");
  taskRunning = true;
  feedWatchDog();
  const bool sampled = sampelData();
  feedWatchDog();  // Important: separate sampling from HTTP timing
  bool sent = false;
  if (sampled && hasSample) {
    Serial.println("[BOOT 10] Sending first sample");
    sent = sendTask();
    feedWatchDog();
  }
  taskRunning = false;
  httpSuscess = sent;
  // Schedule the next normal attempt from this point.
  loopMillis = millis();
  if (sent) {
    httpFailCount = 0;
    Serial.println("[BOOT] First upload successful");

    // Preserve your current policy:
    // successful data upload is the OTA health confirmation.
    if (otaPending) {
      const esp_err_t result =
        esp_ota_mark_app_valid_cancel_rollback();

      if (result == ESP_OK) {
        otaPending = false;
        Serial.println("[OTA] Running firmware confirmed");
      } else {
        Serial.printf(
          "[OTA] Confirmation failed: %d\n",
          (int)result
        );
      }
    }
  } else {
    Serial.println(
      "[BOOT] First upload failed; normal loop will retry"
    );
  }
  // ==========================================================
  // 16. GSM firmware check
  //
  // Do not start another OTA while the running image is
  // still awaiting validation.
  // ==========================================================
  if (!otaPending) {
    Serial.println("[BOOT 11] GSM firmware check");
    taskRunning = true;
    feedWatchDog();
    fetchHTTPResponse(checkFirmwareURL.c_str());
    feedWatchDog();
    taskRunning = false;
  }
  lastFirmwareCheck = millis();
  lastSystemReset = millis();
  Serial.println("[BOOT 12] Setup finished");
  feedWatchDog();
}

void loop() {
  unsigned long now = millis();
  if (apModeActive) {
    dnsServer.processNextRequest();
    yield();
  }
  // Data sending timer
  if (now - loopMillis >= LOOP_INTERVAL && !apModeActive) {
    loopMillis = now;
    bool success = false;
    // Sample once per interval
    if (!hasSample) {
      sampelData();
    }

    for (int attp = 1; attp <= HTTP_FAIL_LIMIT; attp++) {
      LOGF("[HTTP] Attempt % d / % d\n", attp, HTTP_FAIL_LIMIT);
      taskRunning = true;
      success = sendTask();
      feedWatchDog();
      if (success) {
        httpFailCount = 0;
        httpSuscess = true;

        if (otaPending) {
          esp_ota_mark_app_valid_cancel_rollback();
          otaPending = false;
          Serial.println("[OTA] Firmware confirmed healthy");
        }
        break;
      } else {
        httpFailCount++;
        httpSuscess = false;
        LOGF("[HTTP] Fail count: % d / % d\n",
             httpFailCount, HTTP_FAIL_LIMIT);
      }
      delay(100);
      yield();
    }
    taskRunning = false;
    if (firmwareCheckPending) {
      firmwareCheckPending = false;
      lastFirmwareCheck = now;
      LOG("[OTA] Running postponed firmware check...");
      taskRunning = true;
      fetchHTTPResponse(checkFirmwareURL.c_str());
      taskRunning = false;
    }


    if (!success && httpFailCount >= HTTP_FAIL_LIMIT) {
      LOG(F("[SYSTEM] Too many HTTP failures, requesting hard power reset"));
      httpFailCount = 0;
      requestHardPowerReset("HTTP retry limit reached");
    }
  }

  // Firmware check timer
  if (!apModeActive && now - lastFirmwareCheck >= FIRMWARE_CHECK_INTERVAL) {
    if (!taskRunning) {
      taskRunning = true;
      lastFirmwareCheck = now;
      LOG("[OTA] Checking firmware...");
      fetchHTTPResponse(checkFirmwareURL.c_str());
      feedWatchDog();
      taskRunning = false;
    } else {
      // GSM task running, defer the firmware check
      firmwareCheckPending = true;
      LOG("[OTA] GSM task running, deferring firmware check");
    }
  }

  if (restartPending && millis() - restartTime > 3000) {
    ESP.restart();
  }

  if (!apModeActive &&
      !otaInProgress &&
      !taskRunning &&
      millis() - lastSystemReset >= SYSTEM_RESET_INTERVAL) {

    LOG("[SYSTEM] Scheduled seven-day maintenance restart");

    lastSystemReset = millis();

    requestHardPowerReset("Scheduled maintenance");
    feedWatchDog();
    delay(1000);

    ESP.restart();
  }

  // AP mode timeout check
  if (apModeActive &&
      millis() - apStartTime > AP_TIMEOUT) {
    LOG("AP timeout; restarting initialization");
    dnsServer.stop();
    server.end();
    WiFi.softAPdisconnect(true);
    feedWatchDog();
    delay(100);
    ESP.restart();
    return;
  }
  if (otaPending && millis() - otaStartTime > OTA_VERIFY_TIMEOUT) {
    Serial.println("[OTA] Verification timeout, forcing rollback");
    esp_ota_mark_app_invalid_rollback_and_reboot();
  }
  feedWatchDog();
  delay(10);  // Allow IDLE1 to run
}
