// ============================================================
// Config.gs — Constantes y configuración del sistema
// IoT Lámpara WiFi v10
// ============================================================

const SHEET_NAMES = {
  USERS:            'USERS',
  DEVICES:          'DEVICES',
  DEVICE_WIFI_SCAN: 'DEVICE_WIFI_SCAN',
  DEVICE_WIFI_CREDS:'DEVICE_WIFI_CREDENTIALS',
  DEVICE_LOG:       'DEVICE_LOG'
};

// Índices de columna (1-based) por hoja
const COL = {
  USERS: {
    USER_ID:          1,
    LOGIN:            2,
    PASSWORD_HASH:    3,
    STATUS:           4,
    CREATED_AT:       5,
    UPDATED_AT:       6,
    LAST_LOGIN_AT:    7,
    SESSION_TOKEN:    8,
    TOKEN_EXPIRES_AT: 9
  },
  DEVICES: {
    DEVICE_ID:                        1,
    MAC_ADDRESS:                      2,
    DEVICE_ALIAS:                     3,
    STATUS:                           4,
    CURRENT_WIFI_SSID:                5,
    LAST_SEEN_AT:                     6,
    LAST_IP:                          7,
    MOTION_MODE:                      8,
    MOTION_BLINK_SECONDS:             9,
    SCHEDULE_ENABLED:                 10,
    SCHEDULE_START_TIME:              11,
    SCHEDULE_END_TIME:                12,
    SCHEDULE_COLOR_HEX:               13,
    MANUAL_POWER_STATE:               14,
    MANUAL_COLOR_HEX:                 15,
    RECONNECT_INTERVAL_SECONDS:       16,
    LAST_WIFI_ERROR:                  17,
    RECOVERY_MODE:                    18,
    RECOVERY_BOOTSTRAP_EVERY_N_CYCLES:19,
    FIRMWARE_VERSION:                 20,
    CREATED_AT:                       21,
    UPDATED_AT:                       22,
    MANUAL_BRIGHTNESS:                23,  // 0-255
    SCHEDULE_BRIGHTNESS:              24   // 0-255
  },
  WIFI_SCAN: {
    SCAN_ID:       1,
    MAC_ADDRESS:   2,
    SSID:          3,
    RSSI:          4,
    SECURITY_TYPE: 5,
    SCANNED_AT:    6,
    IS_CURRENT:    7
  },
  WIFI_CREDS: {
    WIFI_CONFIG_ID:        1,
    MAC_ADDRESS:           2,
    SSID:                  3,
    PASSWORD:              4,
    STATUS:                5,
    REQUESTED_BY_USER_ID:  6,
    REQUESTED_AT:          7,
    APPLIED_AT:            8,
    FAILED_AT:             9,
    FAIL_REASON:           10
  },
  LOG: {
    LOG_ID:       1,
    MAC_ADDRESS:  2,
    EVENT_TYPE:   3,
    EVENT_TS:     4,
    EVENT_VALUE:  5,
    DETAILS_JSON: 6,
    SOURCE:       7
  }
};

// Status values
const ST = {
  // Device
  ONLINE:          'ONLINE',
  OFFLINE:         'OFFLINE',
  CONFIG_PENDING:  'CONFIG_PENDING',
  WIFI_CONNECTING: 'WIFI_CONNECTING',
  WIFI_FAILED:     'WIFI_FAILED',
  // WiFi credentials
  PENDING: 'PENDING',
  APPLIED: 'APPLIED',
  FAILED:  'FAILED',
  // User
  ACTIVE:   'ACTIVE',
  INACTIVE: 'INACTIVE',
  // Motion
  BLINK:    'BLINK',
  NO_BLINK: 'NO_BLINK',
  // Power
  ON:  'ON',
  OFF: 'OFF'
};

// Defaults (alineados con decisiones confirmadas)
const DEFAULTS = {
  RECONNECT_INTERVAL_SECONDS:        60,
  RECOVERY_BOOTSTRAP_EVERY_N_CYCLES: 5,
  MOTION_MODE:                       'NO_BLINK',
  MOTION_BLINK_SECONDS:              5,
  MANUAL_POWER_STATE:                'OFF',
  MANUAL_COLOR_HEX:                  '#FFFFFF',
  MANUAL_BRIGHTNESS:                 200,  // 0-255 (~78%)
  SCHEDULE_BRIGHTNESS:               200,
  TOKEN_EXPIRES_HOURS:               24,
  OFFLINE_THRESHOLD_MINUTES:         5,
  MAX_LOG_ROWS:                      50
};

// Event types
const EV = {
  DEVICE_REGISTERED:           'DEVICE_REGISTERED',
  DEVICE_ONLINE:               'DEVICE_ONLINE',
  DEVICE_OFFLINE:              'DEVICE_OFFLINE',
  WIFI_SCAN_PUBLISHED:         'WIFI_SCAN_PUBLISHED',
  WIFI_CONFIG_REQUESTED:       'WIFI_CONFIG_REQUESTED',
  WIFI_CONNECTING:             'WIFI_CONNECTING',
  WIFI_CONNECTED:              'WIFI_CONNECTED',
  WIFI_FAILED:                 'WIFI_FAILED',
  MOTION_DETECTED:             'MOTION_DETECTED',
  POWER_ON:                    'POWER_ON',
  POWER_OFF:                   'POWER_OFF',
  COLOR_CHANGED:               'COLOR_CHANGED',
  BRIGHTNESS_CHANGED:          'BRIGHTNESS_CHANGED',
  SCHEDULE_UPDATED:            'SCHEDULE_UPDATED',
  WATCHDOG_RESTART:            'WATCHDOG_RESTART',
  RECOVERY_MODE_ENTERED:       'RECOVERY_MODE_ENTERED',
  RECOVERY_RETRY_OPERATIONAL:  'RECOVERY_RETRY_OPERATIONAL',
  RECOVERY_BOOTSTRAP_CONNECTED:'RECOVERY_BOOTSTRAP_CONNECTED',
  RECOVERY_BOOTSTRAP_FAILED:   'RECOVERY_BOOTSTRAP_FAILED'
};

// Headers para cada hoja (usados en Setup.gs)
const HEADERS = {
  USERS: ['USER_ID','LOGIN','PASSWORD_HASH','STATUS','CREATED_AT','UPDATED_AT',
          'LAST_LOGIN_AT','SESSION_TOKEN','TOKEN_EXPIRES_AT'],
  DEVICES: ['DEVICE_ID','MAC_ADDRESS','DEVICE_ALIAS','STATUS','CURRENT_WIFI_SSID',
            'LAST_SEEN_AT','LAST_IP','MOTION_MODE','MOTION_BLINK_SECONDS',
            'SCHEDULE_ENABLED','SCHEDULE_START_TIME','SCHEDULE_END_TIME',
            'SCHEDULE_COLOR_HEX','MANUAL_POWER_STATE','MANUAL_COLOR_HEX',
            'RECONNECT_INTERVAL_SECONDS','LAST_WIFI_ERROR','RECOVERY_MODE',
            'RECOVERY_BOOTSTRAP_EVERY_N_CYCLES','FIRMWARE_VERSION',
            'CREATED_AT','UPDATED_AT','MANUAL_BRIGHTNESS','SCHEDULE_BRIGHTNESS'],
  DEVICE_WIFI_SCAN: ['SCAN_ID','MAC_ADDRESS','SSID','RSSI','SECURITY_TYPE',
                     'SCANNED_AT','IS_CURRENT'],
  DEVICE_WIFI_CREDENTIALS: ['WIFI_CONFIG_ID','MAC_ADDRESS','SSID','PASSWORD',
                             'STATUS','REQUESTED_BY_USER_ID','REQUESTED_AT',
                             'APPLIED_AT','FAILED_AT','FAIL_REASON'],
  DEVICE_LOG: ['LOG_ID','MAC_ADDRESS','EVENT_TYPE','EVENT_TS','EVENT_VALUE',
               'DETAILS_JSON','SOURCE']
};
