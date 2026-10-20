long baudRates[] = { MODEM_BAUD,
                     300,
                     600,
                     1200,
                     2400,
                     4800,
                     9600,
                     14400,
                     19200,
                     28800,
                     38400,
                     57600,
                     76800,
                     115200,
                     230400,
                     250000,
                     460800,
                     500000,
                     921600,
                     1000000,
                     2000000
                   };
int numBauds = sizeof(baudRates) / sizeof(baudRates[0]);
void flushModemInput() {
  unsigned long start = millis();

  while (modem.available() && millis() - start < 200) {
    modem.read();
    delay(1);
  }
}
String readResponse(unsigned long timeoutMs) {
  String response;
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    while (modem.available()) {
      response += (char)modem.read();
    }
    if (response.indexOf("OK") != -1 || response.indexOf("ERROR") != -1) break;
    delay(10);
  }
  return response;
}
bool sendATExpectOK(const String &cmd, unsigned long timeoutMs = 1000) {
  flushModemInput();
  modem.print(cmd);
  modem.print("\r\n");
  String r = readResponse(timeoutMs);
  return (r.indexOf("OK") != -1);
}
bool probeBaud(long baud) {
  modem.begin(baud, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(120);
  return sendATExpectOK("AT", 800);
}
long detectBaudRate() {
  for (int i = 0; i < numBauds; i++) {
    long b = baudRates[i];
    if (probeBaud(b)) return b;
  }
  return 0;
}
bool setModemBaudPersistent(long currentBaud, long newBaud) {
  modem.begin(currentBaud, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(100);

  bool ok = false;

  // Try AT+IPREX=<baud>  (original)
  {
    String cmd = "AT+IPREX=" + String(newBaud);
    ok = sendATExpectOK(cmd, 2000);
  }

  // Fallback: AT+IPR=<baud> (common on many GSM modules)
  if (!ok) {
    String cmd = "AT+IPR=" + String(newBaud);
    ok = sendATExpectOK(cmd, 2000);
  }

  // If it replied OK, switch local UART to new baud and verify
  if (ok) {
    delay(150);
    modem.begin(newBaud, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(120);
    if (sendATExpectOK("AT", 1000)) return true;
  }

  return false;
}
