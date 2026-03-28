// ============================================================
// WiFiManager.h — Gestión de WiFi con estrategia escalonada 3 fases
// IoT Lámpara WiFi v10
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

  // ─── Conectar a red bootstrap ─────────────────────────────
  bool connectBootstrap() {
    DBGF("[WiFi] Conectando a bootstrap: %s\n", BOOTSTRAP_SSID);
    return _connect(BOOTSTRAP_SSID, BOOTSTRAP_PASSWORD, WiFiState::BOOTSTRAP_CONNECTED);
  }

  // ─── Conectar a red operativa ─────────────────────────────
  bool connectOperational(const char* ssid, const char* password) {
    DBGF("[WiFi] Conectando a red operativa: %s\n", ssid);
    return _connect(ssid, password, WiFiState::OPERATIONAL_CONNECTED);
  }

  bool isConnected() { return WiFi.status() == WL_CONNECTED; }

  String localIP() { return WiFi.localIP().toString(); }

  void disconnect() { WiFi.disconnect(true); state = WiFiState::DISCONNECTED; }

  /**
   * Estrategia de reconexión escalonada 3 fases (DER-04 §8.3).
   *
   * Fase A: Fast retries   → 3 reintentos rápidos cada 10s (atrapa parpadeos de red)
   * Fase B: Exp. backoff   → Intervalos crecientes 10s→20s→40s→60s (cap) a red operativa
   * Fase C: Bootstrap      → Cada N fallos, reconectar a bootstrap para re-escanear/obtener creds
   *
   * @param opSSID       Última red operativa conocida
   * @param opPass       Password de red operativa
   * @param failCount    Contador de fallos acumulados (manejado externamente)
   * @param every_n      Valor de RECOVERY_BOOTSTRAP_EVERY_N_CYCLES
   * @returns true si logró reconectarse (a cualquier red)
   */
  bool attemptRecovery(const char* opSSID, const char* opPass, int& failCount, int every_n) {
    if (strlen(opSSID) == 0) {
      // Sin red operativa → ir directo a bootstrap
      DBGLN("[WiFi] Sin red operativa → bootstrap");
      return connectBootstrap();
    }

    // ── Fase A: Fast retries (solo en los primeros intentos)
    if (failCount == 0) {
      DBGLN("[WiFi] Fase A: Reintentos rápidos");
      for (int i = 0; i < INITIAL_RETRY_COUNT; i++) {
        DBGF("[WiFi]   Reintento rápido %d/%d a \"%s\"\n", i + 1, INITIAL_RETRY_COUNT, opSSID);
        if (_connect(opSSID, opPass, WiFiState::OPERATIONAL_CONNECTED)) {
          DBGLN("[WiFi] Fase A: Reconexión exitosa ✓");
          failCount = 0;
          return true;
        }
        if (i < INITIAL_RETRY_COUNT - 1) delay(INITIAL_RETRY_INTERVAL_MS);
      }
      failCount = INITIAL_RETRY_COUNT;
      DBGF("[WiFi] Fase A agotada (%d intentos). Pasando a Fase B.\n", INITIAL_RETRY_COUNT);
    }

    // ── Fase B: Intentar red operativa con backoff exponencial
    DBGF("[WiFi] Fase B: Intento a \"%s\" (fallo #%d)\n", opSSID, failCount + 1);
    if (_connect(opSSID, opPass, WiFiState::OPERATIONAL_CONNECTED)) {
      failCount = 0;
      DBGLN("[WiFi] Fase B: Reconexión operativa exitosa ✓");
      return true;
    }

    failCount++;
    DBGF("[WiFi] Fase B: Fallo #%d\n", failCount);

    // ── Fase C: Cada N fallos → bootstrap para diagnóstico y re-escaneo
    if (failCount % every_n == 0) {
      DBGF("[WiFi] Fase C: Bootstrap de recuperación (cada %d fallos)\n", every_n);
      if (connectBootstrap()) {
        state = WiFiState::RECOVERY_MODE;
        return true; // conectado a bootstrap en modo recuperación
      }
      DBGLN("[WiFi] Fase C: Bootstrap también falló ✗");
    }

    state = WiFiState::RECOVERY_MODE;
    return false;
  }

  /**
   * Calcula el intervalo de backoff exponencial para reconexión.
   * 10s → 20s → 40s → 60s (cap = RECONNECT_MAX_INTERVAL_MS)
   */
  static unsigned long getBackoffInterval(int failCount) {
    // Base: 10s, doubles each failure, capped at max
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
      DBGF("[WiFi] Conectado a %s IP: %s\n", ssid, WiFi.localIP().toString().c_str());
      return true;
    }

    DBGF("[WiFi] Fallo al conectar a %s\n", ssid);
    state = WiFiState::DISCONNECTED;
    return false;
  }
};
