// ============================================================
// BackendClient.h — Cliente HTTP para Google Apps Script
// IoT Lámpara WiFi v10
// ============================================================
#pragma once
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "Storage.h"

class BackendClient {
public:
  // ─── Bootstrap: registro / heartbeat ─────────────────────
  bool bootstrap(const char* mac, const char* ip, DeviceConfig& cfg) {
    StaticJsonDocument<256> body;
    body["mac_address"]       = mac;
    body["firmware_version"]  = FIRMWARE_VERSION;
    body["ip"]                = ip;
    body["current_wifi_ssid"] = WiFi.SSID();

    StaticJsonDocument<1024> resp;
    if (!_post("bootstrap", body, resp)) return false;

    if (resp["success"].as<bool>()) {
      JsonObject config = resp["config"];
      _applyJsonConfig(cfg, config);
      Storage::save(cfg);
      return true;
    }
    return false;
  }

  // ─── Publicar escaneo WiFi ────────────────────────────────
  bool publishScan(const char* mac, JsonArray& networks) {
    StaticJsonDocument<1024> body;
    body["mac"]     = mac;
    body["scan_ts"] = "";          // Apps Script usará now()
    body["networks"]= networks;

    StaticJsonDocument<256> resp;
    return _post("wifi_scan", body, resp) && resp["success"].as<bool>();
  }

  // ─── Obtener configuración pendiente (polling 30s D1) ─────
  bool getPendingConfig(const char* mac, DeviceConfig& cfg,
                        char* wifiSsidOut, char* wifiPassOut, bool& hasPendingWifi) {
    String url = String(BACKEND_URL) + "?action=pending_config&mac=" + mac;
    StaticJsonDocument<1024> resp;
    if (!_get(url, resp)) return false;
    if (!resp["success"].as<bool>()) return false;

    // Configuración del dispositivo
    JsonObject devCfg = resp["device_config"];
    if (!devCfg.isNull()) {
      _applyJsonConfig(cfg, devCfg);
      Storage::save(cfg);
    }

    // WiFi pendiente
    hasPendingWifi = false;
    JsonObject wifiCfg = resp["wifi_config"];
    if (!wifiCfg.isNull() && strcmp(wifiCfg["status"] | "", "PENDING") == 0) {
      strlcpy(wifiSsidOut, wifiCfg["ssid"] | "", 64);
      strlcpy(wifiPassOut, wifiCfg["password"] | "", 64);
      hasPendingWifi = true;
    }
    return true;
  }

  // ─── Reportar estado ─────────────────────────────────────
  bool updateStatus(const char* mac, const char* status, const char* ssid, const char* ip,
                    const char* failReason = nullptr) {
    StaticJsonDocument<256> body;
    body["mac"]              = mac;
    body["status"]           = status;
    body["current_wifi_ssid"]= ssid;
    body["ip"]               = ip;
    if (failReason) body["fail_reason"] = failReason;

    StaticJsonDocument<256> resp;
    return _post("update_status", body, resp);
  }

  // ─── Registrar evento ────────────────────────────────────
  bool logEvent(const char* mac, const char* eventType, const char* value = "") {
    StaticJsonDocument<256> body;
    body["mac"]        = mac;
    body["event_type"] = eventType;
    body["event_value"]= value;
    body["source"]     = "DEVICE";

    StaticJsonDocument<256> resp;
    return _post("log_event", body, resp);
  }

private:
  // POST con body JSON, parsea respuesta en respDoc
  bool _post(const char* action, JsonDocument& body, JsonDocument& respDoc) {
    HTTPClient http;
    String url = String(BACKEND_URL) + "?action=" + action;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    String bodyStr;
    serializeJson(body, bodyStr);

    int code = http.POST(bodyStr);
    if (code < 0) {
      DBGF("    [HTTP] POST %s error=%d\n", action, code);
      http.end();
      return false;
    }

    String payload = http.getString();
    http.end();

    if (code != 200) {
      DBGF("    [HTTP] POST %s cod=%d\n", action, code);
    }

    DeserializationError err = deserializeJson(respDoc, payload);
    if (err != DeserializationError::Ok) {
      DBGF("    [HTTP] POST %s JSON error: %s\n", action, err.c_str());
      // Show first 80 chars of response for diagnosis
      String preview = payload.substring(0, 80);
      DBGF("    [HTTP] Resp: %s...\n", preview.c_str());
      return false;
    }
    return true;
  }

  // GET a URL completa, parsea respuesta en respDoc
  bool _get(const String& url, JsonDocument& respDoc) {
    HTTPClient http;
    http.begin(url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int code = http.GET();
    if (code < 0) {
      DBGF("    [HTTP] GET error=%d\n", code);
      http.end();
      return false;
    }

    String payload = http.getString();
    http.end();

    if (code != 200) {
      DBGF("    [HTTP] GET cod=%d\n", code);
    }

    DeserializationError err = deserializeJson(respDoc, payload);
    if (err != DeserializationError::Ok) {
      DBGF("    [HTTP] GET JSON error: %s\n", err.c_str());
      String preview = payload.substring(0, 80);
      DBGF("    [HTTP] Resp: %s...\n", preview.c_str());
      return false;
    }
    return true;
  }

  void _applyJsonConfig(DeviceConfig& cfg, JsonObject& obj) {
    if (obj.containsKey("manual_power_state"))                strlcpy(cfg.manualPowerState, obj["manual_power_state"] | "", sizeof(cfg.manualPowerState));
    if (obj.containsKey("manual_color_hex"))                  strlcpy(cfg.manualColorHex,   obj["manual_color_hex"]   | "", sizeof(cfg.manualColorHex));
    if (obj.containsKey("manual_brightness"))                 cfg.manualBrightness            = obj["manual_brightness"]   | 200;
    if (obj.containsKey("motion_mode"))                       strlcpy(cfg.motionMode,        obj["motion_mode"]        | "", sizeof(cfg.motionMode));
    if (obj.containsKey("motion_blink_seconds"))              cfg.motionBlinkSeconds          = obj["motion_blink_seconds"]  | 5;
    if (obj.containsKey("schedule_enabled"))                  cfg.scheduleEnabled             = obj["schedule_enabled"]      | false;
    if (obj.containsKey("schedule_start_time"))               strlcpy(cfg.scheduleStartTime, obj["schedule_start_time"] | "", sizeof(cfg.scheduleStartTime));
    if (obj.containsKey("schedule_end_time"))                 strlcpy(cfg.scheduleEndTime,   obj["schedule_end_time"]   | "", sizeof(cfg.scheduleEndTime));
    if (obj.containsKey("schedule_color_hex"))                strlcpy(cfg.scheduleColorHex,  obj["schedule_color_hex"]  | "", sizeof(cfg.scheduleColorHex));
    if (obj.containsKey("schedule_brightness"))               cfg.scheduleBrightness          = obj["schedule_brightness"]  | 200;
    if (obj.containsKey("reconnect_interval_seconds"))        cfg.reconnectIntervalSeconds    = obj["reconnect_interval_seconds"] | 60;
    if (obj.containsKey("recovery_bootstrap_every_n_cycles")) cfg.recoveryBootstrapEveryNCycles = obj["recovery_bootstrap_every_n_cycles"] | 5;
  }
};
