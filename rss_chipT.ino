float readChipTemperature() {
  float chipTemp = temperatureRead();

  if (isnan(chipTemp) || chipTemp < -40.0 || chipTemp > 125.0) {
    LOG("Invalid ESP32 chip temperature");
    return NAN;
  }

  LOGF("ESP32 chip temperature: %.2f C", chipTemp);
  return chipTemp;
}
int readGsmRssi() {
  String response = sendAT("AT+CSQ", 3000);

  int csq = -1;

  int pos = response.indexOf("+CSQ:");
  if (pos == -1) {
    LOG("CSQ response not found");
    return -999;
  }

  pos += 5;  // Move after "+CSQ:"

  int commaPos = response.indexOf(',', pos);
  if (commaPos == -1) {
    LOG("Invalid CSQ response");
    return -999;
  }

  String csqString = response.substring(pos, commaPos);
  csqString.trim();

  csq = csqString.toInt();

  // 99 means signal is unknown or unavailable
  if (csq == 99) {
    LOG("GSM signal strength unknown");
    return -999;
  }

  if (csq < 0 || csq > 31) {
    LOGF("Invalid CSQ value: %d", csq);
    return -999;
  }

  int rssiDbm = -113 + (2 * csq);

  LOGF("GSM CSQ: %d, RSSI: %d dBm", csq, rssiDbm);

  return rssiDbm;
}
