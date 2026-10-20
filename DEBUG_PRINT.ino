#define LOG_ENABLE 1  // disable logs

#if LOG_ENABLE
#include <stdarg.h>
#include <Arduino.h>

inline void LOG(const __FlashStringHelper *msg) {
  Serial.println(msg);
}
inline void LOG(const char *msg) {
  Serial.println(msg);
}
inline void LOG(const String &msg) {
  Serial.println(msg);
}
inline void LOGG(const String &msg) {
  Serial.print(msg);
}

inline void LOGF(const char *fmt, ...) {
  char buf[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.println(buf);
}

#else
// LOG disabled — all overloads do nothing
inline void LOG(const __FlashStringHelper *msg) {}
inline void LOG(const char *msg) {}
inline void LOG(const String &msg) {}
inline void LOGG(const String &msg) {}
inline void LOGF(const char *fmt, ...) {}
#endif
