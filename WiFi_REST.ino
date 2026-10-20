bool connectWiFi(uint32_t timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  LOG("Connecting to WiFi");

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start >= timeoutMs) {
      LOG("\nWiFi connection timeout");
      return false;
    }
    yield();
    delay(500);
    LOGG(".");
  }

  LOG("\nWiFi connected");
  LOG("IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

void stopWiFi()
{
  // Close any open connections
  WiFi.disconnect(true);   // true = erase AP config
  delay(100);
  WiFi.mode(WIFI_OFF);     // turn off WiFi hardware
  delay(100);
  WiFi.mode(WIFI_OFF);// put WiFi into sleep
  delay(100);
  LOG("WiFi stopped");

}


bool checkFirmwareOverWiFi() {
  bool responseStatus = false;
  if (WiFi.status() == WL_CONNECTED) {
    LOG(checkFirmwareURL);
    WiFiClient client;          // plain HTTP client
    HTTPClient http;
    http.begin(client, checkFirmwareURL);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = -1;
    String response = "";
    for (int attempt = 1; attempt <= 3; attempt++) {
      httpResponseCode = http.GET();
      LOG("Attempt " + String(attempt) + " - Firmware check GET response: " + String(httpResponseCode));
      response = http.getString();
      if (httpResponseCode == 200) {
        LOG("Success");
        parseJson(response, OTA_WIFI);
        responseStatus = true;
        break;
      } else {
        responseStatus = false;
        LOG("Error sending data. Response code: " + String(httpResponseCode));
      }
      yield();
      delay(500);
    }
    LOG(response);
    http.end();

  }
  else {
    responseStatus = false;
    LOG("WiFi Not Connected While HTTP");
  }
  return responseStatus;
}

bool performOTA_Stream(String path) {
  if (WiFi.status() != WL_CONNECTED) {
    LOG("WiFi not connected");
    return false;
  }

  path.trim();
  String Url = MainDomain + path;
  Url.trim();
  LOG(Url);

  WiFiClient client;          // plain HTTP client
  HTTPClient http;
  http.setTimeout(60000);

  LOG("Connecting to firmware server...");
  if (!http.begin(client, Url)) {  // use client object
    LOG("HTTP begin failed");
    return false;
  }

  int httpCode = http.GET();
  LOG("HTTP code: " + String(httpCode));

  if (httpCode != 200) {
    LOG("Firmware download failed");
    http.end();
    return false;
  }

  int contentLength = http.getSize();
  LOG("Firmware size: " + String(contentLength));

  if (contentLength <= 0) {
    LOG("Invalid firmware size");
    http.end();
    return false;
  }

  Update.onProgress([](size_t progress, size_t total) {
    if (total == 0) return;

    static int last_percent = -1;
    int percent = (progress * 100) / total;
    if (percent != last_percent) {
      LOG("OTA Progress: " + String(percent) + "%");
      last_percent = percent;
      feedWatchDog();
    }
  });

  if (!Update.begin(contentLength)) {
    LOG("Not enough space for OTA");
    Update.printError(Serial);
    http.end();
    return false;
  }

  LOG("Starting firmware write...");
  WiFiClient *stream = http.getStreamPtr();
  size_t written = Update.writeStream(*stream);
  LOG("Written bytes: " + String(written));

  if (written != (size_t)contentLength) {
    LOG("Written size mismatch");
    http.end();
    return false;
  }

  if (!Update.end()) {
    LOG("Update.end() failed");
    Update.printError(Serial);
    http.end();
    return false;
  }

  if (!Update.isFinished()) {
    LOG("Update not finished");
    http.end();
    return false;
  }

  LOG("OTA successful! Rebooting...");
  http.end();
  return true;
}
