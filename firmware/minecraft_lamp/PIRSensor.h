// ============================================================
// PIRSensor.h — Sensor PIR HC-SR501 con debounce de 30s (D3)
// IoT Lámpara WiFi v10
// ============================================================
#pragma once
#include "Config.h"

class PIRSensor {
public:
  void begin() {
    pinMode(PIR_PIN, INPUT);
    _lastTrigger = 0;
    DBGLN("  [PIR] Inicializado");
  }

  /**
   * Llama en cada iteración del loop.
   * Devuelve true si hay una detección nueva (respetando debounce de 30s).
   */
  bool check() {
    if (digitalRead(PIR_PIN) == HIGH) {
      unsigned long now = millis();
      if (now - _lastTrigger >= PIR_DEBOUNCE_MS) {
        _lastTrigger = now;
        DBGLN("  [PIR] Movimiento detectado");
        return true;
      }
    }
    return false;
  }

  unsigned long lastTriggerMs() const { return _lastTrigger; }

private:
  unsigned long _lastTrigger = 0;
};
