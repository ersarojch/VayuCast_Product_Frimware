bool sampelData() {
  LOG("===== Sampling Started =====");
  LOG("===== SDS Wakeup =====");
  sdsSerial.write(wakeCmd, 19);
  delay(100);
  // Clear only command response for 1 second
  uint32_t clrStart = millis();
  while (millis() - clrStart < 1000) {
    while (sdsSerial.available()) {  
      sdsSerial.read();
    }
    yield();
  }

  // Warmup SDS011 for 45s
  uint32_t warmupStart = millis();
  while (millis() - warmupStart < 45000) {
    float temp25, temp10;
    readSDS(temp25, temp10);  // drain continuously
    yield();
    delay(5);
  }

  lastSample = sampleSensors(10, 4000);  // store globally
  LOG("===== Sampling Done =====");
  hasSample = true;
  return true;
}
bool sendTask() {
  if (!hasSample) {
    LOG("No sample available!");
    return false;
  }
  float chiT = readChipTemperature();
  int grssi = readGsmRssi();
  float batV = readBatteryVoltage();

  char jsonPayload[256];
  buildJson(
    jsonPayload,
    sizeof(jsonPayload),
    deviceID.c_str(),
    lastSample.h,
    lastSample.t,
    lastSample.p25,
    lastSample.p10,
    chiT,
    grssi,
    batV
  );
  LOG(jsonPayload);
  String serverResponse;
  bool ok = httpPost(
              HTTP_URL_POST.c_str(),
              jsonPayload,
              "application/json",
              serverResponse
            );

  if (ok) {
    hasSample = false;
    sdsSerial.write(sleepCmd, 19);  // only sleep on success
    delay(100);
    return true;
  }
  return false;
}
