// ============================================================
// RTCManager.h — DS3231 + sincronización NTP (D4)
// IoT Lámpara WiFi v10
// ============================================================
#pragma once
#include <Wire.h>
#include <RTClib.h>
#include "Config.h"
#include <time.h>

class RTCManager {
public:
  bool begin() {
    Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN);
    if (!rtc.begin()) {
      DBGLN("  [RTC] DS3231 no encontrado");
      return false;
    }
    DBGLN("  [RTC] DS3231 OK");
    return true;
  }

  /**
   * Sincroniza el DS3231 con NTP. Llamar SOLO cuando hay WiFi activa.
   * Implementa D4: "Desde NTP durante bootstrap".
   */
  void syncWithNTP() {
    DBGLN("  [RTC] Sincronizando con NTP...");
    configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);
    setenv("TZ", NTP_TIMEZONE, 1);
    tzset();

    struct tm timeinfo;
    int attempts = 0;
    while (!getLocalTime(&timeinfo) && attempts < 10) {
      delay(500);
      attempts++;
    }

    if (attempts < 10) {
      // Cargar hora NTP al DS3231
      time_t now;
      time(&now);
      rtc.adjust(DateTime((uint32_t)now));
      DBGF("  [RTC] Sincronizado: %02d:%02d:%02d\n",
           timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
      DBGLN("  [RTC] NTP fallo - usando hora interna DS3231");
    }
  }

  // Devuelve la hora actual del DS3231
  DateTime now() { return rtc.now(); }

  // Devuelve "HH:MM" como string
  String timeString() {
    DateTime t = rtc.now();
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", t.hour(), t.minute());
    return String(buf);
  }

  /**
   * Evalúa si la hora actual está dentro del rango [start, end].
   * Soporta rangos que cruzan medianoche, ej. "22:00" → "06:00".
   */
  bool isWithinSchedule(const char* startTime, const char* endTime) {
    DateTime t  = rtc.now();
    int curMins = t.hour() * 60 + t.minute();
    int stMins  = _toMinutes(startTime);
    int enMins  = _toMinutes(endTime);

    if (stMins <= enMins) {
      return curMins >= stMins && curMins < enMins;
    } else {
      // Cruza medianoche
      return curMins >= stMins || curMins < enMins;
    }
  }

private:
  RTC_DS3231 rtc;

  int _toMinutes(const char* hhmm) {
    int h = 0, m = 0;
    sscanf(hhmm, "%d:%d", &h, &m);
    return h * 60 + m;
  }
};
