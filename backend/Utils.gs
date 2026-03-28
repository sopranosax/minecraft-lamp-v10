// ============================================================
// Utils.gs — Funciones helpers compartidas
// IoT Lámpara WiFi v10
// ============================================================

function generateId() {
  return Utilities.getUuid();
}

function nowIso() {
  return new Date().toISOString();
}

/** SHA-256 del password en hex */
function hashPassword(password) {
  const bytes = Utilities.computeDigest(
    Utilities.DigestAlgorithm.SHA_256,
    password,
    Utilities.Charset.UTF_8
  );
  return bytes.map(b => ('0' + (b & 0xFF).toString(16)).slice(-2)).join('');
}

/** Respuesta JSON estándar */
function jsonResponse(data) {
  return ContentService
    .createTextOutput(JSON.stringify(data))
    .setMimeType(ContentService.MimeType.JSON);
}

function successResponse(data) {
  return jsonResponse(Object.assign({ success: true }, data || {}));
}

function errorResponse(message, code) {
  return jsonResponse({ success: false, error: message, code: code || 'ERROR' });
}

/** Obtiene hoja por nombre */
function getSheet(name) {
  return SpreadsheetApp.getActiveSpreadsheet().getSheetByName(name);
}

/** Devuelve todas las filas de datos (sin encabezado) */
function getAllRows(sheet) {
  const data = sheet.getDataRange().getValues();
  return data.length <= 1 ? [] : data.slice(1);
}

/**
 * Busca la primera fila donde colIndex coincide con value.
 * @returns { rowData, rowNumber } o null
 */
function findRowByCol(sheet, colIndex, value) {
  const rows = getAllRows(sheet);
  for (let i = 0; i < rows.length; i++) {
    if (String(rows[i][colIndex - 1]).trim() === String(value).trim()) {
      return { rowData: rows[i], rowNumber: i + 2 }; // +2: 1-based + header
    }
  }
  return null;
}

/** Actualiza una celda específica */
function setCell(sheet, rowNumber, colIndex, value) {
  sheet.getRange(rowNumber, colIndex).setValue(value);
}

/** Valida token y retorna userId o null */
function validateToken(token) {
  if (!token) return null;
  const sheet = getSheet(SHEET_NAMES.USERS);
  const rows  = getAllRows(sheet);
  for (let i = 0; i < rows.length; i++) {
    const row = rows[i];
    if (String(row[COL.USERS.SESSION_TOKEN - 1]) === String(token)) {
      const expires = new Date(row[COL.USERS.TOKEN_EXPIRES_AT - 1]);
      if (expires > new Date()) {
        return String(row[COL.USERS.USER_ID - 1]);
      }
    }
  }
  return null;
}

/** Verifica si un dispositivo está online según threshold de 5 min */
function isDeviceOnline(lastSeenAtStr) {
  if (!lastSeenAtStr) return false;
  const lastSeen = new Date(lastSeenAtStr);
  const threshold = new Date();
  threshold.setMinutes(threshold.getMinutes() - DEFAULTS.OFFLINE_THRESHOLD_MINUTES);
  return lastSeen > threshold;
}

/** Parsea body de POST: intenta JSON, luego usa e.parameter */
function parseBody(e) {
  if (e.postData && e.postData.contents) {
    try { return JSON.parse(e.postData.contents); } catch (_) {}
  }
  return e.parameter || {};
}

/** Construye objeto de configuración del dispositivo desde una fila de DEVICES */
function buildDeviceConfig(row) {
  return {
    manual_power_state:                String(row[COL.DEVICES.MANUAL_POWER_STATE - 1]) || DEFAULTS.MANUAL_POWER_STATE,
    manual_color_hex:                  String(row[COL.DEVICES.MANUAL_COLOR_HEX - 1])   || DEFAULTS.MANUAL_COLOR_HEX,
    motion_mode:                       String(row[COL.DEVICES.MOTION_MODE - 1])         || DEFAULTS.MOTION_MODE,
    motion_blink_seconds:              Number(row[COL.DEVICES.MOTION_BLINK_SECONDS - 1])|| DEFAULTS.MOTION_BLINK_SECONDS,
    schedule_enabled:                  String(row[COL.DEVICES.SCHEDULE_ENABLED - 1])    === 'TRUE',
    schedule_start_time:               String(row[COL.DEVICES.SCHEDULE_START_TIME - 1]) || '',
    schedule_end_time:                 String(row[COL.DEVICES.SCHEDULE_END_TIME - 1])   || '',
    schedule_color_hex:                String(row[COL.DEVICES.SCHEDULE_COLOR_HEX - 1])  || '',
    reconnect_interval_seconds:        Number(row[COL.DEVICES.RECONNECT_INTERVAL_SECONDS - 1]) || DEFAULTS.RECONNECT_INTERVAL_SECONDS,
    recovery_bootstrap_every_n_cycles: Number(row[COL.DEVICES.RECOVERY_BOOTSTRAP_EVERY_N_CYCLES - 1]) || DEFAULTS.RECOVERY_BOOTSTRAP_EVERY_N_CYCLES
  };
}
