// ============================================================
// WatchDog.h — Hardware WatchDog del ESP32
// IoT Lámpara WiFi v10
// ============================================================
#pragma once
#include <esp_task_wdt.h>
#include "Config.h"
#include "Storage.h"

class WatchDog {
public:
  void begin() {
    esp_task_wdt_config_t wdt_cfg = {
      .timeout_ms = WATCHDOG_TIMEOUT_S * 1000,
      .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
      .trigger_panic = true
    };
    esp_task_wdt_reconfigure(&wdt_cfg);
    esp_task_wdt_add(NULL); // Registrar tarea actual
    DBGF("[WDT] Inicializado: timeout=%ds\n", WATCHDOG_TIMEOUT_S);
  }

  // Llamar en CADA iteración del loop principal
  void feed() {
    esp_task_wdt_reset();
  }

  // Marcar en NVS que el reinicio fue por WDT, para informar al backend
  static void markRestart() { Storage::setWatchdogFlag(true); }
  static bool wasWatchdogRestart() { return Storage::getWatchdogFlag(); }
  static void clearRestartFlag()   { Storage::clearWatchdogFlag(); }
};
