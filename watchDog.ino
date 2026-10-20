#include "esp_task_wdt.h"

#define WATCHDOG_TIMEOUT_MS 120 *1000  // 120 seconds

bool watchdogEnabled = false;

//--------------------------------------------------
// Configure and enable ESP32 Task Watchdog
//--------------------------------------------------

void setupWatchDog()
{
  esp_task_wdt_config_t config = {
    .timeout_ms = WATCHDOG_TIMEOUT_MS,
    .idle_core_mask = (1U << portNUM_PROCESSORS) - 1U,
    .trigger_panic = true
  };

  // Arduino may already initialize the Task Watchdog
  esp_err_t result = esp_task_wdt_reconfigure(&config);

  // If not initialized, initialize it
  if (result == ESP_ERR_INVALID_STATE) {
    result = esp_task_wdt_init(&config);
  }

  if (result != ESP_OK) {
    Serial.printf(
      "[WDT] Watchdog configuration failed: %d\n",
      result
    );
    return;
  }

  // Subscribe the current Arduino setup/loop task
  if (esp_task_wdt_status(NULL) != ESP_OK) {
    result = esp_task_wdt_add(NULL);

    if (result != ESP_OK) {
      Serial.printf(
        "[WDT] Failed to subscribe loop task: %d\n",
        result
      );
      return;
    }
  }

  watchdogEnabled = true;

  Serial.printf(
    "[WDT] Enabled with %lu-second timeout\n",
    WATCHDOG_TIMEOUT_MS / 1000
  );
}

//--------------------------------------------------
// Feed watchdog from setup()/loop() task
//--------------------------------------------------

void feedWatchDog()
{
  if (!watchdogEnabled) {
    return;
  }

  esp_err_t result = esp_task_wdt_reset();

  if (result != ESP_OK) {
    Serial.printf(
      "[WDT] Feed failed: %d\n",
      result
    );
  }
} 
