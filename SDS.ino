byte wakeCmd[] = {
  0xAA, 0xB4, 0x06, 0x01, 0x01, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xFF, 0xFF, 0x06, 0xAB
};

byte sleepCmd[] = {
  0xAA, 0xB4, 0x06, 0x01, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xFF, 0xFF, 0x05, 0xAB
};

// Static variables persist between function calls to remember where we are in the packet
static int  sds_len = 0;
static int  sds_pm25_raw = 0;
static int  sds_pm10_raw = 0;
static int  sds_checksum_calc = 0;

bool readSDS(float &pm25, float &pm10) {
  // Process all available bytes in the serial buffer
  while (sdsSerial.available() > 0) {
    byte value = sdsSerial.read();

    switch (sds_len) {
      case 0: // Expecting Start Header 0xAA
        if (value == 0xAA) {
          sds_len = 1;
          sds_checksum_calc = 0;
        }
        break;

      case 1: // Expecting Command ID 0xC0
        if (value == 0xC0) sds_len = 2;
        else sds_len = 0; // Reset if wrong byte
        break;

      case 2: // PM2.5 Low Byte
        sds_pm25_raw = value;
        sds_checksum_calc = value;
        sds_len = 3;
        break;

      case 3: // PM2.5 High Byte
        sds_pm25_raw |= (value << 8);
        sds_checksum_calc += value;
        sds_len = 4;
        break;

      case 4: // PM10 Low Byte
        sds_pm10_raw = value;
        sds_checksum_calc += value;
        sds_len = 5;
        break;

      case 5: // PM10 High Byte
        sds_pm10_raw |= (value << 8);
        sds_checksum_calc += value;
        sds_len = 6;
        break;

      case 6: // ID Byte 1
      case 7: // ID Byte 2
        sds_checksum_calc += value;
        sds_len++;
        break;

      case 8: // Checksum Verification
        if (value == (sds_checksum_calc % 256)) {
          sds_len = 9;
        } else {
          LOG("SDS Checksum Error!");
          sds_len = 0; // Reset
          sds_checksum_calc = 0;
        }
        break;

      case 9: // Expecting Tail Header 0xAB
        if (value == 0xAB) {
          // Success! Fill the reference variables
          pm25 = sds_pm25_raw / 10.0;
          pm10 = sds_pm10_raw / 10.0;
          sds_len = 0; // Reset for next packet
          return true;
        }
        sds_len = 0;
        break;
    }
  }
  return false; // No full packet received yet
}
