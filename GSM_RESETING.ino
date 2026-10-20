
bool requestHardPowerReset(const char *reason) {
  Serial.println("[GSM] Power reset");

  modem.end();

  // Prevent UART back-powering
  pinMode(MODEM_RX, INPUT);
  pinMode(MODEM_TX, INPUT);

  // Cut GSM power
  digitalWrite(GSM_POWER_PIN, HIGH);
  //  delay(5000);
  for (int i = 0; i <30 ; i++) {
    digitalWrite(HB_LED_PIN, !digitalRead(HB_LED_PIN));
    delay(500);
  }
  // Restore GSM power
  digitalWrite(GSM_POWER_PIN, LOW);
  delay(10000);

  modem.setRxBufferSize(4096);
  modem.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);

  if (!waitForModemBoot(20000)) {
    Serial.println("[GSM] Modem failed to boot");
    return false;
  }
  if (!setupGsmNetwork()) {
    Serial.println("[GSM] Network recovery failed");
    return false;
  }

  return true;
  feedWatchDog();
  sendAT("ATE0", 2000);
  return true;
}
