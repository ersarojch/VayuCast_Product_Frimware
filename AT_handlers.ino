String sendAT(const char *cmd, uint32_t timeout) {
  if (apModeActive) return "";

  flushModemInput();

  String resp;
  resp.reserve(512);

  LOGF(">> %s", cmd);
  modem.print(cmd);
  modem.print("\r\n");

  uint32_t start = millis();

  while (millis() - start < timeout) {
    while (modem.available()) {
      char c = modem.read();
      resp += c;

      if (resp.indexOf("DOWNLOAD") != -1 ||
          resp.indexOf("\r\nOK") != -1 ||
          resp.indexOf("\nOK") != -1 ||
          resp.indexOf("ERROR") != -1) {
        return resp;
      }

      if (resp.length() > 1500) {
        resp.remove(0, resp.length() - 512);
      }
    }

    yield();
    delay(2);
  }

  return resp;
}
bool isSimPresent() {
  String resp = "";
  modem.println("AT+CPIN?");
  unsigned long start = millis();

  while (millis() - start < 10000) {
    if (modem.available()) {
      resp += modem.readString();
      //        LOG(resp);
    }
  }
  LOG(F("SIM RAW response:"));
  LOG(resp);
  if (resp.indexOf("+CPIN: READY") != -1) {
    return true;
  }
  if (resp.indexOf("SIM NOT INSERTED") != -1) {
    return false;
  }
  if (resp.indexOf("+CPIN:") != -1) {
    // SIM present but locked (PIN required)
    return true;
  }
  return false;
}


bool waitForModemBoot(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    //    blinkFastWhileWaiting();

    // Send AT periodically
    static uint32_t lastAT = 0;
    if (millis() - lastAT > 1000) {
      modem.println("AT");
      lastAT = millis();
    }

    // Read modem responses
    while (modem.available()) {
      String line = modem.readStringUntil('\n');
      line.trim();
      LOG(line);

      if (
        line.indexOf("ATREADY") >= 0 ||
        line.indexOf("OK") >= 0
      ) {
        LOG(F("GSM modem ready"));
        return true;
      }
    }
    yield();
    delay(10);
  }
  LOG(F("GSM modem boot timeout"));
  return false;
}
