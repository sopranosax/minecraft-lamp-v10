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

  // Obtiene la MAC del ESP32
  String getMac() {
    return WiFi.macAddress();
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
   * Estrategia de reconexión escalonada (DER-04 §8.3).
   * Llama periódicamente desde el loop cuando se está en RECOVERY_MODE.
   *
   * @param opSSID       Última red operativa conocida
   * @param opPass       Password de red operativa
   * @param failCount    Contador de fallos acumulados (manejado externamente)
   * @param every_n      Valor de RECOVERY_BOOTSTRAP_EVERY_N_CYCLES
   * @returns true si logró reconectarse (a cualquier red)
   */
  bool attemptRecovery(const char* opSSID, const char* opPass, int& failCount, int every_n) {
    if (strlen(opSSID) == 0) {
      // Sin red operativa → ir a bootstrap
      return connectBootstrap();
    }

    // Intentar reconectar a red operativa
    if (_connect(opSSID, opPass, WiFiState::OPERATIONAL_CONNECTED)) {
      failCount = 0;
      DBGLN("[WiFi] Reconexión operativa exitosa.");
      return true;
    }

    failCount++;
    DBGF("[WiFi] Fallo de reconexión #%d\n", failCount);

    // Cada N fallos → conectar momentáneamente a bootstrap para publicar diagnóstico
    if (failCount % every_n == 0) {
      DBGLN("[WiFi] Intentando bootstrap de recuperación...");
      if (connectBootstrap()) {
        state = WiFiState::RECOVERY_MODE;
        return true; // conectado a bootstrap en modo recuperación
      }
    }

    state = WiFiState::RECOVERY_MODE;
    return false;
  }

  // Fase A: 3 reintentos rápidos a la red operativa (cada 10s)
  bool fastRetries(const char* ssid, const char* password) {
    for (int i = 0; i < INITIAL_RETRY_COUNT; i++) {
      DBGF("[WiFi] Reintento rápido %d/%d a %s\n", i+1, INITIAL_RETRY_COUNT, ssid);
      if (_connect(ssid, password, WiFiState::OPERATIONAL_CONNECTED)) return true;
      delay(INITIAL_RETRY_INTERVAL_MS);
    }
    return false;
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
