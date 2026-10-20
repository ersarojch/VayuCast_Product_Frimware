bool httpInit(bool useSSL) {
  sendAT("AT+HTTPTERM");
  delay(300);

  if (sendAT("AT+HTTPINIT").indexOf("OK") == -1) {
    LOG(F("[HTTP] INIT failed"));
    return false;
  }

  if (useSSL) {
    sendAT("AT+CSSLCFG=\"sslversion\",0,4");   // TLS 1.2
    sendAT("AT+CSSLCFG=\"authmode\",0,0");     // no cert check
    sendAT("AT+CSSLCFG=\"ignorelocaltime\",0,1");
    if (sendAT("AT+HTTPPARA=\"SSLCFG\",0").indexOf("OK") == -1) {
      LOG(F("[HTTP] SSLCFG bind failed"));
      return false;
    }
  }
  return true;
}
void httpSetURL(const char *url) {
  sendAT((String("AT+HTTPPARA=\"URL\",\"") + url + "\"").c_str());
}
bool waitHTTPAction(int &status, int &dataLen, uint32_t timeout = 15000) {
  String line = "";
  uint32_t start = millis();
  while (millis() - start < timeout) {
    while (modem.available()) {
      char c = modem.read();
      Serial.write(c);
      if (c == '\n') {          // end of line
        line.trim();
        if (line == "ERROR") {
          LOG(F("[HTTP] Modem ERROR before HTTPACTION"));
          return false;
        }
        if (line.startsWith("+HTTPACTION:")) {
          return httpParseAction(line, status, dataLen);
        }
        line = "";
      } else if (c != '\r') {
        line += c;
      }
    }

    yield();
    delay(10);
  }

  LOG(F("[HTTP] HTTPACTION timeout"));
  return false;
}

bool httpParseAction(const String &resp, int &status, int &dataLen) {
  int c1 = resp.indexOf(',');
  int c2 = resp.indexOf(',', c1 + 1);

  if (c1 == -1 || c2 == -1) {
    LOG(F("[HTTP] Malformed HTTPACTION"));
    return false;
  }
  status  = resp.substring(c1 + 1, c2).toInt();
  dataLen = resp.substring(c2 + 1).toInt();
  LOGF("[HTTP] Status: %d, DataLen: %d\n", status, dataLen);
  //Transport / SSL errors
  if (status >= 700) {
    LOG(F("[HTTP] Transport / SSL error"));
    return false;
  }
  if (status == 200 || status == 201) {
    LOG(F("[HTTP] Request SUCCESS"));
    return true;
  }
  LOG(F("[HTTP] HTTP error"));
  return false;
}

//const char* httpStatusText(int status) {
//  switch (status) {
//    case 200: return "OK";
//    case 201: return "Created";
//    case 400: return "Bad Request";
//    case 401: return "Unauthorized";
//    case 403: return "Forbidden";
//    case 404: return "Not Found";
//    case 408: return "Timeout";
//    case 500: return "Internal Server Error";
//    case 502: return "Bad Gateway";
//    case 503: return "Service Unavailable";
//    default:  return "Unknown Error";
//  }
//}
