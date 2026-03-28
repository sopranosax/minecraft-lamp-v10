// ============================================================
// LogService.gs — Logging y auditoría
// IoT Lámpara WiFi v10
// ============================================================

/**
 * Inserta un evento en DEVICE_LOG.
 * Llamado internamente por otros handlers Y por el ESP32 vía handleLogEvent.
 */
function addLog(macAddress, eventType, eventTs, eventValue, detailsJson, source) {
  try {
    const sheet  = getSheet(SHEET_NAMES.DEVICE_LOG);
    const nowTs  = eventTs || nowIso();
    const row    = new Array(7).fill('');
    row[COL.LOG.LOG_ID       - 1] = generateId();
    row[COL.LOG.MAC_ADDRESS  - 1] = macAddress;
    row[COL.LOG.EVENT_TYPE   - 1] = eventType;
    row[COL.LOG.EVENT_TS     - 1] = nowTs;
    row[COL.LOG.EVENT_VALUE  - 1] = eventValue  || '';
    row[COL.LOG.DETAILS_JSON - 1] = detailsJson || '';
    row[COL.LOG.SOURCE       - 1] = source      || 'DEVICE';
    sheet.appendRow(row);
  } catch (err) {
    console.error('addLog error:', err);
  }
}

/**
 * Handler POST: el ESP32 envía un evento de movimiento u otro tipo.
 * Body: { mac, event_type, event_ts, event_value, details, source }
 */
function handleLogEvent(mac, body) {
  if (!mac) return errorResponse('mac es requerido', 'MISSING_MAC');

  const { event_type, event_ts, event_value, details, source } = body;
  if (!event_type) return errorResponse('event_type es requerido', 'MISSING_EVENT_TYPE');

  // Solo aceptar MOTION_DETECTED si el dispositivo reportó presencia reciente
  if (event_type === EV.MOTION_DETECTED) {
    const sheet = getSheet(SHEET_NAMES.DEVICES);
    const found = findRowByCol(sheet, COL.DEVICES.MAC_ADDRESS, mac);
    if (!found) return errorResponse('Dispositivo no encontrado', 'NOT_FOUND');
    const lastSeen = String(found.rowData[COL.DEVICES.LAST_SEEN_AT - 1]);
    if (!isDeviceOnline(lastSeen))
      return errorResponse('Dispositivo offline, no se registra movimiento', 'DEVICE_OFFLINE');
  }

  const detailsStr = typeof details === 'object' ? JSON.stringify(details) : (details || '');
  addLog(mac, event_type, event_ts || nowIso(), event_value || '', detailsStr, source || 'DEVICE');
  return successResponse({ logged: true });
}

/**
 * Handler GET: la web app consulta los últimos N eventos de un dispositivo.
 * ?action=get_logs&mac=XX:XX&token=yyy&limit=50
 */
function handleGetLogs(mac, token, limit) {
  const userId = validateToken(token);
  if (!userId) return errorResponse('No autorizado', 'UNAUTHORIZED');
  if (!mac)    return errorResponse('mac es requerido', 'MISSING_MAC');

  const sheet = getSheet(SHEET_NAMES.DEVICE_LOG);
  const rows  = getAllRows(sheet).filter(r => String(r[COL.LOG.MAC_ADDRESS - 1]).toUpperCase() === mac);

  // Más recientes primero, máximo limit
  const recent = rows.reverse().slice(0, limit || DEFAULTS.MAX_LOG_ROWS);

  const logs = recent.map(r => ({
    log_id:       String(r[COL.LOG.LOG_ID       - 1]),
    event_type:   String(r[COL.LOG.EVENT_TYPE   - 1]),
    event_ts:     String(r[COL.LOG.EVENT_TS     - 1]),
    event_value:  String(r[COL.LOG.EVENT_VALUE  - 1]),
    details_json: String(r[COL.LOG.DETAILS_JSON - 1]),
    source:       String(r[COL.LOG.SOURCE       - 1])
  }));

  return successResponse({ logs: logs, total: logs.length });
}
