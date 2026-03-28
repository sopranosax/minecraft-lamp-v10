// ============================================================
// minecraft_lamp.ino — Loop cooperativo principal
// IoT Lámpara WiFi v10  —  Firmware v1.1.1
//
// Hardware:
//   ESP32, NeoPixel WS2812B, PIR HC-SR501, RTC DS3231
//
// Librerías requeridas (instalar en Arduino IDE Library Manager):
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

// ─── Instancias globales ──────────────────────────────────────
LEDControl    led;
PIRSensor     pir;
RTCManager    rtc;
WatchDog      wdt;
WiFiManager   wifi;
BackendClient backend;
DeviceConfig  cfg;

// ─── Variables de estado del loop ─────────────────────────────
String  macAddress;
bool    scheduleCurrentlyOn    = false; // estado del horario programado
int     reconnectFailCount     = 0;
unsigned long lastPollingMs    = 0;
unsigned long lastReconnectMs  = 0;
unsigned long loopCount        = 0;

// ─── Helper: print a divider line ─────────────────────────────
void _printDivider(const char* label = nullptr) {
  DBGLN("────────────────────────────────────────");
  if (label) { DBGF("  %s\n", label); DBGLN("────────────────────────────────────────"); }
}

// ─── Helper: dump current config to serial ────────────────────
void _printConfig() {
  _printDivider("CONFIGURACION ACTIVA");
  DBGF("  Power       : %s\n", cfg.manualPowerState);
  DBGF("  Color       : %s\n", cfg.manualColorHex);
  DBGF("  Brightness  : %d / 255 (%d%%)\n", cfg.manualBrightness, cfg.manualBrightness * 100 / 255);
  DBGF("  Motion mode : %s\n", cfg.motionMode);
  DBGF("  Blink secs  : %d\n", cfg.motionBlinkSeconds);
  DBGF("  Schedule    : %s\n", cfg.scheduleEnabled ? "ON" : "OFF");
  if (cfg.scheduleEnabled) {
    DBGF("    Start     : %s\n", cfg.scheduleStartTime);
    DBGF("    End       : %s\n", cfg.scheduleEndTime);
    DBGF("    Color     : %s\n", cfg.scheduleColorHex);
    DBGF("    Brightness: %d / 255 (%d%%)\n", cfg.scheduleBrightness, cfg.scheduleBrightness * 100 / 255);
  }
  DBGF("  Reconn int  : %ds\n", cfg.reconnectIntervalSeconds);
  DBGF("  Recovery N  : cada %d ciclos\n", cfg.recoveryBootstrapEveryNCycles);
  DBGF("  Op. SSID    : %s\n", strlen(cfg.operationalSSID) > 0 ? cfg.operationalSSID : "(ninguna)");
  _printDivider();
}

// ─── Setup ───────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  DBGLN("");
  DBGLN("╔═══════════════════════════════════════╗");
  DBGLN("║                                       ║");
  DBGF( "║   MineCraft Lamp  — FW v%-14s║\n", FIRMWARE_VERSION);
  DBGLN("║   IoT NeoPixel + PIR + RTC            ║");
  DBGLN("║                                       ║");
  DBGLN("╚═══════════════════════════════════════╝");
  DBGLN("");

  // ── 1. Inicializar hardware ─────────────────────────────────
  _printDivider("PASO 1: HARDWARE");
  DBGF("  NeoPixel    : GPIO %d, %d LEDs\n", NEOPIXEL_PIN, NEOPIXEL_COUNT);
  led.begin();
  DBGLN("  NeoPixel    : OK ✓");

  DBGF("  PIR sensor  : GPIO %d\n", PIR_PIN);
  pir.begin();
  DBGLN("  PIR sensor  : OK ✓");

  DBGF("  RTC DS3231  : SDA=%d SCL=%d\n", RTC_SDA_PIN, RTC_SCL_PIN);
  bool rtcOk = rtc.begin();
  DBGF("  RTC DS3231  : %s\n", rtcOk ? "OK ✓" : "FALLO ✗");

  // ── 2. Cargar configuración NVS ─────────────────────────────
  _printDivider("PASO 2: NVS (FLASH)");
  Storage::init();
  Storage::load(cfg);
  DBGLN("  Config NVS  : Cargada ✓");
  _printConfig();

  // ── 3. Obtener MAC ──────────────────────────────────────────
  _printDivider("PASO 3: IDENTIDAD");
  macAddress = wifi.getMac();
  DBGF("  MAC address : %s\n", macAddress.c_str());
  DBGF("  Firmware    : v%s\n", FIRMWARE_VERSION);
  DBGF("  Free heap   : %d bytes\n", ESP.getFreeHeap());

  // ── 4. Iniciar WatchDog ─────────────────────────────────────
  _printDivider("PASO 4: WATCHDOG");
  wdt.begin();
  DBGF("  Timeout     : %ds\n", WATCHDOG_TIMEOUT_S);
  if (WatchDog::wasWatchdogRestart()) {
    DBGLN("  ⚠ REINICIO PREVIO POR WATCHDOG DETECTADO");
  } else {
    DBGLN("  Arranque    : Normal ✓");
  }

  // ── 5. Señal visual ─────────────────────────────────────────
  DBGLN("");
  DBGLN("[LED] Parpadeo de arranque (blanco)...");
  led.blink(1, "#FFFFFF");
  DBGLN("[LED] Listo.");

  // ── 6. Conectar WiFi ────────────────────────────────────────
  _printDivider("PASO 5: CONEXION WiFi");
  bool connectedToOperational = false;

  if (strlen(cfg.operationalSSID) > 0) {
    DBGF("  Red guardada: \"%s\"\n", cfg.operationalSSID);
    DBGLN("  Intentando conexión operativa...");
    connectedToOperational = wifi.connectOperational(cfg.operationalSSID, cfg.operationalPassword);
    DBGF("  Resultado   : %s\n", connectedToOperational ? "CONECTADO ✓" : "FALLO ✗");
  } else {
    DBGLN("  No hay red operativa guardada.");
  }

  if (!connectedToOperational) {
    DBGF("  Intentando bootstrap: \"%s\"...\n", BOOTSTRAP_SSID);
    if (wifi.connectBootstrap()) {
      DBGF("  Bootstrap   : CONECTADO ✓  IP: %s\n", wifi.localIP().c_str());
      _printDivider("PASO 6: SECUENCIA BOOTSTRAP");
      _doBootstrapSequence();
    } else {
      DBGLN("  Bootstrap   : NO DISPONIBLE ✗");
      DBGLN("  → Entrando en modo LOCAL (offline).");
      wifi.state = WiFiState::RECOVERY_MODE;
    }
  } else {
    DBGF("  IP          : %s\n", wifi.localIP().c_str());
    DBGF("  SSID        : %s\n", WiFi.SSID().c_str());
    DBGLN("  Enviando bootstrap al backend...");
    bool bsOk = backend.bootstrap(macAddress.c_str(), wifi.localIP().c_str(), cfg);
    DBGF("  Backend     : %s\n", bsOk ? "OK ✓" : "FALLO ✗");

    if (WatchDog::wasWatchdogRestart()) {
      DBGLN("  Reportando reinicio WDT al backend...");
      backend.updateStatus(macAddress.c_str(), "WATCHDOG_RESTART", cfg.operationalSSID, wifi.localIP().c_str());
      WatchDog::clearRestartFlag();
      DBGLN("  WDT flag    : Limpiada ✓");
    }
  }

  // ── 7. Aplicar estado LED ───────────────────────────────────
  _printDivider("PASO 7: APLICAR ESTADO LED");
  DBGF("  Power=%s  Color=%s  Bri=%d\n", cfg.manualPowerState, cfg.manualColorHex, cfg.manualBrightness);
  led.applyConfig(cfg.manualPowerState, cfg.manualColorHex, cfg.manualBrightness);
  DBGLN("  LED aplicado ✓");

  // ── Setup completo ──────────────────────────────────────────
  DBGLN("");
  DBGLN("╔═══════════════════════════════════════╗");
  DBGF( "║  SETUP COMPLETO  — FW v%-14s ║\n", FIRMWARE_VERSION);
  DBGF( "║  MAC: %-32s║\n", macAddress.c_str());
  DBGF( "║  WiFi: %-18s  IP: %-10s║\n",
        wifi.isConnected() ? WiFi.SSID().c_str() : "OFFLINE",
        wifi.isConnected() ? wifi.localIP().c_str() : "—");
  DBGLN("╚═══════════════════════════════════════╝");
  DBGLN("");
  DBGLN("[LOOP] Iniciando loop principal...");
  DBGF( "[LOOP] Polling cada %lus  |  PIR debounce %lus  |  Reconn cada %ds\n",
        POLLING_INTERVAL_MS / 1000, PIR_DEBOUNCE_MS / 1000, cfg.reconnectIntervalSeconds);
  _printDivider();
}

// ─── Loop principal cooperativo (D5) ─────────────────────────
void loop() {
  wdt.feed(); // ← Siempre primero
  loopCount++;

  // ──── 1. Revisar PIR ─────────────────────────────────────
  if (pir.check()) {
    _handleMotion();
  }

  // ──── 2. Evaluar programación horaria ────────────────────
  if (cfg.scheduleEnabled && strlen(cfg.scheduleStartTime) > 0) {
    _evaluateSchedule();
  }

  // ──── 3. Polling al backend (cada 30s D1) ────────────────
  unsigned long now = millis();
  if (wifi.isConnected() && (now - lastPollingMs >= POLLING_INTERVAL_MS)) {
    lastPollingMs = now;
    _pollBackend();
  }

  // ──── 4. Reconexión WiFi si está offline ─────────────────
  if (!wifi.isConnected()) {
    unsigned long reconnInterval = WiFiManager::getBackoffInterval(reconnectFailCount);
    if (now - lastReconnectMs >= reconnInterval) {
      lastReconnectMs = now;
      DBGF("[LOOP] Backoff interval: %lums (fallo #%d)\n", reconnInterval, reconnectFailCount);
      _attemptReconnect();
    }
  }

  delay(50); // Pequeña pausa no bloqueante
}

// ─── Secuencia de bootstrap ───────────────────────────────────
void _doBootstrapSequence() {
  // Sincronizar RTC con NTP (D4)
  DBGLN("[BOOTSTRAP] 1/4 Sincronizando RTC con NTP...");
  rtc.syncWithNTP();

  // Registrar / actualizar presencia en backend
  DBGLN("[BOOTSTRAP] 2/4 Enviando bootstrap al backend...");
  bool bsOk = backend.bootstrap(macAddress.c_str(), wifi.localIP().c_str(), cfg);
  DBGF("[BOOTSTRAP]     Resultado: %s\n", bsOk ? "OK ✓" : "FALLO ✗");

  // Informar WDT restart si aplica
  if (WatchDog::wasWatchdogRestart()) {
    DBGLN("[BOOTSTRAP]     Reportando reinicio WDT...");
    backend.updateStatus(macAddress.c_str(), "WATCHDOG_RESTART", "", wifi.localIP().c_str());
    WatchDog::clearRestartFlag();
    DBGLN("[BOOTSTRAP]     WDT flag limpiada ✓");
  }

  // Escanear redes WiFi y publicar
  DBGLN("[BOOTSTRAP] 3/4 Escaneando redes WiFi...");
  _scanAndPublishNetworks();

  // Verificar si hay credenciales operativas pendientes
  DBGLN("[BOOTSTRAP] 4/4 Verificando credenciales WiFi pendientes...");
  _checkPendingWifi();
  DBGLN("[BOOTSTRAP] Secuencia completa ✓");
}

// ─── Escaneo y publicación de redes WiFi ─────────────────────
void _scanAndPublishNetworks() {
  DBGLN("[SCAN] Escaneando redes cercanas...");
  int n = WiFi.scanNetworks();
  if (n <= 0) {
    DBGLN("[SCAN] Sin redes encontradas.");
    return;
  }
  DBGF("[SCAN] %d redes encontradas:\n", n);
  for (int i = 0; i < min(n, 15); i++) {
    DBGF("[SCAN]   %2d. %-24s  %d dBm  %s\n",
         i + 1,
         WiFi.SSID(i).c_str(),
         WiFi.RSSI(i),
         (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "OPEN" : "SECURED");
  }

  StaticJsonDocument<1024> doc;
  JsonArray networks = doc.to<JsonArray>();
  for (int i = 0; i < min(n, 15); i++) {
    JsonObject net = networks.createNestedObject();
    net["ssid"] = WiFi.SSID(i);
    net["rssi"] = WiFi.RSSI(i);
    net["auth"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "OPEN" : "SECURED";
  }

  DBGLN("[SCAN] Publicando al backend...");
  bool ok = backend.publishScan(macAddress.c_str(), networks);
  DBGF("[SCAN] Publicación: %s\n", ok ? "OK ✓" : "FALLO ✗");
  WiFi.scanDelete();
}

// ─── Verificar credenciales WiFi pendientes ───────────────────
void _checkPendingWifi() {
  char pendSsid[64] = {0};
  char pendPass[64] = {0};
  bool hasPending   = false;

  backend.getPendingConfig(macAddress.c_str(), cfg, pendSsid, pendPass, hasPending);

  if (hasPending && strlen(pendSsid) > 0) {
    DBGF("[WIFI-CRED] Credencial pendiente encontrada: \"%s\"\n", pendSsid);
    DBGLN("[WIFI-CRED] Reportando WIFI_CONNECTING...");
    backend.updateStatus(macAddress.c_str(), "WIFI_CONNECTING", pendSsid, wifi.localIP().c_str());

    DBGF("[WIFI-CRED] Intentando conexión a \"%s\"...\n", pendSsid);
    if (wifi.connectOperational(pendSsid, pendPass)) {
      // Éxito: guardar red operativa
      DBGF("[WIFI-CRED] CONECTADO ✓  IP: %s\n", wifi.localIP().c_str());
      strlcpy(cfg.operationalSSID,     pendSsid, sizeof(cfg.operationalSSID));
      strlcpy(cfg.operationalPassword, pendPass, sizeof(cfg.operationalPassword));
      Storage::save(cfg);
      reconnectFailCount = 0;
      DBGLN("[WIFI-CRED] Credenciales guardadas en NVS ✓");

      DBGLN("[WIFI-CRED] Reportando WIFI_CONNECTED al backend...");
      backend.updateStatus(macAddress.c_str(), "WIFI_CONNECTED", pendSsid, wifi.localIP().c_str());
      DBGLN("[WIFI-CRED] Re-sincronizando config con bootstrap...");
      backend.bootstrap(macAddress.c_str(), wifi.localIP().c_str(), cfg);
      DBGLN("[WIFI-CRED] Conexión operativa completa ✓");
    } else {
      DBGF("[WIFI-CRED] FALLO al conectar a \"%s\" ✗\n", pendSsid);
      DBGLN("[WIFI-CRED] Reportando WIFI_FAILED...");
      backend.updateStatus(macAddress.c_str(), "WIFI_FAILED", pendSsid, wifi.localIP().c_str(), "Auth failed");
      DBGLN("[WIFI-CRED] Volviendo a bootstrap para re-escanear...");
      wifi.connectBootstrap();
      _scanAndPublishNetworks();
    }
  } else {
    DBGLN("[WIFI-CRED] No hay credenciales pendientes.");
  }
}

// ─── Polling periódico al backend (D1: cada 30s) ─────────────
void _pollBackend() {
  DBGF("[POLL] Consultando backend... (loop #%lu, uptime %lus)\n", loopCount, millis() / 1000);

  char pendSsid[64] = {0};
  char pendPass[64] = {0};
  bool hasPending   = false;

  bool ok = backend.getPendingConfig(macAddress.c_str(), cfg, pendSsid, pendPass, hasPending);
  if (!ok) {
    DBGLN("[POLL] Fallo al contactar backend ✗");
    return;
  }
  DBGLN("[POLL] Backend respondió OK ✓");

  // Aplicar cambios de configuración al LED
  DBGF("[POLL] Aplicando config: power=%s color=%s bri=%d\n",
       cfg.manualPowerState, cfg.manualColorHex, cfg.manualBrightness);
  led.applyConfig(cfg.manualPowerState, cfg.manualColorHex, cfg.manualBrightness);

  // Si hay credenciales pendientes, intentar conectar
  if (hasPending && strlen(pendSsid) > 0) {
    DBGF("[POLL] ¡Credenciales WiFi pendientes! SSID: \"%s\"\n", pendSsid);
    _checkPendingWifi();
  }
}

// ─── Lógica PIR ───────────────────────────────────────────────
void _handleMotion() {
  DBGF("[PIR] ¡MOVIMIENTO DETECTADO! (uptime %lus)\n", millis() / 1000);

  // Registrar en backend si hay conectividad
  if (wifi.isConnected()) {
    DBGLN("[PIR] Enviando evento MOTION_DETECTED al backend...");
    bool ok = backend.logEvent(macAddress.c_str(), "MOTION_DETECTED");
    DBGF("[PIR] Registro: %s\n", ok ? "OK ✓" : "FALLO ✗");
  } else {
    DBGLN("[PIR] Sin WiFi — evento no registrado en backend.");
  }

  // Aplicar política de blink
  if (strcmp(cfg.motionMode, "BLINK") == 0 && cfg.motionBlinkSeconds > 0) {
    DBGF("[PIR] Modo BLINK: parpadeando %ds con color %s\n", cfg.motionBlinkSeconds, cfg.manualColorHex);
    led.blink(cfg.motionBlinkSeconds, cfg.manualColorHex);
    DBGLN("[PIR] Blink terminado, LED restaurado.");
  } else {
    DBGLN("[PIR] Modo NO_BLINK: LED sin cambio.");
  }
}

// ─── Evaluación de programación horaria ───────────────────────
void _evaluateSchedule() {
  bool shouldBeOn = rtc.isWithinSchedule(cfg.scheduleStartTime, cfg.scheduleEndTime);

  if (shouldBeOn && !scheduleCurrentlyOn) {
    // Entrar en franja horaria → encender con color y brillo programado
    DBGF("[SCHED] ► INICIO horario: %s→%s  color=%s  bri=%d (%d%%)\n",
         cfg.scheduleStartTime, cfg.scheduleEndTime,
         cfg.scheduleColorHex, cfg.scheduleBrightness,
         cfg.scheduleBrightness * 100 / 255);
    led.setColor(cfg.scheduleColorHex, (uint8_t)cfg.scheduleBrightness);
    scheduleCurrentlyOn = true;
  } else if (!shouldBeOn && scheduleCurrentlyOn) {
    // Salir de franja horaria → apagar
    DBGF("[SCHED] ■ FIN horario: %s→%s  → Apagando LED\n",
         cfg.scheduleStartTime, cfg.scheduleEndTime);
    led.turnOff();
    scheduleCurrentlyOn = false;
  }
}

// ─── Reconexión WiFi escalonada ───────────────────────────────
void _attemptReconnect() {
  _printDivider("RECONEXION WiFi");
  DBGF("  Intento     : #%d\n", reconnectFailCount + 1);
  DBGF("  Uptime      : %lus\n", millis() / 1000);
  DBGF("  Red operativa: \"%s\"\n",
       strlen(cfg.operationalSSID) > 0 ? cfg.operationalSSID : "(ninguna)");
  DBGF("  Backoff     : %lums\n", WiFiManager::getBackoffInterval(reconnectFailCount));

  bool ok = wifi.attemptRecovery(
    cfg.operationalSSID,
    cfg.operationalPassword,
    reconnectFailCount,
    cfg.recoveryBootstrapEveryNCycles
  );

  if (ok) {
    if (wifi.state == WiFiState::OPERATIONAL_CONNECTED) {
      DBGF("  ✓ Reconexión OPERATIVA exitosa  IP: %s\n", wifi.localIP().c_str());
      reconnectFailCount = 0;
      backend.updateStatus(macAddress.c_str(), "ONLINE", cfg.operationalSSID, wifi.localIP().c_str());
      backend.bootstrap(macAddress.c_str(), wifi.localIP().c_str(), cfg);
      DBGLN("  Backend notificado ✓");
    } else if (wifi.state == WiFiState::RECOVERY_MODE) {
      DBGF("  ⚠ Bootstrap de RECOVERY conectado  IP: %s\n", wifi.localIP().c_str());
      backend.updateStatus(macAddress.c_str(), "RECOVERY_BOOTSTRAP_CONNECTED", "", wifi.localIP().c_str());
      DBGLN("  Re-escaneando redes...");
      _scanAndPublishNetworks();
      _checkPendingWifi();
    }
  } else {
    DBGF("  ✗ Reconexión fallida (fallos acumulados: %d)\n", reconnectFailCount);
    unsigned long nextBackoff = WiFiManager::getBackoffInterval(reconnectFailCount);
    DBGF("  Próximo intento en %lus\n", nextBackoff / 1000);
    if (reconnectFailCount == INITIAL_RETRY_COUNT) {
      DBGLN("  → Fase A agotada. Entrando en modo OFFLINE_LOCAL. Operando sin backend.");
    }
  }
  _printDivider();
}
