bool registrationOK(const String &response, const char *prefix) {
  int position = response.indexOf(prefix);

  if (position < 0) {
    return false;
  }

  int comma = response.indexOf(',', position);

  if (comma < 0) {
    return false;
  }

  int status = response.substring(comma + 1).toInt();

  return status == 1 || status == 5;
}

bool setupGsmNetwork(uint32_t timeoutMs) {
  LOG("[GSM] Configuring network");

  sendAT("ATE0", 3000);

  // Check SIM
  String response = sendAT("AT+CPIN?", 5000);

  if (response.indexOf("+CPIN: READY") < 0) {
    LOG("[GSM] SIM is not ready");
    return false;
  }

  // Detect SIM using IMSI
  String imsi = sendAT("AT+CIMI", 5000);
  String apn = "";

  if (imsi.indexOf("42901") >= 0) {
    apn = "ntnet";
    LOG("[GSM] NTC SIM detected");
  }
  else if (imsi.indexOf("42902") >= 0) {
    apn = "web";
    LOG("[GSM] Ncell SIM detected");
  }
  else {
    LOG("[GSM] Unknown SIM, keeping stored APN");
  }

  // Set APN only when it is missing or different
  if (!apn.isEmpty()) {
    response = sendAT("AT+CGDCONT?", 5000);

    String expectedApn = String("\"") + apn + "\"";

    if (response.indexOf(expectedApn) < 0) {
      String command =
        String("AT+CGDCONT=1,\"IP\",\"") + apn + "\"";

      response = sendAT(command.c_str(), 10000);

      if (response.indexOf("OK") < 0) {
        LOG("[GSM] APN configuration failed");
        return false;
      }

      LOG("[GSM] APN configured");
    }
    else {
      LOG("[GSM] APN already correct");
    }
  }

  // Automatic LTE/GSM selection
  sendAT("AT+CNMP=2", 10000);

  // Wait for network registration
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    response = sendAT("AT+CEREG?", 5000);

    if (registrationOK(response, "+CEREG:")) {
      LOG("[GSM] LTE network registered");
      break;
    }

    response = sendAT("AT+CGREG?", 5000);

    if (registrationOK(response, "+CGREG:")) {
      LOG("[GSM] GSM network registered");
      break;
    }

    feedWatchDog();
    delay(2000);
  }

  if (millis() - start >= timeoutMs) {
    LOG("[GSM] Network registration timeout");
    return false;
  }

  // Ensure packet service is attached
  response = sendAT("AT+CGATT?", 5000);

  if (response.indexOf("+CGATT: 1") < 0) {
    response = sendAT("AT+CGATT=1", 15000);

    if (response.indexOf("OK") < 0) {
      LOG("[GSM] Packet attachment failed");
      return false;
    }
  }

  LOG("[GSM] Network configuration completed");
  return true;
}
