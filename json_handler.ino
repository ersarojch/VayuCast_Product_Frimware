
size_t buildJson(
  char *buffer,
  size_t bufferSize,
  const char *device_id,
  float h,
  float t,
  float p25,
  float p10,
  float chiT,
  int grssi,
  float batV

) {
  StaticJsonDocument<256> doc;

  // Top-level UID
  doc["uid"] = device_id;

  // Nested object: mes
  JsonObject mes = doc.createNestedObject("mes");
  if (!isnan(h))
    mes["shu45"] = round(h * 100) / 100.0;
  else
    mes["shu45"] = "NAN";

  if (!isnan(t))
    mes["ste45"] = round(t * 100) / 100.0;
  else
    mes["ste45"] = "NAN";

  // PM10
  if (p10 >= 0) mes["sp1"] = round(p10 * 100) / 100.0;
  else           mes["sp1"] = "NAN";

  // PM2.5
  if (p25 >= 0) mes["sp2"] = round(p25 * 100) / 100.0;
  else           mes["sp2"] = "NAN";

  // ESP32 internal chip temperature
  if (!isnan(chiT)) {
    mes["chiT"] = round(chiT * 100.0) / 100.0;
  } else {
    mes["chiT"] = "NAN";
  }

  // GSM signal strength in dBm
  if (grssi != -999) {
    mes["grssi"] = grssi;
  } else {
    mes["grssi"] = "NAN";
  }

  if (batV >= 2.0f && batV <= 8.4f)
    mes["batV"] = roundf(batV * 100.0f) / 100.0f;
  else
    mes["batV"] = "NAN";

  // Serialize JSON into the provided buffer
  return serializeJson(doc, buffer, bufferSize);
}
