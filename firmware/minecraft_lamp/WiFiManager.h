// ============================================================
// WiFiManager.h — Gestion de WiFi con estrategia escalonada 3 fases
// IoT Lampara WiFi v10
// ============================================================
#pragma once
#include <WiFi.h>
#include "Config.h"

enum class WiFiState {
  DISCONNECTED,
  BOOTSTRAP_CONNECTED,
  OPERATIONAL_CONNECTED,
  RECOVERY_MODE
};

class WiFiManager {
public:
  WiFiState state = WiFiState::DISCONNECTED;

  // Obtiene la MAC del ESP32 desde eFuse (no depende del driver WiFi)
  String getMac() {
    uint64_t chipid = ESP.getEfuseMac();
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             (uint8_t)(chipid),
             (uint8_t)(chipid >> 8),
             (uint8_t)(chipid >> 16),
             (uint8_t)(chipid >> 24),
             (uint8_t)(chipid >> 32),
             (uint8_t)(chipid >> 40));
    return String(buf);
  }

  bool connectBootstrap() {
    return _connect(BOOTSTRAP_SSID, BOOTSTRAP_PASSWORD, WiFiState::BOOTSTRAP_CONNECTED);
  }

  bool connectOperational(const char* ssid, const char* password) {
    return _connect(ssid, password, WiFiState::OPERATIONAL_CONNECTED);
  }

  bool isConnected() { return WiFi.status() == WL_CONNECTED; }

  String localIP() { return WiFi.localIP().toString(); }

  void disconnect() { WiFi.disconnect(true); state = WiFiState::DISCONNECTED; }

  /**
   * Estrategia de reconexion escalonada 3 fases:
   *   Fase A: 3 reintentos rapidos cada 10s
   *   Fase B: Backoff exponencial 10s-60s a red operativa
   *   Fase C: Cada N fallos, reconectar a bootstrap
   */
  bool attemptRecovery(const char* opSSID, const char* opPass, int& failCount, int every_n) {
    if (strlen(opSSID) == 0) {
      return connectBootstrap();
    }

    // Fase A: Fast retries (solo cuando failCount==0)
    if (failCount == 0) {
      for (int i = 0; i < INITIAL_RETRY_COUNT; i++) {
        if (_connect(opSSID, opPass, WiFiState::OPERATIONAL_CONNECTED)) {
          failCount = 0;
          return true;
        }
        if (i < INITIAL_RETRY_COUNT - 1) delay(INITIAL_RETRY_INTERVAL_MS);
      }
      failCount = INITIAL_RETRY_COUNT;
    }

    // Fase B: Intento con backoff exponencial
    if (_connect(opSSID, opPass, WiFiState::OPERATIONAL_CONNECTED)) {
      failCount = 0;
      return true;
    }

    failCount++;

    // Fase C: Cada N fallos -> bootstrap
    if (failCount % every_n == 0) {
      if (connectBootstrap()) {
        state = WiFiState::RECOVERY_MODE;
        return true;
      }
    }

    state = WiFiState::RECOVERY_MODE;
    return false;
  }

  /**
   * Calcula intervalo de backoff exponencial: 10s -> 20s -> 40s -> 60s (cap)
   */
  static unsigned long getBackoffInterval(int failCount) {
    unsigned long interval = INITIAL_RETRY_INTERVAL_MS;
    for (int i = 1; i < failCount && interval < RECONNECT_MAX_INTERVAL_MS; i++) {
      interval *= 2;
    }
    return min(interval, RECONNECT_MAX_INTERVAL_MS);
  }

private:
  bool _connect(const char* ssid, const char* password, WiFiState onSuccess) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
      delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
      state = onSuccess;
      return true;
    }

    state = WiFiState::DISCONNECTED;
    return false;
  }
};
