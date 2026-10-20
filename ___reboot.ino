//
//#include "esp_system.h"
//
//
//const char* getResetReasonText(esp_reset_reason_t reason)
//{
//  switch (reason)
//  {
//    case ESP_RST_POWERON:
//      return "Power ON";
//
//    case ESP_RST_EXT:
//      return "External Reset";
//
//    case ESP_RST_SW:
//      return "Software Reset";
//
//    case ESP_RST_PANIC:
//      return "Crash / Panic";
//
//    case ESP_RST_INT_WDT:
//      return "Interrupt Watchdog";
//
//    case ESP_RST_TASK_WDT:
//      return "Task Watchdog";
//
//    case ESP_RST_WDT:
//      return "Other Watchdog";
//
//    case ESP_RST_DEEPSLEEP:
//      return "Deep Sleep Wakeup";
//
//    case ESP_RST_BROWNOUT:
//      return "Brownout";
//
//    case ESP_RST_SDIO:
//      return "SDIO Reset";
//
//    default:
//      return "Unknown";
//  }
//}
//
//
//void trackBootCount()
//{
//  Preferences preferences;
//
//  // Open NVS namespace
//  preferences.begin("system", false);
//
//  // Read previous count
//  uint32_t bootCount = preferences.getUInt("bootCount", 0);
//
//  // Increment because ESP has booted again
//  bootCount++;
//
//  // Store updated value
//  preferences.putUInt("bootCount", bootCount);
//
//  preferences.end();
//
//
//  // Get reset reason
//  esp_reset_reason_t resetReason = esp_reset_reason();
//
//
//  Serial.println();
//  Serial.println("================================");
//  Serial.println("       ESP32 BOOT INFO");
//  Serial.println("================================");
//
//  Serial.print("Boot Count   : ");
//  Serial.println(bootCount);
//
//  Serial.print("Reset Reason : ");
//  Serial.println(getResetReasonText(resetReason));
//
//  Serial.println("================================");
//  Serial.println();
//}
//
//
//// Use only for deliberate, bounded waits.
//// Calling this does not feed the watchdog while another function is blocked.
void startupDelay(uint32_t durationMs)
{
  const uint32_t startedAt = millis();

  while ((uint32_t)(millis() - startedAt) < durationMs) {
    feedWatchDog();
    delay(10);
  }

  feedWatchDog();
}


// Keep failed startup from falling through into normal operation.
//
// This preserves your existing reset-based recovery policy.
// The requestHardPowerReset() implementation must have its own
// bounded waits and watchdog handling.
void recoverStartupFailure(const char *reason)
{
  Serial.printf("[BOOT] Startup failed: %s\n", reason);
  Serial.println("[BOOT] Waiting 30 seconds before recovery");

  startupDelay(30000);

  // Your existing modem/system recovery function.
  requestHardPowerReset(reason);

  // If it returns, restart the ESP so setup() begins from a known state.
  Serial.println("[BOOT] Recovery function returned; restarting ESP");
  startupDelay(1000);

  ESP.restart();
}
