
bool parseJson(const String& json, OtaMode Mode)
{
  StaticJsonDocument<256> doc;

  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    LOG("JSON parse failed: ");
    LOG(err.c_str());
    return false;
  }

  if (!doc.containsKey("version") || !doc.containsKey("bin")) {
    LOG("Firmware already up to date." + readRawVersion());

    return false;
  }

  const char* versionStr = doc["version"];
  const char* binPath    = doc["bin"];
  uint32_t fileSize      = doc["size"];


  LOG("Version: ");
  LOG(versionStr);
  LOG("Bin Path: ");
  LOG(binPath);
  LOGF("Size    : % ld", fileSize);


  LOG("Newer firmware detected. Starting OTA...");
  bool otaSuccess = false;

  switch (Mode) {
    case OTA_GSM:
      {
        String firmwareUrl = MainDomain + binPath;
        otaSuccess = performOTA(firmwareUrl.c_str());
      }
      break;
    case OTA_WIFI:
      otaSuccess = performOTA_Stream(binPath);
      break;
  }
  if (!otaSuccess) {
    LOG("OTA failed!");
    return false;
  }

  LOG("Saving new firmware version...");
  writePref(VERSION_KEY, versionStr);

  LOG("Rebooting...");
  delay(1000);
  ESP.restart();
  return true;
}

void checkOTAState() {

  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;

  if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {

    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
      Serial.println("[OTA] Firmware pending verification");
      otaPending = true;
    }
  }
}
