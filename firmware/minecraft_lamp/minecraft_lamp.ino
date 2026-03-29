// ============================================================
// minecraft_lamp.ino  --  Loop cooperativo principal
// IoT Lampara WiFi v10  --  Firmware v1.2.2
//
// Hardware:
//   ESP32, NeoPixel WS2812B, PIR HC-SR501, RTC DS3231
//
// Librerias requeridas (instalar en Arduino IDE Library Manager):
//   - Adafruit NeoPixel
//   - RTClib (Adafruit)
//   - ArduinoJson (Benoit Blanchon)
// ============================================================

#include "Config.h"
#include "Storage.h"
#include "LEDControl.h"
#include "PIRSensor.h"
#include "RTCManager.h"
#include "WatchDog.h"
#include "WiFiManager.h"
#include "BackendClient.h"

// --- Instancias globales ---
LEDControl    led;
PIRSensor     pir;
RTCManager    rtc;
WatchDog      wdt;
WiFiManager   wifi;
BackendClient backend;
DeviceConfig  cfg;

// --- Variables de estado ---
String  macAddress;
bool    scheduleCurrentlyOn    = false;
int     reconnectFailCount     = 0;
unsigned long lastPollingMs    = 0;
unsigned long lastReconnectMs  = 0;
unsigned long loopCount        = 0;
unsigned long pollCount        = 0;

#define WIFI_RESCAN_EVERY_N_POLLS 10  // Re-escanear cada 10 polls (~5 min)

// =====================================================================
//  SERIAL OUTPUT HELPERS
// =====================================================================

void _line() {
  DBGLN("----------------------------------------");
}

void _section(const char* title) {
  DBGLN("");
  _line();
  DBGF("  %s\n", title);
  _line();
}

void _kv(const char* key, const char* val) {
  DBGF("  %-16s %s\n", key, val);
}

void _kv(const char* key, int val) {
  DBGF("  %-16s %d\n", key, val);
}

void _kv(const char* key, unsigned long val) {
  DBGF("  %-16s %lu\n", key, val);
}

void _kvOk(const char* key, bool ok) {
  DBGF("  %-16s %s\n", key, ok ? "OK" : "FALLO");
}

void _printConfig() {
  _section("CONFIGURACION ACTIVA");
  _kv("Power",         cfg.manualPowerState);
  _kv("Color",         cfg.manualColorHex);
  DBGF("  %-16s %d / 255 (%d%%)\n", "Brightness", cfg.manualBrightness, cfg.manualBrightness * 100 / 255);
  _kv("Motion mode",   cfg.motionMode);
  _kv("Blink secs",    cfg.motionBlinkSeconds);
  _kv("Schedule",      cfg.scheduleEnabled ? "ON" : "OFF");
  if (cfg.scheduleEnabled) {
    _kv("  Start",      cfg.scheduleStartTime);
    _kv("  End",        cfg.scheduleEndTime);
    _kv("  Color",      cfg.scheduleColorHex);
    DBGF("  %-16s %d / 255 (%d%%)\n", "  Brightness", cfg.scheduleBrightness, cfg.scheduleBrightness * 100 / 255);
  }
  _kv("Reconn int",    cfg.reconnectIntervalSeconds);
  _kv("Recovery N",    cfg.recoveryBootstrapEveryNCycles);
  _kv("Op. SSID",      strlen(cfg.operationalSSID) > 0 ? cfg.operationalSSID : "(ninguna)");
}

// =====================================================================
//  SETUP
// =====================================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  DBGLN("");
  DBGLN("========================================");
  DBGF("  MineCraft Lamp  FW v%s\n", FIRMWARE_VERSION);
  DBGLN("  IoT NeoPixel + PIR + RTC");
  DBGLN("========================================");

  // -- 1. Hardware --
  _section("PASO 1: HARDWARE");
  DBGF("  NeoPixel        GPIO %d, %d LEDs\n", NEOPIXEL_PIN, NEOPIXEL_COUNT);
  led.begin();
  _kvOk("NeoPixel", true);
  DBGF("  PIR             GPIO %d\n", PIR_PIN);
  pir.begin();
  _kvOk("PIR", true);
  DBGF("  RTC DS3231      SDA=%d SCL=%d\n", RTC_SDA_PIN, RTC_SCL_PIN);
  bool rtcOk = rtc.begin();
  _kvOk("RTC", rtcOk);

  // -- 2. NVS --
  _section("PASO 2: NVS (FLASH)");
  Storage::init();
  Storage::load(cfg);
  _kvOk("Config NVS", true);
  _printConfig();

  // -- 3. Identidad --
  _section("PASO 3: IDENTIDAD");
  macAddress = wifi.getMac();
  _kv("MAC", macAddress.c_str());
  _kv("Firmware", FIRMWARE_VERSION);
  _kv("Free heap", ESP.getFreeHeap());

  // -- 4. WatchDog --
  _section("PASO 4: WATCHDOG");
  wdt.begin();
  _kv("Timeout (s)", WATCHDOG_TIMEOUT_S);
  if (WatchDog::wasWatchdogRestart()) {
    DBGLN("  *** REINICIO POR WATCHDOG DETECTADO ***");
  } else {
    _kv("Arranque", "Normal");
  }

  // -- 5. Blink arranque --
  DBGLN("");
  DBGLN("  LED parpadeo de arranque...");
  led.blink(1, "#FFFFFF");

  // -- 6. WiFi --
  _section("PASO 5: CONEXION WiFi");
  bool connectedToOperational = false;

  if (strlen(cfg.operationalSSID) > 0) {
    _kv("Red guardada", cfg.operationalSSID);
    DBGLN("  Conectando...");
    connectedToOperational = wifi.connectOperational(cfg.operationalSSID, cfg.operationalPassword);
    _kvOk("Resultado", connectedToOperational);
  } else {
    DBGLN("  No hay red operativa guardada.");
  }

  if (!connectedToOperational) {
    DBGF("  Intentando bootstrap \"%s\"...\n", BOOTSTRAP_SSID);
    if (wifi.connectBootstrap()) {
      _kv("Bootstrap IP", wifi.localIP().c_str());
      _section("PASO 6: SECUENCIA BOOTSTRAP");
      _doBootstrapSequence();
    } else {
      DBGLN("  Bootstrap no disponible");
      DBGLN("  Entrando en modo LOCAL (offline)");
      wifi.state = WiFiState::RECOVERY_MODE;
    }
  } else {
    _kv("IP", wifi.localIP().c_str());
    _kv("SSID", WiFi.SSID().c_str());
    DBGLN("  Enviando bootstrap al backend...");
    bool bsOk = backend.bootstrap(macAddress.c_str(), wifi.localIP().c_str(), cfg);
    _kvOk("Backend", bsOk);
    DBGLN("  Escaneando redes WiFi...");
    _scanAndPublishNetworks();

    if (WatchDog::wasWatchdogRestart()) {
      DBGLN("  Reportando reinicio WDT...");
      backend.updateStatus(macAddress.c_str(), "WATCHDOG_RESTART", cfg.operationalSSID, wifi.localIP().c_str());
      WatchDog::clearRestartFlag();
    }
  }

  // -- 7. LED --
  _section("PASO 7: APLICAR ESTADO LED");
  DBGF("  Power=%s  Color=%s  Bri=%d\n", cfg.manualPowerState, cfg.manualColorHex, cfg.manualBrightness);
  led.applyConfig(cfg.manualPowerState, cfg.manualColorHex, cfg.manualBrightness);

  // -- Setup completo --
  DBGLN("");
  DBGLN("========================================");
  DBGF("  SETUP COMPLETO  FW v%s\n", FIRMWARE_VERSION);
  DBGF("  MAC  : %s\n", macAddress.c_str());
  DBGF("  WiFi : %s\n", wifi.isConnected() ? WiFi.SSID().c_str() : "OFFLINE");
  DBGF("  IP   : %s\n", wifi.isConnected() ? wifi.localIP().c_str() : "-");
  DBGLN("========================================");
  DBGLN("");
  DBGF("  Loop: poll %lus | PIR %lus | reconn %ds\n",
       POLLING_INTERVAL_MS / 1000, PIR_DEBOUNCE_MS / 1000, cfg.reconnectIntervalSeconds);
  _line();
}

// =====================================================================
//  LOOP
// =====================================================================

void loop() {
  wdt.feed();
  loopCount++;

  // 1. PIR
  if (pir.check()) {
    _handleMotion();
  }

  // 2. Horario programado
  if (cfg.scheduleEnabled && strlen(cfg.scheduleStartTime) > 0) {
    _evaluateSchedule();
  }

  // 3. Polling backend (cada 30s)
  unsigned long now = millis();
  if (wifi.isConnected() && (now - lastPollingMs >= POLLING_INTERVAL_MS)) {
    lastPollingMs = now;
    _pollBackend();
  }

  // 4. Reconexion WiFi si offline
  if (!wifi.isConnected()) {
    unsigned long reconnInterval = WiFiManager::getBackoffInterval(reconnectFailCount);
    if (now - lastReconnectMs >= reconnInterval) {
      lastReconnectMs = now;
      _attemptReconnect();
    }
  }

  delay(50);
}

// =====================================================================
//  BOOTSTRAP SEQUENCE
// =====================================================================

void _doBootstrapSequence() {
  DBGLN("  1/4 Sincronizando RTC con NTP...");
  rtc.syncWithNTP();

  DBGLN("  2/4 Enviando bootstrap al backend...");
  bool bsOk = backend.bootstrap(macAddress.c_str(), wifi.localIP().c_str(), cfg);
  _kvOk("Backend", bsOk);

  if (WatchDog::wasWatchdogRestart()) {
    DBGLN("  Reportando reinicio WDT...");
    backend.updateStatus(macAddress.c_str(), "WATCHDOG_RESTART", "", wifi.localIP().c_str());
    WatchDog::clearRestartFlag();
  }

  DBGLN("  3/4 Escaneando redes WiFi...");
  _scanAndPublishNetworks();

  DBGLN("  4/4 Verificando credenciales pendientes...");
  _checkPendingWifi();
  DBGLN("  Bootstrap completo");
}

// =====================================================================
//  WIFI SCAN & PUBLISH
// =====================================================================

void _scanAndPublishNetworks() {
  int n = WiFi.scanNetworks();
  if (n <= 0) {
    DBGLN("  Sin redes encontradas");
    return;
  }
  DBGF("  %d redes encontradas:\n", n);
  for (int i = 0; i < min(n, 15); i++) {
    DBGF("    %2d. %-24s %4d dBm  %s\n",
         i + 1,
         WiFi.SSID(i).c_str(),
         WiFi.RSSI(i),
         (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "OPEN" : "LOCK");
  }

  StaticJsonDocument<1024> doc;
  JsonArray networks = doc.to<JsonArray>();
  for (int i = 0; i < min(n, 15); i++) {
    JsonObject net = networks.createNestedObject();
    net["ssid"] = WiFi.SSID(i);
    net["rssi"] = WiFi.RSSI(i);
    net["auth"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "OPEN" : "SECURED";
  }

  bool ok = backend.publishScan(macAddress.c_str(), networks);
  DBGF("  Publicacion: %s\n", ok ? "OK" : "FALLO");
  WiFi.scanDelete();
}

// =====================================================================
//  PENDING WIFI CREDENTIALS
// =====================================================================

void _checkPendingWifi() {
  char pendSsid[64] = {0};
  char pendPass[64] = {0};
  bool hasPending   = false;

  backend.getPendingConfig(macAddress.c_str(), cfg, pendSsid, pendPass, hasPending);

  if (hasPending && strlen(pendSsid) > 0) {
    DBGF("  Credencial pendiente: \"%s\"\n", pendSsid);
    backend.updateStatus(macAddress.c_str(), "WIFI_CONNECTING", pendSsid, wifi.localIP().c_str());

    DBGF("  Conectando a \"%s\"...\n", pendSsid);
    if (wifi.connectOperational(pendSsid, pendPass)) {
      DBGF("  CONECTADO  IP: %s\n", wifi.localIP().c_str());
      strlcpy(cfg.operationalSSID,     pendSsid, sizeof(cfg.operationalSSID));
      strlcpy(cfg.operationalPassword, pendPass, sizeof(cfg.operationalPassword));
      Storage::save(cfg);
      reconnectFailCount = 0;
      DBGLN("  Credenciales guardadas en NVS");
      backend.updateStatus(macAddress.c_str(), "WIFI_CONNECTED", pendSsid, wifi.localIP().c_str());
      backend.bootstrap(macAddress.c_str(), wifi.localIP().c_str(), cfg);
    } else {
      DBGF("  FALLO al conectar a \"%s\"\n", pendSsid);
      backend.updateStatus(macAddress.c_str(), "WIFI_FAILED", pendSsid, wifi.localIP().c_str(), "Auth failed");
      DBGLN("  Volviendo a bootstrap...");
      wifi.connectBootstrap();
      _scanAndPublishNetworks();
    }
  } else {
    DBGLN("  No hay credenciales pendientes");
  }
}

// =====================================================================
//  POLLING
// =====================================================================

void _pollBackend() {
  pollCount++;
  DBGF("  [POLL #%lu] uptime %lus\n", pollCount, millis() / 1000);

  char pendSsid[64] = {0};
  char pendPass[64] = {0};
  bool hasPending   = false;

  bool ok = backend.getPendingConfig(macAddress.c_str(), cfg, pendSsid, pendPass, hasPending);
  if (!ok) {
    DBGLN("  [POLL] Backend no responde");
    return;
  }

  // Aplicar config al LED
  DBGF("  [POLL] Config: pwr=%s color=%s bri=%d\n",
       cfg.manualPowerState, cfg.manualColorHex, cfg.manualBrightness);
  led.applyConfig(cfg.manualPowerState, cfg.manualColorHex, cfg.manualBrightness);

  // WiFi pendiente
  if (hasPending && strlen(pendSsid) > 0) {
    DBGF("  [POLL] WiFi pendiente: \"%s\"\n", pendSsid);
    _checkPendingWifi();
  }

  // Re-escaneo periodico
  if (pollCount % WIFI_RESCAN_EVERY_N_POLLS == 0) {
    DBGLN("  [POLL] Re-escaneo WiFi periodico");
    _scanAndPublishNetworks();
  }
}

// =====================================================================
//  PIR MOTION
// =====================================================================

void _handleMotion() {
  DBGF("  [PIR] Movimiento detectado (uptime %lus)\n", millis() / 1000);

  if (wifi.isConnected()) {
    backend.logEvent(macAddress.c_str(), "MOTION_DETECTED");
  }

  if (strcmp(cfg.motionMode, "BLINK") == 0 && cfg.motionBlinkSeconds > 0) {
    DBGF("  [PIR] Blink %ds color %s\n", cfg.motionBlinkSeconds, cfg.manualColorHex);
    led.blink(cfg.motionBlinkSeconds, cfg.manualColorHex);
  }
}

// =====================================================================
//  SCHEDULE
// =====================================================================

void _evaluateSchedule() {
  bool shouldBeOn = rtc.isWithinSchedule(cfg.scheduleStartTime, cfg.scheduleEndTime);

  if (shouldBeOn && !scheduleCurrentlyOn) {
    DBGF("  [SCHED] INICIO %s-%s color=%s bri=%d\n",
         cfg.scheduleStartTime, cfg.scheduleEndTime,
         cfg.scheduleColorHex, cfg.scheduleBrightness);
    led.setColor(cfg.scheduleColorHex, (uint8_t)cfg.scheduleBrightness);
    scheduleCurrentlyOn = true;
  } else if (!shouldBeOn && scheduleCurrentlyOn) {
    DBGF("  [SCHED] FIN %s-%s apagando\n",
         cfg.scheduleStartTime, cfg.scheduleEndTime);
    led.turnOff();
    scheduleCurrentlyOn = false;
  }
}

// =====================================================================
//  WIFI RECONNECTION (3-phase)
// =====================================================================

void _attemptReconnect() {
  _section("RECONEXION WiFi");
  DBGF("  Intento        #%d\n", reconnectFailCount + 1);
  DBGF("  Uptime         %lus\n", millis() / 1000);
  DBGF("  Red operativa  \"%s\"\n",
       strlen(cfg.operationalSSID) > 0 ? cfg.operationalSSID : "(ninguna)");
  DBGF("  Backoff        %lums\n", WiFiManager::getBackoffInterval(reconnectFailCount));

  bool ok = wifi.attemptRecovery(
    cfg.operationalSSID,
    cfg.operationalPassword,
    reconnectFailCount,
    cfg.recoveryBootstrapEveryNCycles
  );

  if (ok) {
    if (wifi.state == WiFiState::OPERATIONAL_CONNECTED) {
      DBGF("  Reconexion OPERATIVA  IP: %s\n", wifi.localIP().c_str());
      reconnectFailCount = 0;
      backend.updateStatus(macAddress.c_str(), "ONLINE", cfg.operationalSSID, wifi.localIP().c_str());
      backend.bootstrap(macAddress.c_str(), wifi.localIP().c_str(), cfg);
    } else if (wifi.state == WiFiState::RECOVERY_MODE) {
      DBGF("  Bootstrap RECOVERY  IP: %s\n", wifi.localIP().c_str());
      backend.updateStatus(macAddress.c_str(), "RECOVERY_BOOTSTRAP_CONNECTED", "", wifi.localIP().c_str());
      DBGLN("  Re-escaneando redes...");
      _scanAndPublishNetworks();
      _checkPendingWifi();
    }
  } else {
    DBGF("  Reconexion fallida (fallos: %d)\n", reconnectFailCount);
    unsigned long nextBackoff = WiFiManager::getBackoffInterval(reconnectFailCount);
    DBGF("  Proximo intento en %lus\n", nextBackoff / 1000);
    if (reconnectFailCount == INITIAL_RETRY_COUNT) {
      DBGLN("  Fase A agotada. Modo OFFLINE_LOCAL.");
    }
  }
  _line();
}
