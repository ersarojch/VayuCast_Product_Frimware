#include <Wire.h>
#include "Adafruit_SHT31.h"
#include "Adafruit_SHT4x.h"

#define SDA_PIN 21
#define SCL_PIN 22

#define I2C_CLOCK_HZ 50000
#define I2C_TIMEOUT_MS 50

#define SENSOR_SHT31 31
#define SENSOR_SHT4X 45

//#define SENSOR_TYPE SENSOR_SHT31
#define SENSOR_TYPE SENSOR_SHT4X

Adafruit_SHT31 sht31 = Adafruit_SHT31();
Adafruit_SHT4x sht4;


bool beginSensor() {

  // Explicitly select ESP32 I2C pins
  Wire.setPins(SDA_PIN, SCL_PIN);

#if SENSOR_TYPE == SENSOR_SHT31

  Serial.println("Initializing SHT31 sensor...");

  if (!sht31.begin(0x44)) {
    Serial.println("Couldn't find SHT31");
    return false;
  }

  Wire.setClock(I2C_CLOCK_HZ);
  Wire.setTimeOut(I2C_TIMEOUT_MS);

  sht31.heater(false);

  Serial.println("SHT31 found.");
  return true;


#elif SENSOR_TYPE == SENSOR_SHT4X

  Serial.println("Initializing SHT4x sensor...");

  if (!sht4.begin(&Wire)) {
    Serial.println("Couldn't find SHT4x");
    return false;
  }

  // Do this AFTER sht4.begin()
  Wire.setClock(I2C_CLOCK_HZ);
  Wire.setTimeOut(I2C_TIMEOUT_MS);

  sht4.setPrecision(SHT4X_HIGH_PRECISION);
  sht4.setHeater(SHT4X_NO_HEATER);

  Serial.print("I2C clock: ");
  Serial.println(Wire.getClock());

  Serial.println("SHT4x found. Heater OFF.");

  return true;

#endif
}
