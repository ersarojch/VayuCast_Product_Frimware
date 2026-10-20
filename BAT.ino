// ============================================================
// 18650 Battery Voltage - ESP32 GPIO34
// Divider: 47K + 47K
// ============================================================

#define BATTERY_PIN      34
#define BATTERY_SAMPLES  64

#define BATTERY_R1       47000.0f
#define BATTERY_R2       47000.0f

// Adjust using multimeter
#define BATTERY_CAL      1.0000f


float readBatteryVoltage()
{
  static bool initialized = false;

  if (!initialized) {
    analogReadResolution(12);
    analogSetPinAttenuation(BATTERY_PIN, ADC_11db);
    pinMode(BATTERY_PIN, INPUT);
    initialized = true;
  }

  uint32_t total_mV = 0;

  for (uint8_t i = 0; i < BATTERY_SAMPLES; i++) {
    total_mV += analogReadMilliVolts(BATTERY_PIN);
    delay(2);
  }

  float adcVoltage = (total_mV / (float)BATTERY_SAMPLES) / 1000.0f;

  float batteryVoltage =
      adcVoltage *
      ((BATTERY_R1 + BATTERY_R2) / BATTERY_R2) *
      BATTERY_CAL;

  LOGF("[BATTERY] ADC: %.3f V | Battery: %.3f V",
       adcVoltage,
       batteryVoltage);

  return batteryVoltage;
}
