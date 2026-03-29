// ============================================================
// Config.h — Constantes de configuración del firmware
// IoT Lámpara WiFi v10
// ============================================================
#pragma once

// ─── WiFi Bootstrap (compilado en firmware) ──────────────────
// IMPORTANTE: cambiar por las credenciales reales de tu red bootstrap
#define BOOTSTRAP_SSID "ANIBAL"
#define BOOTSTRAP_PASSWORD "igkz1998"

// ─── Backend Apps Script ─────────────────────────────────────
// Reemplazar con la URL real obtenida al deployar el Apps Script
#define BACKEND_URL                                                            \
  "https://script.google.com/macros/s/"                                        \
  "AKfycbyH03D88pGiMwoAGkqYROxISrscGmhq8AHcMErihNzuZZjeRkg_"                   \
  "UNSUxMevx4cfqlvezg/exec"

// ─── Hardware pins ────────────────────────────────────────────
#define NEOPIXEL_PIN 5   // GPIO del NeoPixel WS2812B
#define NEOPIXEL_COUNT 16 // ⚠️ AJUSTAR al número real de LEDs de tu anillo (ej: 8, 12, 16, 24)
#define PIR_PIN 14       // GPIO del sensor PIR HC-SR501
#define RTC_SDA_PIN 21   // I2C SDA para DS3231
#define RTC_SCL_PIN 22   // I2C SCL para DS3231

// ─── Timing (D1, D3 confirmados) ────────────────────────────
#define POLLING_INTERVAL_MS 30000UL // D1: polling backend cada 30s
#define PIR_DEBOUNCE_MS 30000UL     // D3: cooldown 30s entre detecciones PIR
#define WIFI_CONNECT_TIMEOUT_MS                                                \
  15000UL // Timeout para intentar conectar a una red
#define INITIAL_RETRY_INTERVAL_MS 10000UL // Fase A: 3 reintentos cada 10s
#define INITIAL_RETRY_COUNT 3 // Fase A: número de reintentos rápidos
#define RECONNECT_MAX_INTERVAL_MS 60000UL // Fase B: tope del backoff exponencial

// D2: OFFLINE_THRESHOLD en backend = 5 min; el ESP32 solo hace polling y
// heartbeat D8: recovery bootstrap cada 5 ciclos
#define RECOVERY_BOOTSTRAP_DEFAULT 5

// ─── WatchDog ─────────────────────────────────────────────────
#define WATCHDOG_TIMEOUT_S 30 // WDT timeout en segundos

// ─── NTP ──────────────────────────────────────────────────────
// D4: sincronizar RTC desde NTP en bootstrap
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.google.com"
// UTC-3 Chile Standard Time (ajustar a UTC-4 en horario de verano si aplica)
#define NTP_TIMEZONE "CLT3"

// ─── Firmware version ─────────────────────────────────────────
#define FIRMWARE_VERSION "1.2.3"

// ─── Debug serial ─────────────────────────────────────────────
#define DEBUG_SERIAL true // false para producción

#ifdef DEBUG_SERIAL
#define DBG(x) Serial.print(x)
#define DBGLN(x) Serial.println(x)
#define DBGF(...) Serial.printf(__VA_ARGS__)
#else
#define DBG(x)
#define DBGLN(x)
#define DBGF(...)
#endif
