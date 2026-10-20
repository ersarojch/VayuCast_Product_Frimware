// ============================================================
// SHT31 / SHT4X + SDS011 SAMPLING
// ============================================================

// Recommended for your long I2C cable (up to ~5 meters)
static const uint32_t SHT_I2C_CLOCK_HZ   = 50000;  // 50 kHz
static const uint16_t SHT_I2C_TIMEOUT_MS = 100;

// Recover SHT/I2C after this many consecutive failures
static const uint8_t SHT_RECOVERY_LIMIT = 2;

static uint8_t shtFailCount = 0;


// ============================================================
// APPLY I2C SETTINGS
// ============================================================

void applySHTI2CSettings() {

  Wire.setClock(SHT_I2C_CLOCK_HZ);
  Wire.setTimeOut(SHT_I2C_TIMEOUT_MS);
}


// ============================================================
// READ SHT SENSOR
// ============================================================

bool readSHT(float &t, float &h)
{
  // Always start with invalid values
  t = NAN;
  h = NAN;

  // Make sure long-cable I2C speed is used
  applySHTI2CSettings();


#if SENSOR_TYPE == SENSOR_SHT31

  t = sht31.readTemperature();
  h = sht31.readHumidity();

  if (isnan(t) || isnan(h)) {

    LOG("[SHT] SHT31 read failed");

    return false;
  }


#elif SENSOR_TYPE == SENSOR_SHT4X

  /*
     Initialize the structures to zero.

     More importantly, we CHECK the return value of
     sht4.getEvent().

     If an I2C transfer or CRC check fails,
     getEvent() returns false.
  */

  sensors_event_t humidity = {};
  sensors_event_t temp     = {};

  bool success = sht4.getEvent(
                   &humidity,
                   &temp
                 );

  if (!success) {

    LOG("[SHT] getEvent FAILED - I2C/CRC error");

    return false;
  }

  h = humidity.relative_humidity;
  t = temp.temperature;


#else

  LOG("[SHT] Invalid SENSOR_TYPE");
  return false;

#endif


  // ----------------------------------------------------------
  // Validate final values
  // ----------------------------------------------------------

  if (isnan(h) || isnan(t)) {

    LOG("[SHT] NAN reading");

    return false;
  }


  if (h < 0.0f || h > 100.0f) {

    LOGF(
      "[SHT] Invalid humidity: %.2f",
      h
    );

    return false;
  }


  if (t < -40.0f || t > 125.0f) {

    LOGF(
      "[SHT] Invalid temperature: %.2f",
      t
    );

    return false;
  }


  return true;
}


// ============================================================
// RECOVER I2C + SHT SENSOR
// ============================================================

bool recoverSHT()
{
  LOG("[SHT] ==================================");
  LOG("[SHT] Starting I2C/SHT recovery...");
  LOG("[SHT] ==================================");


  // ----------------------------------------------------------
  // Stop ESP32 I2C peripheral
  // ----------------------------------------------------------

  Wire.end();

  delay(50);


  // ----------------------------------------------------------
  // Start I2C again on your ESP32 pins
  // ----------------------------------------------------------

  bool wireStarted = Wire.begin(
                       SDA_PIN,
                       SCL_PIN
                     );

  if (!wireStarted) {

    LOG("[SHT] Wire.begin FAILED");

    return false;
  }


  applySHTI2CSettings();

  delay(20);


  // ----------------------------------------------------------
  // Reinitialize SHT sensor
  //
  // beginSensor() is already present in your SHT_CONFIG tab.
  // For SHT4x this also initializes/resets the sensor.
  // ----------------------------------------------------------

  if (!beginSensor()) {

    LOG("[SHT] Sensor reinitialization FAILED");

    return false;
  }


  /*
     beginSensor()/library initialization may modify
     I2C settings, so force 50 kHz again afterward.
  */

  applySHTI2CSettings();

  delay(20);


  LOG("[SHT] Recovery SUCCESS");
  LOG("[SHT] I2C = 50 kHz");

  return true;
}


// ============================================================
// SAMPLE ALL SENSORS
// ============================================================

SensorData sampleSensors(
  uint8_t samples,
  uint32_t interval_ms
) {

  SensorData out;


  // ----------------------------------------------------------
  // Accumulators
  // ----------------------------------------------------------

  float sum_h   = 0.0f;
  float sum_t   = 0.0f;
  float sum_p25 = 0.0f;
  float sum_p10 = 0.0f;


  uint8_t cnt_sht = 0;
  uint8_t cnt_pm  = 0;


  // ----------------------------------------------------------
  // Sampling loop
  // ----------------------------------------------------------

  for (uint8_t i = 0; i < samples; i++) {

    uint32_t startTime = millis();


    float h   = NAN;
    float t   = NAN;

    float p25 = NAN;
    float p10 = NAN;


    // ========================================================
    // SHT31 / SHT45
    // ========================================================

    bool sht_ok = readSHT(t, h);


    // --------------------------------------------------------
    // SHT reading failed
    // --------------------------------------------------------

    if (!sht_ok) {

      shtFailCount++;


      LOGF(
        "[SHT] Failure %d / %d",
        shtFailCount,
        SHT_RECOVERY_LIMIT
      );


      // ------------------------------------------------------
      // Recover after repeated failures
      // ------------------------------------------------------

      if (shtFailCount >= SHT_RECOVERY_LIMIT) {

        LOG("[SHT] Consecutive failures detected");
        LOG("[SHT] Attempting automatic recovery...");


        if (recoverSHT()) {

          /*
             Reset counter because bus/sensor
             initialization succeeded.
          */

          shtFailCount = 0;


          // Give sensor a little time
          delay(50);


          // --------------------------------------------------
          // Retry immediately after recovery
          // --------------------------------------------------

          LOG("[SHT] Retrying measurement...");


          sht_ok = readSHT(t, h);


          if (sht_ok) {

            LOG("[SHT] Reading restored after recovery");

          } else {

            /*
               Recovery completed but actual reading
               still failed.

               Start failure count again.
            */

            shtFailCount = 1;

            LOG("[SHT] Still failing after recovery");
          }


        } else {

          /*
             Keep it at the limit.

             This allows another recovery attempt
             during the next sample.
          */

          shtFailCount = SHT_RECOVERY_LIMIT;

          LOG("[SHT] Recovery FAILED");
        }
      }
    }


    // --------------------------------------------------------
    // Valid SHT measurement
    // --------------------------------------------------------

    if (sht_ok) {

      shtFailCount = 0;


      sum_h += h;
      sum_t += t;

      cnt_sht++;


      LOGF(
        "SHT OK: Temp = %.2f C, Hum = %.2f %%",
        t,
        h
      );


    } else {

      /*
         IMPORTANT:

         Nothing is added to the average.

         Therefore bad I2C readings such as:
         0 C / 1.96 %
         will NOT be used.
      */

      LOG("[SHT] Reading rejected");
    }



    // ========================================================
    // SDS011 PM SENSOR
    // ========================================================

    bool pm_ok = false;


    /*
       Continue checking SDS011 during the sample interval.

       The last valid PM measurement received during
       this interval will be used.
    */

    while (millis() - startTime < interval_ms) {

      float current_p25;
      float current_p10;


      if (readSDS(
            current_p25,
            current_p10
          )) {


        if (!isnan(current_p25) &&
            !isnan(current_p10) &&
            current_p25 >= 0.0f &&
            current_p10 >= 0.0f) {


          p25 = current_p25;
          p10 = current_p10;

          pm_ok = true;
        }
      }


      yield();
      delay(2);
    }


    // --------------------------------------------------------
    // Valid SDS011 reading
    // --------------------------------------------------------

    if (pm_ok) {

      sum_p25 += p25;
      sum_p10 += p10;

      cnt_pm++;


      LOGF(
        "SDS011 OK: PM2.5 = %.2f ug/m3, PM10 = %.2f ug/m3",
        p25,
        p10
      );


    } else {

      LOG("SDS011 invalid or no reading received");
    }


    LOGF(
      "Sampling: %d / %d",
      i + 1,
      samples
    );
  }



  // ==========================================================
  // FINAL AVERAGES
  // ==========================================================

  out.valid_dht = (cnt_sht > 0);
  out.valid_pm  = (cnt_pm > 0);


  // ----------------------------------------------------------
  // SHT average
  // ----------------------------------------------------------

  if (out.valid_dht) {

    out.h = sum_h / cnt_sht;
    out.t = sum_t / cnt_sht;

  } else {

    /*
       Do NOT send fake values.

       Your JSON code will convert this to "NAN".
    */

    out.h = NAN;
    out.t = NAN;
  }


  // ----------------------------------------------------------
  // SDS011 average
  // ----------------------------------------------------------

  if (out.valid_pm) {

    out.p25 = sum_p25 / cnt_pm;
    out.p10 = sum_p10 / cnt_pm;

  } else {

    out.p25 = NAN;
    out.p10 = NAN;
  }



  // ==========================================================
  // DEBUG OUTPUT
  // ==========================================================

  LOGF(
    "SHT successful samples = %d / %d",
    cnt_sht,
    samples
  );


  LOGF(
    "PM successful samples = %d / %d",
    cnt_pm,
    samples
  );


  if (out.valid_dht) {

    LOGF(
      "SHT average: Temp = %.2f C, Hum = %.2f %%",
      out.t,
      out.h
    );

  } else {

    LOG("[SHT] NO valid SHT readings");
  }


  if (out.valid_pm) {

    LOGF(
      "SDS011 average: PM2.5 = %.2f ug/m3, PM10 = %.2f ug/m3",
      out.p25,
      out.p10
    );

  } else {

    LOG("[SDS] NO valid PM readings");
  }


  return out;
}
