#include <Update.h>

// ---------- OTA policy ----------
static const uint16_t OTA_CHUNK_SIZE = 1024;
static const uint8_t  OTA_CHUNK_RETRIES = 8;
static const uint32_t OTA_READ_TIMEOUT_MS = 12000;
static const uint32_t OTA_HEADER_TIMEOUT_MS = 8000;
static const uint32_t OTA_TAIL_TIMEOUT_MS = 1200;


// --------------------------------------------------

static bool readLineFromModem(String &line, uint32_t timeoutMs) {
  line = "";
  uint32_t start = millis();

  while (millis() - start < timeoutMs) {
    while (modem.available()) {
      char c = modem.read();

      if (c == '\r') continue;

      if (c == '\n') {
        line.trim();
        if (line.length() == 0) {
          line = "";
          continue;
        }
        return true;
      }

      line += c;

      if (line.length() > 160) {
        line = "";
      }
    }

    delay(1);
    yield();
  }

  return false;
}

static int parseHTTPReadLen(const String &line) {
  int colon = line.indexOf(':');
  if (colon < 0) return -1;

  String rest = line.substring(colon + 1);
  rest.trim();

  int comma = rest.lastIndexOf(',');
  if (comma >= 0) {
    rest = rest.substring(comma + 1);
    rest.trim();
  }

  return rest.toInt();
}

static bool waitForHTTPReadHeader(uint16_t requestedLen, int &advertisedLen) {
  advertisedLen = -1;

  String line;
  uint32_t start = millis();

  while (millis() - start < OTA_HEADER_TIMEOUT_MS) {
    if (!readLineFromModem(line, 700)) continue;

    LOGF("[HTTPREAD-H] %s", line.c_str());

    if (line == "OK") {
      continue;
    }

    if (line == "ERROR") {
      LOG("[HTTPREAD] ERROR before header");
      return false;
    }

    if (line.startsWith("+HTTPREAD:")) {
      advertisedLen = parseHTTPReadLen(line);

      if (advertisedLen == 0) {
        LOG("[HTTPREAD] Got +HTTPREAD: 0 before payload");
        return false;
      }

      if (advertisedLen < 0 || advertisedLen > requestedLen) {
        LOGF("[HTTPREAD] Bad advertised len %d req %u", advertisedLen, requestedLen);
        return false;
      }

      return true;
    }
  }

  LOG("[HTTPREAD] Header timeout");
  return false;
}

static bool waitHTTPReadTail() {
  String line;
  uint32_t start = millis();

  while (millis() - start < 1200) {
    if (!readLineFromModem(line, 300)) continue;

    LOGF("[HTTPREAD-T] %s", line.c_str());

    if (line == "OK") {
      return true;
    }

    if (line == "+HTTPREAD: 0") {
      return true;
    }

    if (line == "ERROR") {
      return false;
    }
  }

  // Do not block 5 seconds per chunk.
  return true;
}

static bool validateFirstChunk(const uint8_t *buf, size_t len) {
  if (len < 24) {
    LOG("[OTA] First chunk too short");
    return false;
  }

  if (buf[0] != 0xE9) {
    LOGF("[OTA] Invalid ESP image magic byte: 0x%02X", buf[0]);
    return false;
  }

  return true;
}

static bool terminateHTTP() {
  String resp = sendAT("AT+HTTPTERM", 2500);
  return resp.indexOf("OK") >= 0 || resp.indexOf("ERROR") >= 0;
}

static bool startHTTPDownloadSession(const char *url, int &status, int &totalLen) {
  status = 0;
  totalLen = 0;

  terminateHTTP();
  delay(500);
  flushModemInput();

  if (!httpInit(false)) {
    LOG("[OTA] httpInit failed");
    return false;
  }

  httpSetURL(url);

  if (sendAT("AT+HTTPPARA=\"READMODE\",1", 3000).indexOf("OK") < 0) {
    LOG("[OTA] READMODE failed");
    return false;
  }

  flushModemInput();
  modem.print("AT+HTTPACTION=0\r\n");

  if (!waitHTTPAction(status, totalLen, 60000)) {
    LOG("[OTA] HTTPACTION failed");
    return false;
  }

  if (status != 200) {
    LOGF("[OTA] HTTP status bad: %d", status);
    return false;
  }

  if (totalLen <= 0) {
    LOG("[OTA] Invalid total length");
    return false;
  }

  return true;
}

static int readHTTPChunk(uint32_t offset, uint16_t requestLen, uint8_t *buf) {
  flushModemInput();
  delay(30);

  modem.printf("AT+HTTPREAD=%lu,%u\r\n",
               (unsigned long)offset,
               requestLen);

  int advertisedLen = -1;

  if (!waitForHTTPReadHeader(requestLen, advertisedLen)) {
    return -1;
  }

  int received = 0;
  uint32_t lastByte = millis();

  while (received < advertisedLen) {
    while (modem.available() && received < advertisedLen) {
      buf[received++] = (uint8_t)modem.read();
      lastByte = millis();
    }

    if (millis() - lastByte > OTA_READ_TIMEOUT_MS) {
      LOGF("[HTTPREAD] Payload timeout %d/%d", received, advertisedLen);
      return -1;
    }

    delay(1);
    yield();
  }

  if (!waitHTTPReadTail()) {
    LOG("[HTTPREAD] Bad tail");
    return -1;
  }

  return received;
}

bool performOTA(const char *url) {
  otaInProgress = true;
  LOG("\n===== START OTA =====");

  int status = 0;
  int totalLen = 0;

  if (!startHTTPDownloadSession(url, status, totalLen)) {
    terminateHTTP();
    return false;
  }

  LOGF("[OTA] Firmware Size: %d bytes", totalLen);

  if (!Update.begin(totalLen, U_FLASH)) {
    LOGF("[OTA] Update.begin failed: %s", Update.errorString());
    terminateHTTP();
    return false;
  }

  uint8_t buf[OTA_CHUNK_SIZE];

  uint32_t offset = 0;
  bool firstChunk = true;
  uint8_t sessionRecoveries = 0;

  LOG("[OTA] Updating firmware...");

  while (offset < (uint32_t)totalLen) {
    uint16_t requestLen = min((uint32_t)OTA_CHUNK_SIZE,
                              (uint32_t)(totalLen - offset));

    bool success = false;
    int received = -1;

    for (uint8_t retry = 0; retry < OTA_CHUNK_RETRIES; retry++) {
      received = readHTTPChunk(offset, requestLen, buf);

      if (received == requestLen) {
        success = true;
        break;
      }

      LOGF("[OTA] Retry offset %lu attempt %u got %d/%u",
           (unsigned long)offset,
           retry + 1,
           received,
           requestLen);

      flushModemInput();
      delay(800);

      // If the modem keeps returning ERROR, restart HTTP session.
      if ((retry == 2 || retry == 5) && sessionRecoveries < 5) {
        LOG("[OTA] Recovering HTTP session...");
        sessionRecoveries++;

        int newStatus = 0;
        int newTotalLen = 0;

        if (!startHTTPDownloadSession(url, newStatus, newTotalLen)) {
          LOG("[OTA] HTTP recovery failed");
          delay(1000);
          continue;
        }

        if (newTotalLen != totalLen) {
          LOGF("[OTA] Size changed after recovery: old=%d new=%d",
               totalLen,
               newTotalLen);
          Update.abort();
          terminateHTTP();
          return false;
        }

        LOG("[OTA] HTTP session recovered");
        delay(500);
      }
    }

    if (!success) {
      LOGF("[OTA] Failed to read chunk at offset %lu",
           (unsigned long)offset);
      Update.abort();
      terminateHTTP();
      otaInProgress = false;
      return false;
    }

    if (firstChunk) {
      firstChunk = false;

      if (!validateFirstChunk(buf, received)) {
        LOG("[OTA] Not a valid ESP32 app binary");
        Update.abort();
        terminateHTTP();
        return false;
      }
    }

    size_t written = Update.write(buf, received);

    if (written != (size_t)received) {
      LOGF("[OTA] Flash write failed: wrote %u of %d",
           (unsigned)written,
           received);
      LOGF("[OTA] Update error: %s", Update.errorString());
      Update.abort();
      terminateHTTP();
      return false;
    }

    offset += written;
    feedWatchDog();
    LOGF("[OTA] Progress %lu/%d",
         (unsigned long)offset,
         totalLen);

    delay(2);
    yield();
  }

  if (offset != (uint32_t)totalLen) {
    LOGF("[OTA] Size mismatch: wrote=%lu expected=%d",
         (unsigned long)offset,
         totalLen);
    Update.abort();
    terminateHTTP();
    return false;
  }

  if (!Update.end(true)) {
    LOGF("[OTA] Update.end failed: %s", Update.errorString());
    terminateHTTP();
    return false;
  }

  if (!Update.isFinished()) {
    LOG("[OTA] Update not finished");
    terminateHTTP();
    return false;
  }

  if (Update.hasError()) {
    LOGF("[OTA] Update error: %s", Update.errorString());
    terminateHTTP();
    return false;
  }

  LOG("[OTA] SUCCESS! New image written and validated.");
  terminateHTTP();
  otaInProgress = false;
  return true;
}
