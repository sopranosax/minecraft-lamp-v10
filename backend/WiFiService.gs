// ============================================================
// WiFiService.gs — Escaneo y configuración WiFi
// IoT Lámpara WiFi v10
//
// Scan data is stored as JSON in DEVICES.WIFI_SCAN_JSON column
// (no separate scan table needed).
// ============================================================

// ─── Recibir escaneo del ESP32 (POST action=wifi_scan) ───────
// Stores the scan JSON directly in the DEVICES row.
function handleWifiScan(mac, body) {
  if (!mac) return errorResponse('mac es requerido', 'MISSING_MAC');

  const { networks } = body;
  if (!networks || !networks.length)
    return errorResponse('networks es requerido y no puede estar vacío', 'MISSING_NETWORKS');

  const lock = LockService.getScriptLock();
  lock.waitLock(10000);
  try {
    const sheet = getSheet(SHEET_NAMES.DEVICES);
    const found = findRowByCol(sheet, COL.DEVICES.MAC_ADDRESS, mac);
    if (!found) return errorResponse('Dispositivo no encontrado', 'NOT_FOUND');

    const nowTs = nowIso();

    // Build compact JSON array: [{ssid, rssi, auth, ts}]
    const scanArray = networks.slice(0, 15).map(net => ({
      ssid: net.ssid || '',
      rssi: net.rssi || 0,
      auth: net.auth || '',
      ts:   nowTs
    }));

    // Store as JSON string in DEVICES.WIFI_SCAN_JSON
    setCell(sheet, found.rowNumber, COL.DEVICES.WIFI_SCAN_JSON, JSON.stringify(scanArray));
    setCell(sheet, found.rowNumber, COL.DEVICES.LAST_SEEN_AT,   nowTs);

    return successResponse({ saved: scanArray.length });
  } finally {
    lock.releaseLock();
  }
}

// ─── Obtener escaneo vigente (GET action=get_scan) ────────────
// Reads from DEVICES.WIFI_SCAN_JSON column.
function handleGetScan(mac, token) {
  const userId = validateToken(token);
  if (!userId) return errorResponse('No autorizado', 'UNAUTHORIZED');
  if (!mac)    return errorResponse('mac es requerido', 'MISSING_MAC');

  const sheet = getSheet(SHEET_NAMES.DEVICES);
  const found = findRowByCol(sheet, COL.DEVICES.MAC_ADDRESS, mac);
  if (!found) return errorResponse('Dispositivo no encontrado', 'NOT_FOUND');

  let networks = [];
  try {
    const scanJson = String(found.rowData[COL.DEVICES.WIFI_SCAN_JSON - 1] || '');
    if (scanJson) networks = JSON.parse(scanJson);
  } catch (_) { /* invalid JSON */ }

  // Map to webapp-expected format and sort by signal strength
  const mapped = networks
    .sort((a, b) => (b.rssi || 0) - (a.rssi || 0))
    .map(n => ({
      ssid:          n.ssid || '',
      rssi:          n.rssi || 0,
      security_type: n.auth || '',
      scanned_at:    n.ts   || ''
    }));

  return successResponse({ networks: mapped, count: mapped.length });
}

// ─── Guardar credenciales WiFi desde web app (POST action=wifi_config) ─
function handleWifiConfig(mac, body) {
  const userId = validateToken(body.token);
  if (!userId) return errorResponse('No autorizado', 'UNAUTHORIZED');
  if (!mac)    return errorResponse('mac es requerido', 'MISSING_MAC');

  const { ssid, password } = body;
  if (!ssid)     return errorResponse('ssid es requerido', 'MISSING_SSID');
  // Password can be empty for OPEN networks
  if (password === undefined || password === null) return errorResponse('password es requerido', 'MISSING_PASSWORD');

  // Validate that the SSID exists in the device's current scan
  const devSheet = getSheet(SHEET_NAMES.DEVICES);
  const devFound = findRowByCol(devSheet, COL.DEVICES.MAC_ADDRESS, mac);
  if (!devFound) return errorResponse('Dispositivo no encontrado', 'NOT_FOUND');

  let scanNetworks = [];
  try {
    const scanJson = String(devFound.rowData[COL.DEVICES.WIFI_SCAN_JSON - 1] || '');
    if (scanJson) scanNetworks = JSON.parse(scanJson);
  } catch (_) {}

  const ssidExists = scanNetworks.some(n => n.ssid === ssid);
  if (!ssidExists)
    return errorResponse('SSID no encontrado en el escaneo vigente del dispositivo', 'SSID_NOT_IN_SCAN');

  const lock = LockService.getScriptLock();
  lock.waitLock(10000);
  try {
    const sheet = getSheet(SHEET_NAMES.DEVICE_WIFI_CREDS);
    const nowTs = nowIso();

    const row = new Array(10).fill('');
    row[COL.WIFI_CREDS.WIFI_CONFIG_ID       - 1] = generateId();
    row[COL.WIFI_CREDS.MAC_ADDRESS          - 1] = mac;
    row[COL.WIFI_CREDS.SSID                 - 1] = ssid;
    row[COL.WIFI_CREDS.PASSWORD             - 1] = password;
    row[COL.WIFI_CREDS.STATUS               - 1] = ST.PENDING;
    row[COL.WIFI_CREDS.REQUESTED_BY_USER_ID - 1] = userId;
    row[COL.WIFI_CREDS.REQUESTED_AT         - 1] = nowTs;
    sheet.appendRow(row);

    // Update device status
    setCell(devSheet, devFound.rowNumber, COL.DEVICES.STATUS,     ST.CONFIG_PENDING);
    setCell(devSheet, devFound.rowNumber, COL.DEVICES.UPDATED_AT, nowTs);

    addLog(mac, EV.WIFI_CONFIG_REQUESTED, nowTs, ssid, '', 'WEBAPP');
    return successResponse({ status: ST.PENDING });
  } finally {
    lock.releaseLock();
  }
}

// ─── Obtener estado de credenciales WiFi (GET action=get_wifi_status) ──
function handleGetWifiStatus(mac, token) {
  const userId = validateToken(token);
  if (!userId) return errorResponse('No autorizado', 'UNAUTHORIZED');
  if (!mac)    return errorResponse('mac es requerido', 'MISSING_MAC');

  const sheet = getSheet(SHEET_NAMES.DEVICE_WIFI_CREDS);
  const rows  = getAllRows(sheet).filter(
    r => String(r[COL.WIFI_CREDS.MAC_ADDRESS - 1]).toUpperCase() === mac
  );

  if (!rows.length) return successResponse({ wifi_status: null });

  // Return the most recent credential entry
  const latest = rows[rows.length - 1];
  return successResponse({
    wifi_status: {
      ssid:         String(latest[COL.WIFI_CREDS.SSID         - 1]),
      status:       String(latest[COL.WIFI_CREDS.STATUS       - 1]),
      requested_at: String(latest[COL.WIFI_CREDS.REQUESTED_AT - 1]),
      applied_at:   String(latest[COL.WIFI_CREDS.APPLIED_AT   - 1]),
      failed_at:    String(latest[COL.WIFI_CREDS.FAILED_AT    - 1]),
      fail_reason:  String(latest[COL.WIFI_CREDS.FAIL_REASON  - 1])
    }
  });
}
