String readHTTPBody(int totalLen) {
  const uint16_t CHUNK_SIZE = 512;
  uint32_t offset = 0;
  String fullResponse = "";

  while (offset < totalLen) {
    uint16_t chunk = min((uint32_t)CHUNK_SIZE, totalLen - offset);

    String cmd = "AT+HTTPREAD=" + String(offset) + "," + String(chunk);
    LOG(">" + cmd);
    modem.println(cmd);

    // Wait for +HTTPREAD: <len>
    String line;
    unsigned long start = millis();
    while (millis() - start < 10000) {
      if (modem.available()) {
        line = modem.readStringUntil('\n');
        line.trim();
        if (line.startsWith("+HTTPREAD:")) {
          break;
        }
      }
      yield();
      delay(1);
    }
    if (!line.startsWith("+HTTPREAD:")) {
      LOG("[ERROR] No +HTTPREAD header");
      return fullResponse;
    }

    // Read actual body chunk
    String bodyChunk = "";
    unsigned long dataStart = millis();

    while (millis() - dataStart < 5000)
    {
      while (modem.available())
      {
        char c = modem.read();
        bodyChunk += c;
        if (bodyChunk.length() >= chunk)
          break;
      }

      if (bodyChunk.length() >= chunk)
        break;
      yield();
      delay(2);
    }

    LOG("[BODY]");
    LOG(bodyChunk);
    fullResponse += bodyChunk;
    offset += chunk;
    yield();
    delay(30);
  }
  return fullResponse;
}

bool fetchHTTPResponse(const char* url) {
  LOG("\n===== START HTTP GET =====");

  int status = 0;
  int totalLen = 0;
  bool success = false;

  if (!httpInit(useSSL)) {
    LOG("[HTTP] GET init failed");
    sendAT("AT+HTTPTERM", 3000);
    return false;
  }

  httpSetURL(url);
  flushModemInput();

  modem.println("AT+HTTPACTION=0");

  if (!waitHTTPAction(status, totalLen, 60000)) {
    LOG("[HTTP] HTTPACTION failed");
    goto cleanup;
  }

  if (status != 200) {
    LOGF("[HTTP] Bad status: %d\n", status);
    goto cleanup;
  }

  LOGF("[HTTP] Total bytes: %d\n", totalLen);

  if (totalLen > 0) {
    String response = readHTTPBody(totalLen);
    success = parseJson(response, OTA_GSM);
  }

cleanup:
  sendAT("AT+HTTPTERM", 3000);
  delay(300);
  return success;
}



bool httpPost(const char* url,
              const char* body,
              const char* contentType,
              String& response,
              bool useSSL) {
  int status = 0;
  int dataLen = 0;
  bool success = false;
  response = "";

  if (!httpInit(useSSL)) {
    LOG("[HTTP] HTTP init failed");
    sendAT("AT+HTTPTERM", 3000);
    delay(300);
    return false;
  }

  do {
    // Set URL
    httpSetURL(url);

    // Set Content-Type
    String ctCmd = "AT+HTTPPARA=\"CONTENT\",\"" + String(contentType) + "\"";
    if (sendAT(ctCmd.c_str(), 5000).indexOf("OK") == -1) {
      LOG("[HTTP] Failed to set CONTENT type");
      break;
    }

    // Upload POST body
    int bodyLength = strlen(body);
    String dataCmd = "AT+HTTPDATA=" + String(bodyLength) + ",10000";

    String resp = sendAT(dataCmd.c_str(), 5000);
    if (resp.indexOf("DOWNLOAD") == -1) {
      LOG("[HTTP] HTTPDATA failed - no DOWNLOAD prompt");
      break;
    }

    LOG("[HTTP] Sending body...");
    modem.print(body);
    modem.flush();
    delay(300);

    while (modem.available()) modem.read();

    // Execute POST
    LOG(">> AT+HTTPACTION=1");
    modem.println("AT+HTTPACTION=1");

    if (!waitHTTPAction(status, dataLen, 60000)) {
      LOG("[HTTP] HTTPACTION failed");
      break;
    }

    LOGF("[HTTP] Status = %d, Response length = %d\n", status, dataLen);

    if (status != 200 && status != 201) {
      LOGF("[HTTP] Bad status: %d\n", status);
      break;
    }

    if (dataLen > 0) {
      response = readHTTPBody(dataLen);
    } else {
      LOG("[HTTP] No response body");
    }

    LOG("[HTTP] POST finished successfully");
    success = true;

  } while (false);

  // Always cleanup HTTP engine, success or failure
  sendAT("AT+HTTPTERM", 3000);
  delay(300);

  return success;
}
