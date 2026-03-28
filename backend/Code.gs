// ============================================================
// Code.gs — Router principal (doGet / doPost)
// IoT Lámpara WiFi v10
// ============================================================

/**
 * Maneja todos los requests GET (web app + lecturas del ESP32).
 * Parámetros vía URL: ?action=xxx&mac=yyy&token=zzz
 */
function doGet(e) {
  try {
    const p      = e.parameter || {};
    const action = p.action || '';
    const mac    = (p.mac || '').toUpperCase();
    const token  = p.token || '';
    const limit  = parseInt(p.limit) || DEFAULTS.MAX_LOG_ROWS;

    switch (action) {
      case 'get_devices':    return handleGetDevices(token);
      case 'get_device':     return handleGetDevice(mac, token);
      case 'pending_config': return handleGetPendingConfig(mac);
      case 'get_scan':       return handleGetScan(mac, token);
      case 'get_wifi_status': return handleGetWifiStatus(mac, token);
      case 'get_logs':       return handleGetLogs(mac, token, limit);
      default:               return errorResponse('Acción desconocida: ' + action, 'UNKNOWN_ACTION');
    }
  } catch (err) {
    console.error('doGet error:', err);
    return errorResponse('Error interno: ' + err.toString(), 'INTERNAL_ERROR');
  }
}

/**
 * Maneja todos los requests POST (ESP32 + escrituras de la web app).
 * Body puede ser JSON (ESP32) o form-encoded (web app).
 * También acepta action como query param para compatibilidad.
 */
function doPost(e) {
  try {
    const body   = parseBody(e);
    const action = (e.parameter && e.parameter.action) || body.action || '';
    const mac    = ((body.mac || body.mac_address || '')).toString().toUpperCase();

    switch (action) {
      // Auth
      case 'login':          return handleLogin(body);

      // ESP32 — arranque y presencia
      case 'bootstrap':      return handleBootstrap(body);
      case 'update_status':  return handleUpdateStatus(mac, body);

      // ESP32 — WiFi
      case 'wifi_scan':      return handleWifiScan(mac, body);

      // Web App — WiFi config
      case 'wifi_config':    return handleWifiConfig(mac, body);

      // ESP32 — eventos
      case 'log_event':      return handleLogEvent(mac, body);

      // Web App — control
      case 'control':        return handleControl(mac, body);
      case 'motion_config':  return handleMotionConfig(mac, body);
      case 'schedule':       return handleSchedule(mac, body);

      default:               return errorResponse('Acción desconocida: ' + action, 'UNKNOWN_ACTION');
    }
  } catch (err) {
    console.error('doPost error:', err);
    return errorResponse('Error interno: ' + err.toString(), 'INTERNAL_ERROR');
  }
}
