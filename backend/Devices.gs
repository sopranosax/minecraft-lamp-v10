// ============================================================
// Devices.gs — Registro, presencia y estado de dispositivos
// IoT Lámpara WiFi v10
// ============================================================

// ─── Bootstrap (POST action=bootstrap) ───────────────────────
function handleBootstrap(body) {
  const mac = (body.mac_address || body.mac || '').toUpperCase();
  if (!mac) return errorResponse('mac_address es requerido', 'MISSING_MAC');

  const lock = LockService.getScriptLock();
  lock.waitLock(15000);
  try {
    const sheet = getSheet(SHEET_NAMES.DEVICES);
    const found = findRowByCol(sheet, COL.DEVICES.MAC_ADDRESS, mac);
    const nowTs = nowIso();
    let deviceId, isNew;

    if (!found) {
      // ─── Dispositivo nuevo: crear fila ───────────────────
      deviceId = generateId();
      isNew    = true;
      const newRow = new Array(25).fill('');
      newRow[COL.DEVICES.DEVICE_ID                        - 1] = deviceId;
      newRow[COL.DEVICES.MAC_ADDRESS                      - 1] = mac;
      newRow[COL.DEVICES.DEVICE_ALIAS                     - 1] = 'Lámpara ' + mac.slice(-5);
      newRow[COL.DEVICES.STATUS                           - 1] = ST.ONLINE;
      newRow[COL.DEVICES.LAST_SEEN_AT                     - 1] = nowTs;
      newRow[COL.DEVICES.LAST_IP                          - 1] = body.ip || '';
      newRow[COL.DEVICES.MOTION_MODE                      - 1] = DEFAULTS.MOTION_MODE;
      newRow[COL.DEVICES.MOTION_BLINK_SECONDS             - 1] = DEFAULTS.MOTION_BLINK_SECONDS;
      newRow[COL.DEVICES.SCHEDULE_ENABLED                 - 1] = 'FALSE';
      newRow[COL.DEVICES.MANUAL_POWER_STATE               - 1] = DEFAULTS.MANUAL_POWER_STATE;
      newRow[COL.DEVICES.MANUAL_COLOR_HEX                 - 1] = DEFAULTS.MANUAL_COLOR_HEX;
      newRow[COL.DEVICES.RECONNECT_INTERVAL_SECONDS       - 1] = DEFAULTS.RECONNECT_INTERVAL_SECONDS;
      newRow[COL.DEVICES.RECOVERY_MODE                    - 1] = 'FALSE';
      newRow[COL.DEVICES.RECOVERY_BOOTSTRAP_EVERY_N_CYCLES-1] = DEFAULTS.RECOVERY_BOOTSTRAP_EVERY_N_CYCLES;
      newRow[COL.DEVICES.FIRMWARE_VERSION                 - 1] = body.firmware_version || '';
      newRow[COL.DEVICES.CREATED_AT                       - 1] = nowTs;
      newRow[COL.DEVICES.UPDATED_AT                       - 1] = nowTs;
      sheet.appendRow(newRow);
      // Forzar texto en columnas MAC e IP para evitar auto-formato de Sheets
      const newRowNum = sheet.getLastRow();
      sheet.getRange(newRowNum, COL.DEVICES.MAC_ADDRESS).setNumberFormat('@');
      sheet.getRange(newRowNum, COL.DEVICES.LAST_IP).setNumberFormat('@');
      addLog(mac, EV.DEVICE_REGISTERED, nowTs, '', JSON.stringify({ ip: body.ip, fw: body.firmware_version }), 'DEVICE');
    } else {
      // ─── Dispositivo existente: actualizar presencia ──────
      deviceId = String(found.rowData[COL.DEVICES.DEVICE_ID - 1]);
      isNew    = false;
      setCell(sheet, found.rowNumber, COL.DEVICES.STATUS,           ST.ONLINE);
      setCell(sheet, found.rowNumber, COL.DEVICES.LAST_SEEN_AT,     nowTs);
      setCell(sheet, found.rowNumber, COL.DEVICES.UPDATED_AT,       nowTs);
      if (body.ip)                  setCellText(sheet, found.rowNumber, COL.DEVICES.LAST_IP,          body.ip);
      if (body.firmware_version)    setCell(sheet, found.rowNumber, COL.DEVICES.FIRMWARE_VERSION, body.firmware_version);
      if (body.current_wifi_ssid)   setCell(sheet, found.rowNumber, COL.DEVICES.CURRENT_WIFI_SSID, body.current_wifi_ssid);
      addLog(mac, EV.DEVICE_ONLINE, nowTs, '', JSON.stringify({ ip: body.ip }), 'DEVICE');
    }

    // Obtener config vigente para devolver al ESP32
    const deviceSheet = getSheet(SHEET_NAMES.DEVICES);
    const row = (found ? found.rowData : findRowByCol(deviceSheet, COL.DEVICES.MAC_ADDRESS, mac).rowData);
    const config = buildDeviceConfig(row);

    return successResponse({ device_id: deviceId, status: ST.ONLINE, is_new: isNew, config: config });
  } finally {
    lock.releaseLock();
  }
}

// ─── Update status (POST action=update_status) ───────────────
function handleUpdateStatus(mac, body) {
  if (!mac) return errorResponse('mac es requerido', 'MISSING_MAC');

  const lock = LockService.getScriptLock();
  lock.waitLock(10000);
  try {
    const sheet = getSheet(SHEET_NAMES.DEVICES);
    const found = findRowByCol(sheet, COL.DEVICES.MAC_ADDRESS, mac);
    if (!found) return errorResponse('Dispositivo no encontrado', 'NOT_FOUND');

    const nowTs  = body.ts || nowIso();
    const status = body.status || ST.ONLINE;

    setCell(sheet, found.rowNumber, COL.DEVICES.STATUS,       status);
    setCell(sheet, found.rowNumber, COL.DEVICES.LAST_SEEN_AT, nowTs);
    setCell(sheet, found.rowNumber, COL.DEVICES.UPDATED_AT,   nowTs);
    if (body.current_wifi_ssid) setCell(sheet, found.rowNumber, COL.DEVICES.CURRENT_WIFI_SSID, body.current_wifi_ssid);
    if (body.ip)                setCellText(sheet, found.rowNumber, COL.DEVICES.LAST_IP, body.ip);

    // Actualizar estado de credenciales WiFi si aplica
    if (status === 'WIFI_CONNECTED') {
      _updateWifiCredStatus(mac, ST.APPLIED);
      setCell(sheet, found.rowNumber, COL.DEVICES.LAST_WIFI_ERROR, '');
      addLog(mac, EV.WIFI_CONNECTED, nowTs, status, JSON.stringify({ ssid: body.current_wifi_ssid, ip: body.ip }), 'DEVICE');
    } else if (status === ST.WIFI_FAILED || status === 'WIFI_FAILED') {
      const reason = body.fail_reason || 'Connection failed';
      _updateWifiCredStatus(mac, ST.FAILED, reason);
      setCell(sheet, found.rowNumber, COL.DEVICES.LAST_WIFI_ERROR, reason);
      addLog(mac, EV.WIFI_FAILED, nowTs, status, JSON.stringify({ reason: reason }), 'DEVICE');
    } else if (status === 'RECOVERY_MODE_ENTERED') {
      setCell(sheet, found.rowNumber, COL.DEVICES.RECOVERY_MODE, 'TRUE');
      addLog(mac, EV.RECOVERY_MODE_ENTERED, nowTs, '', '', 'DEVICE');
    } else if (status === 'RECOVERY_BOOTSTRAP_CONNECTED') {
      addLog(mac, EV.RECOVERY_BOOTSTRAP_CONNECTED, nowTs, '', '', 'DEVICE');
    } else if (status === 'WATCHDOG_RESTART') {
      addLog(mac, EV.WATCHDOG_RESTART, nowTs, '', body.details || '', 'DEVICE');
    }

    return successResponse({ updated: true });
  } finally {
    lock.releaseLock();
  }
}

// ─── Get devices list (GET action=get_devices) ───────────────
function handleGetDevices(token) {
  const userId = validateToken(token);
  if (!userId) return errorResponse('No autorizado', 'UNAUTHORIZED');

  const sheet = getSheet(SHEET_NAMES.DEVICES);
  const rows  = getAllRows(sheet);

  const devices = rows.map(row => ({
    device_id:         String(row[COL.DEVICES.DEVICE_ID       - 1]),
    mac_address:       formatMac(row[COL.DEVICES.MAC_ADDRESS      - 1]),
    device_alias:      String(row[COL.DEVICES.DEVICE_ALIAS     - 1]),
    status:            isDeviceOnline(String(row[COL.DEVICES.LAST_SEEN_AT - 1])) ? ST.ONLINE : ST.OFFLINE,
    current_wifi_ssid: String(row[COL.DEVICES.CURRENT_WIFI_SSID- 1]),
    last_seen_at:      String(row[COL.DEVICES.LAST_SEEN_AT     - 1]),
    manual_power_state:String(row[COL.DEVICES.MANUAL_POWER_STATE-1])
  }));

  return successResponse({ devices: devices });
}

// ─── Get device detail (GET action=get_device) ───────────────
function handleGetDevice(mac, token) {
  const userId = validateToken(token);
  if (!userId) return errorResponse('No autorizado', 'UNAUTHORIZED');
  if (!mac)    return errorResponse('mac es requerido', 'MISSING_MAC');

  const sheet = getSheet(SHEET_NAMES.DEVICES);
  const found = findRowByCol(sheet, COL.DEVICES.MAC_ADDRESS, mac);
  if (!found)  return errorResponse('Dispositivo no encontrado', 'NOT_FOUND');

  const row = found.rowData;
  // Parse WiFi scan JSON from DEVICES table
  let wifiScan = [];
  try {
    const scanJson = String(row[COL.DEVICES.WIFI_SCAN_JSON - 1] || '');
    if (scanJson) wifiScan = JSON.parse(scanJson);
  } catch (_) { /* invalid JSON, return empty */ }

  return successResponse({
    device: {
      device_id:          String(row[COL.DEVICES.DEVICE_ID          - 1]),
      mac_address:        formatMac(row[COL.DEVICES.MAC_ADDRESS         - 1]),
      device_alias:       String(row[COL.DEVICES.DEVICE_ALIAS        - 1]),
      status:             isDeviceOnline(String(row[COL.DEVICES.LAST_SEEN_AT-1])) ? ST.ONLINE : ST.OFFLINE,
      current_wifi_ssid:  String(row[COL.DEVICES.CURRENT_WIFI_SSID  - 1]),
      last_seen_at:       String(row[COL.DEVICES.LAST_SEEN_AT        - 1]),
      last_ip:            formatIp(row[COL.DEVICES.LAST_IP             - 1]),
      firmware_version:   String(row[COL.DEVICES.FIRMWARE_VERSION    - 1]),
      last_wifi_error:    String(row[COL.DEVICES.LAST_WIFI_ERROR     - 1]),
      recovery_mode:      String(row[COL.DEVICES.RECOVERY_MODE       - 1]) === 'TRUE',
      wifi_scan:          wifiScan,
      config:             buildDeviceConfig(row)
    }
  });
}

// ─── Get pending config (GET action=pending_config) ──────────
// Llamado por el ESP32 cada 30s (D1)
function handleGetPendingConfig(mac) {
  if (!mac) return errorResponse('mac es requerido', 'MISSING_MAC');

  const sheet = getSheet(SHEET_NAMES.DEVICES);
  const found = findRowByCol(sheet, COL.DEVICES.MAC_ADDRESS, mac);
  if (!found) return errorResponse('Dispositivo no encontrado', 'NOT_FOUND');

  const row    = found.rowData;
  const config = buildDeviceConfig(row);

  // Buscar credenciales WiFi pendientes
  const wifiSheet = getSheet(SHEET_NAMES.DEVICE_WIFI_CREDS);
  const wifiRows  = getAllRows(wifiSheet).filter(
    r => String(r[COL.WIFI_CREDS.MAC_ADDRESS - 1]).toUpperCase() === mac &&
         String(r[COL.WIFI_CREDS.STATUS      - 1]) === ST.PENDING
  );
  const pendingWifi = wifiRows.length > 0 ? {
    status:   ST.PENDING,
    ssid:     String(wifiRows[wifiRows.length-1][COL.WIFI_CREDS.SSID     - 1]),
    password: String(wifiRows[wifiRows.length-1][COL.WIFI_CREDS.PASSWORD - 1])
  } : null;

  // Actualizar heartbeat
  const nowTs = nowIso();
  setCell(sheet, found.rowNumber, COL.DEVICES.LAST_SEEN_AT, nowTs);

  return successResponse({ wifi_config: pendingWifi, device_config: config });
}

// ─── Interno: actualiza estado de primera credencial PENDING ─
function _updateWifiCredStatus(mac, status, failReason) {
  const sheet = getSheet(SHEET_NAMES.DEVICE_WIFI_CREDS);
  const rows  = getAllRows(sheet);
  const nowTs = nowIso();
  for (let i = 0; i < rows.length; i++) {
    if (String(rows[i][COL.WIFI_CREDS.MAC_ADDRESS - 1]).toUpperCase() === mac &&
        String(rows[i][COL.WIFI_CREDS.STATUS - 1]) === ST.PENDING) {
      const rowNum = i + 2;
      setCell(sheet, rowNum, COL.WIFI_CREDS.STATUS, status);
      if (status === ST.APPLIED) setCell(sheet, rowNum, COL.WIFI_CREDS.APPLIED_AT, nowTs);
      if (status === ST.FAILED) {
        setCell(sheet, rowNum, COL.WIFI_CREDS.FAILED_AT,   nowTs);
        setCell(sheet, rowNum, COL.WIFI_CREDS.FAIL_REASON, failReason || '');
      }
      break;
    }
  }
}
