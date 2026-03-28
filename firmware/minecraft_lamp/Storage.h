// ============================================================
// Storage.h — Persistencia local NVS (Preferences)
// IoT Lámpara WiFi v10
// ============================================================
#pragma once
#include <Preferences.h>
#include "Config.h"

// Estructura que contiene toda la configuración operativa local
struct DeviceConfig {
  char  operationalSSID[64];
  char  operationalPassword[64];
  char  manualPowerState[4];   // "ON" o "OFF"
  char  manualColorHex[8];     // "#RRGGBB"
  int   manualBrightness;      // 0-255
  char  motionMode[10];        // "BLINK" o "NO_BLINK"
  int   motionBlinkSeconds;
  bool  scheduleEnabled;
  char  scheduleStartTime[6];  // "HH:MM"
  char  scheduleEndTime[6];
  char  scheduleColorHex[8];
  int   scheduleBrightness;    // 0-255
  int   reconnectIntervalSeconds;
  int   recoveryBootstrapEveryNCycles;
  bool  watchdogRestartFlag;
};

class Storage {
public:
  static void init() {
    prefs.begin("lamp", false);
  }

  static void load(DeviceConfig& cfg) {
    strlcpy(cfg.operationalSSID,              prefs.getString("op_ssid",   "").c_str(),  sizeof(cfg.operationalSSID));
    strlcpy(cfg.operationalPassword,          prefs.getString("op_pass",   "").c_str(),  sizeof(cfg.operationalPassword));
    strlcpy(cfg.manualPowerState,             prefs.getString("pwr",       "OFF").c_str(),sizeof(cfg.manualPowerState));
    strlcpy(cfg.manualColorHex,               prefs.getString("color",     "#FFFFFF").c_str(),sizeof(cfg.manualColorHex));
    cfg.manualBrightness                    = prefs.getInt("bri_m",      200);
    strlcpy(cfg.motionMode,                   prefs.getString("motion",    "NO_BLINK").c_str(),sizeof(cfg.motionMode));
    cfg.motionBlinkSeconds                  = prefs.getInt("blink_s",     5);
    cfg.scheduleEnabled                     = prefs.getBool("sched_en",   false);
    strlcpy(cfg.scheduleStartTime,            prefs.getString("sched_st",  "").c_str(),  sizeof(cfg.scheduleStartTime));
    strlcpy(cfg.scheduleEndTime,              prefs.getString("sched_en_t","").c_str(),  sizeof(cfg.scheduleEndTime));
    strlcpy(cfg.scheduleColorHex,             prefs.getString("sched_col", "#FFFFFF").c_str(),sizeof(cfg.scheduleColorHex));
    cfg.scheduleBrightness                  = prefs.getInt("bri_s",      200);
    cfg.reconnectIntervalSeconds            = prefs.getInt("reconn_s",    60);
    cfg.recoveryBootstrapEveryNCycles       = prefs.getInt("rec_boot_n",  RECOVERY_BOOTSTRAP_DEFAULT);
    cfg.watchdogRestartFlag                 = prefs.getBool("wdt_restart", false);
  }

  static void save(const DeviceConfig& cfg) {
    prefs.putString("op_ssid",    cfg.operationalSSID);
    prefs.putString("op_pass",    cfg.operationalPassword);
    prefs.putString("pwr",        cfg.manualPowerState);
    prefs.putString("color",      cfg.manualColorHex);
    prefs.putInt(   "bri_m",      cfg.manualBrightness);
    prefs.putString("motion",     cfg.motionMode);
    prefs.putInt(   "blink_s",    cfg.motionBlinkSeconds);
    prefs.putBool(  "sched_en",   cfg.scheduleEnabled);
    prefs.putString("sched_st",   cfg.scheduleStartTime);
    prefs.putString("sched_en_t", cfg.scheduleEndTime);
    prefs.putString("sched_col",  cfg.scheduleColorHex);
    prefs.putInt(   "bri_s",      cfg.scheduleBrightness);
    prefs.putInt(   "reconn_s",   cfg.reconnectIntervalSeconds);
    prefs.putInt(   "rec_boot_n", cfg.recoveryBootstrapEveryNCycles);
  }

  static void setWatchdogFlag(bool val) { prefs.putBool("wdt_restart", val); }
  static bool getWatchdogFlag()          { return prefs.getBool("wdt_restart", false); }
  static void clearWatchdogFlag()        { prefs.putBool("wdt_restart", false); }

  static void applyRemoteConfig(DeviceConfig& cfg, const char* key, const char* value) {
    if (strcmp(key, "manual_power_state") == 0)                strlcpy(cfg.manualPowerState, value, sizeof(cfg.manualPowerState));
    else if (strcmp(key, "manual_color_hex") == 0)             strlcpy(cfg.manualColorHex,   value, sizeof(cfg.manualColorHex));
    else if (strcmp(key, "manual_brightness") == 0)            cfg.manualBrightness           = atoi(value);
    else if (strcmp(key, "motion_mode") == 0)                  strlcpy(cfg.motionMode,        value, sizeof(cfg.motionMode));
    else if (strcmp(key, "motion_blink_seconds") == 0)         cfg.motionBlinkSeconds         = atoi(value);
    else if (strcmp(key, "schedule_enabled") == 0)             cfg.scheduleEnabled            = (strcmp(value,"true")==0);
    else if (strcmp(key, "schedule_start_time") == 0)          strlcpy(cfg.scheduleStartTime, value, sizeof(cfg.scheduleStartTime));
    else if (strcmp(key, "schedule_end_time") == 0)            strlcpy(cfg.scheduleEndTime,   value, sizeof(cfg.scheduleEndTime));
    else if (strcmp(key, "schedule_color_hex") == 0)           strlcpy(cfg.scheduleColorHex,  value, sizeof(cfg.scheduleColorHex));
    else if (strcmp(key, "schedule_brightness") == 0)          cfg.scheduleBrightness         = atoi(value);
    else if (strcmp(key, "reconnect_interval_seconds") == 0)   cfg.reconnectIntervalSeconds   = atoi(value);
    else if (strcmp(key, "recovery_bootstrap_every_n_cycles") == 0) cfg.recoveryBootstrapEveryNCycles = atoi(value);
    save(cfg);
  }

private:
  static Preferences prefs;
};

Preferences Storage::prefs;
