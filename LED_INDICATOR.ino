void LED_indicator(void *parameter) {
  pinMode(Status_LED_PIN, OUTPUT);
  digitalWrite(Status_LED_PIN, LOW);

  while (true) {

    // Highest priority: OTA fast blinking
    if (otaInProgress) {
      digitalWrite(Status_LED_PIN, HIGH);
      vTaskDelay(pdMS_TO_TICKS(200));

      digitalWrite(Status_LED_PIN, LOW);
      vTaskDelay(pdMS_TO_TICKS(200));

      continue;
    }

    // Server communication successful: LED continuously ON
    if (httpSuscess) {
      digitalWrite(Status_LED_PIN, HIGH);
      vTaskDelay(pdMS_TO_TICKS(1000));

      continue;
    }

    // Server communication failed: slow blinking
    digitalWrite(Status_LED_PIN, HIGH);
    vTaskDelay(pdMS_TO_TICKS(1000));

    digitalWrite(Status_LED_PIN, LOW);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
