// ============================================================
// minecraft_lamp.ino — Loop cooperativo principal
// IoT Lámpara WiFi v10
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

// ─── Setup ───────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  DBGLN("\n╔══════════════════════════════╗");
  DBGLN("║  MineCraft Lamp v10          ║");
  DBGLN("╚══════════════════════════════╝");

  // 1. Inicializar hardware
  led.begin();
  pir.begin();
  rtc.begin();

  // 2. Cargar configuración persistida en NVS
  Storage::init();
  Storage::load(cfg);
  DBGLN("[SETUP] Configuración NVS cargada.");

  // 3. Obtener MAC
  macAddress = wifi.getMac();
  DBGF("[SETUP] MAC: %s\n", macAddress.c_str());

  // 4. Iniciar WatchDog
  wdt.begin();

  // 5. Señal visual de arranque: parpadeo breve blanco
  led.blink(1, "#FFFFFF");

  // 6. Arranque: intentar conectar a red operativa primero (si existe)
  bool connectedToOperational = false;
  if (strlen(cfg.operationalSSID) > 0) {
    DBGLN("[SETUP] Intentando red operativa guardada...");
    connectedToOperational = wifi.connectOperational(cfg.operationalSSID, cfg.operationalPassword);
  }

  if (!connectedToOperational) {
    // 7. Conectar a bootstrap
    DBGLN("[SETUP] Conectando a bootstrap...");
    if (wifi.connectBootstrap()) {
      _doBootstrapSequence();
    } else {
      DBGLN("[SETUP] Bootstrap no disponible. Modo local.");
      wifi.state = WiFiState::RECOVERY_MODE;
    }
  } else {
    // Conectado a operativa desde arranque: solo hacer heartbeat
    backend.bootstrap(macAddress.c_str(), wifi.localIP().c_str(), cfg);
    // Informar restart por WDT si aplica
    if (WatchDog::wasWatchdogRestart()) {
      backend.updateStatus(macAddress.c_str(), "WATCHDOG_RESTART", cfg.operationalSSID, wifi.localIP().c_str());
      WatchDog::clearRestartFlag();
    }
  }

  // 8. Aplicar último estado conocido al LED
  led.applyConfig(cfg.manualPowerState, cfg.manualColorHex, cfg.manualBrightness);
  DBGLN("[SETUP] Setup completo. Iniciando loop.");
}

// ─── Loop principal cooperativo (D5) ─────────────────────────
void loop() {
  wdt.feed(); // ← Siempre primero

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
    unsigned long reconnInterval = (unsigned long)cfg.reconnectIntervalSeconds * 1000UL;
    if (now - lastReconnectMs >= reconnInterval) {
      lastReconnectMs = now;
      _attemptReconnect();
    }
  }

  delay(50); // Pequeña pausa no bloqueante
}

// ─── Secuencia de bootstrap ───────────────────────────────────
void _doBootstrapSequence() {
  // Sincronizar RTC con NTP (D4)
  rtc.syncWithNTP();

  // Registrar / actualizar presencia en backend
  backend.bootstrap(macAddress.c_str(), wifi.localIP().c_str(), cfg);

  // Informar WDT restart si aplica
  if (WatchDog::wasWatchdogRestart()) {
    backend.updateStatus(macAddress.c_str(), "WATCHDOG_RESTART", "", wifi.localIP().c_str());
    WatchDog::clearRestartFlag();
  }

  // Escanear redes WiFi y publicar
  _scanAndPublishNetworks();

  // Verificar si hay credenciales operativas pendientes
  _checkPendingWifi();
}

// ─── Escaneo y publicación de redes WiFi ─────────────────────
void _scanAndPublishNetworks() {
  DBGLN("[SCAN] Escaneando redes...");
  int n = WiFi.scanNetworks();
  if (n <= 0) { DBGLN("[SCAN] Sin redes encontradas."); return; }

  StaticJsonDocument<1024> doc;
  JsonArray networks = doc.to<JsonArray>();
  for (int i = 0; i < min(n, 15); i++) {
    JsonObject net = networks.createNestedObject();
    net["ssid"] = WiFi.SSID(i);
    net["rssi"] = WiFi.RSSI(i);
    net["auth"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "OPEN" : "SECURED";
  }

  backend.publishScan(macAddress.c_str(), networks);
  WiFi.scanDelete();
}

// ─── Verificar credenciales WiFi pendientes ───────────────────
void _checkPendingWifi() {
  char pendSsid[64] = {0};
  char pendPass[64] = {0};
  bool hasPending   = false;

  backend.getPendingConfig(macAddress.c_str(), cfg, pendSsid, pendPass, hasPending);

  if (hasPending && strlen(pendSsid) > 0) {
    DBGF("[WiFi] Credencial pendiente: %s\n", pendSsid);
    backend.updateStatus(macAddress.c_str(), "WIFI_CONNECTING", pendSsid, wifi.localIP().c_str());

    if (wifi.connectOperational(pendSsid, pendPass)) {
      // Éxito: guardar red operativa
      strlcpy(cfg.operationalSSID,     pendSsid, sizeof(cfg.operationalSSID));
      strlcpy(cfg.operationalPassword, pendPass, sizeof(cfg.operationalPassword));
      Storage::save(cfg);
      reconnectFailCount = 0;

      backend.updateStatus(macAddress.c_str(), "WIFI_CONNECTED", pendSsid, wifi.localIP().c_str());
      backend.bootstrap(macAddress.c_str(), wifi.localIP().c_str(), cfg); // re-sync config
    } else {
      // Fallo: reportar e intentar volver a bootstrap para publicar nuevo scan
      backend.updateStatus(macAddress.c_str(), "WIFI_FAILED", pendSsid, wifi.localIP().c_str(), "Auth failed");
      wifi.connectBootstrap();
      _scanAndPublishNetworks(); // publicar nuevas redes para que el usuario reintente
    }
  }
}

// ─── Polling periódico al backend (D1: cada 30s) ─────────────
void _pollBackend() {
  char pendSsid[64] = {0};
  char pendPass[64] = {0};
  bool hasPending   = false;

  bool ok = backend.getPendingConfig(macAddress.c_str(), cfg, pendSsid, pendPass, hasPending);
  if (!ok) { DBGLN("[POLL] Fallo al contactar backend."); return; }

  // Aplicar cambios de configuración al LED
  led.applyConfig(cfg.manualPowerState, cfg.manualColorHex, cfg.manualBrightness);

  // Si hay credenciales pendientes, intentar conectar
  if (hasPending && strlen(pendSsid) > 0) {
    _checkPendingWifi();
  }
}

// ─── Lógica PIR ───────────────────────────────────────────────
void _handleMotion() {
  // Registrar en backend si hay conectividad
  if (wifi.isConnected()) {
    backend.logEvent(macAddress.c_str(), "MOTION_DETECTED");
  }

  // Aplicar política de blink
  if (strcmp(cfg.motionMode, "BLINK") == 0 && cfg.motionBlinkSeconds > 0) {
    DBGF("[PIR] BLINK por %d segundos\n", cfg.motionBlinkSeconds);
    // Capturar estado previo (LEDControl::blink lo restaura internamente)
    led.blink(cfg.motionBlinkSeconds, cfg.manualColorHex);
  } else {
    DBGLN("[PIR] NO_BLINK: LED sin cambio.");
  }
}

// ─── Evaluación de programación horaria ───────────────────────
void _evaluateSchedule() {
  bool shouldBeOn = rtc.isWithinSchedule(cfg.scheduleStartTime, cfg.scheduleEndTime);

  if (shouldBeOn && !scheduleCurrentlyOn) {
    // Entrar en franja horaria → encender con color y brillo programado
    DBGF("[SCHED] Inicio horario: %s bri=%d\n", cfg.scheduleColorHex, cfg.scheduleBrightness);
    led.setColor(cfg.scheduleColorHex, (uint8_t)cfg.scheduleBrightness);
    scheduleCurrentlyOn = true;
  } else if (!shouldBeOn && scheduleCurrentlyOn) {
    // Salir de franja horaria → apagar
    DBGLN("[SCHED] Fin horario → apagando.");
    led.turnOff();
    scheduleCurrentlyOn = false;
  }
}

// ─── Reconexión WiFi escalonada ───────────────────────────────
void _attemptReconnect() {
  DBGF("[RECONN] Intentando reconexión (fallo #%d)\n", reconnectFailCount + 1);

  bool ok = wifi.attemptRecovery(
    cfg.operationalSSID,
    cfg.operationalPassword,
    reconnectFailCount,
    cfg.recoveryBootstrapEveryNCycles
  );

  if (ok) {
    if (wifi.state == WiFiState::OPERATIONAL_CONNECTED) {
      // Reconexión a red operativa exitosa
      backend.updateStatus(macAddress.c_str(), "ONLINE", cfg.operationalSSID, wifi.localIP().c_str());
      backend.bootstrap(macAddress.c_str(), wifi.localIP().c_str(), cfg);
    } else if (wifi.state == WiFiState::RECOVERY_MODE) {
      // Conectado a bootstrap en modo recuperación
      backend.updateStatus(macAddress.c_str(), "RECOVERY_BOOTSTRAP_CONNECTED", "", wifi.localIP().c_str());
      _scanAndPublishNetworks();
      _checkPendingWifi();
    }
  } else {
    // Todavía sin conexión: reportar modo recuperación (primera vez)
    if (reconnectFailCount == 1) {
      DBGLN("[RECONN] Entrando en OFFLINE_LOCAL_MODE.");
    }
  }
}
