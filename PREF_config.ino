void initPreferences() {
  Preferences testPrefs;
  if (!testPrefs.begin("config", false)) {
    LOG(F("Preferences init failed"));
    return;
  }

  LOG(F("Preferences initialized successfully"));
  
  //  size_t written = testPrefs.putString("deviceID", "");
  //  if (written > 0) {
  //    LOG(F("TEST PREF WRITE OK"));
  //  } else {
  //    LOG(F("TEST PREF WRITE FAILED"));
  //  }
  //
  //  testPrefs.end();
}

bool writePref(const char* key, const char* value) {
  LOGF("Writing preference: %s\n", key);

  Preferences prefs;
  if (!prefs.begin("config", false)) {   // read/write
    LOG(F("- failed to open Preferences for writing"));
    return false;
  }

  size_t written = prefs.putString(key, value);
  prefs.end();

  if (written > 0) {
    LOG(F("- preference written"));
    return true;
  } else {
    LOG(F("- preference write failed"));
    return false;
  }
}

String readPref(const char* key) {
  Preferences prefs;
  if (!prefs.begin("config", true)) {   // read only
    LOG(F("- failed to open Preferences for reading"));
    return "";
  }

  String value = prefs.getString(key, "");
  prefs.end();

  if (value == "") {
    LOGF("Preference %s does not exist or is empty\n", key);
  }

  return value;
}

bool removePref(const char* key) {
  Preferences prefs;
  if (!prefs.begin("config", false)) {
    LOG(F("- failed to open Preferences"));
    return false;
  }

  bool removed = prefs.remove(key);
  prefs.end();

  if (removed) {
    LOG(F("- preference removed"));
  } else {
    LOG(F("- failed to remove preference"));
  }

  return removed;
}

String readRawVersion() {
  String pv = readPref(VERSION_KEY);
  if (pv == "") {
    return "0.0.0";
  }
  LOG("Previous Version: " + pv);
  return pv;
}
