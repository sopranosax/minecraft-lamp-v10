// ============================================================
// WiFiService.gs — Escaneo y configuración WiFi
// IoT Lámpara WiFi v10
// ============================================================

// ─── Recibir escaneo del ESP32 (POST action=wifi_scan) ───────
function handleWifiScan(mac, body) {
  if (!mac) return errorResponse('mac es requerido', 'MISSING_MAC');

  const { scan_ts, networks } = body;
  if (!networks || !networks.length)
    return errorResponse('networks es requerido y no puede estar vacío', 'MISSING_NETWORKS');

  const lock = LockService.getScriptLock();
  lock.waitLock(15000);
  try {
    const sheet = getSheet(SHEET_NAMES.DEVICE_WIFI_SCAN);
    const nowTs = scan_ts || nowIso();

    // Demarcar escaneos anteriores como no vigentes
    const allRows = getAllRows(sheet);
    allRows.forEach((row, i) => {
      if (String(row[COL.WIFI_SCAN.MAC_ADDRESS - 1]).toUpperCase() === mac &&
          String(row[COL.WIFI_SCAN.IS_CURRENT  - 1]) === 'TRUE') {
        setCell(sheet, i + 2, COL.WIFI_SCAN.IS_CURRENT, 'FALSE');
      }
    });

    // Insertar nueva lista de redes
    networks.forEach(net => {
      const row = new Array(7).fill('');
      row[COL.WIFI_SCAN.SCAN_ID       - 1] = generateId();
      row[COL.WIFI_SCAN.MAC_ADDRESS   - 1] = mac;
      row[COL.WIFI_SCAN.SSID          - 1] = net.ssid    || '';
      row[COL.WIFI_SCAN.RSSI          - 1] = net.rssi    || 0;
      row[COL.WIFI_SCAN.SECURITY_TYPE - 1] = net.auth    || '';
      row[COL.WIFI_SCAN.SCANNED_AT    - 1] = nowTs;
      row[COL.WIFI_SCAN.IS_CURRENT    - 1] = 'TRUE';
      sheet.appendRow(row);
    });

    addLog(mac, EV.WIFI_SCAN_PUBLISHED, nowTs, networks.length + ' redes', '', 'DEVICE');
    return successResponse({ saved: networks.length });
  } finally {
    lock.releaseLock();
  }
}

// ─── Obtener escaneo vigente (GET action=get_scan) ────────────
function handleGetScan(mac, token) {
  const userId = validateToken(token);
  if (!userId) return errorResponse('No autorizado', 'UNAUTHORIZED');
  if (!mac)    return errorResponse('mac es requerido', 'MISSING_MAC');

  const sheet = getSheet(SHEET_NAMES.DEVICE_WIFI_SCAN);
  const rows  = getAllRows(sheet).filter(
    r => String(r[COL.WIFI_SCAN.MAC_ADDRESS - 1]).toUpperCase() === mac &&
         String(r[COL.WIFI_SCAN.IS_CURRENT  - 1]) === 'TRUE'
  );

  const networks = rows
    .sort((a, b) => Number(b[COL.WIFI_SCAN.RSSI - 1]) - Number(a[COL.WIFI_SCAN.RSSI - 1]))
    .map(r => ({
      ssid:          String(r[COL.WIFI_SCAN.SSID          - 1]),
      rssi:          Number(r[COL.WIFI_SCAN.RSSI          - 1]),
      security_type: String(r[COL.WIFI_SCAN.SECURITY_TYPE - 1]),
      scanned_at:    String(r[COL.WIFI_SCAN.SCANNED_AT    - 1])
    }));

  return successResponse({ networks: networks, count: networks.length });
}

// ─── Guardar credenciales WiFi desde web app (POST action=wifi_config) ─
function handleWifiConfig(mac, body) {
  const userId = validateToken(body.token);
  if (!userId) return errorResponse('No autorizado', 'UNAUTHORIZED');
  if (!mac)    return errorResponse('mac es requerido', 'MISSING_MAC');

  const { ssid, password } = body;
  if (!ssid)     return errorResponse('ssid es requerido', 'MISSING_SSID');
  if (!password) return errorResponse('password es requerido', 'MISSING_PASSWORD');

  // Validar que el SSID pertenece a un escaneo vigente de este dispositivo
  const scanSheet = getSheet(SHEET_NAMES.DEVICE_WIFI_SCAN);
  const scanRows  = getAllRows(scanSheet).filter(
    r => String(r[COL.WIFI_SCAN.MAC_ADDRESS - 1]).toUpperCase() === mac &&
         String(r[COL.WIFI_SCAN.IS_CURRENT  - 1]) === 'TRUE' &&
         String(r[COL.WIFI_SCAN.SSID        - 1]) === ssid
  );
  if (!scanRows.length)
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

    // Actualizar estado del dispositivo
    const devSheet = getSheet(SHEET_NAMES.DEVICES);
    const found    = findRowByCol(devSheet, COL.DEVICES.MAC_ADDRESS, mac);
    if (found) {
      setCell(devSheet, found.rowNumber, COL.DEVICES.STATUS,     ST.CONFIG_PENDING);
      setCell(devSheet, found.rowNumber, COL.DEVICES.UPDATED_AT, nowTs);
    }

    addLog(mac, EV.WIFI_CONFIG_REQUESTED, nowTs, ssid, '', 'WEBAPP');
    return successResponse({ status: ST.PENDING });
  } finally {
    lock.releaseLock();
  }
}
