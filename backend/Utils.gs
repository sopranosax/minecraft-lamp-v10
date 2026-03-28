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

/** Actualiza una celda forzando formato texto (para IPs, MACs, etc.) */
function setCellText(sheet, rowNumber, colIndex, value) {
  const cell = sheet.getRange(rowNumber, colIndex);
  cell.setNumberFormat('@');   // Forzar tipo texto
  cell.setValue(String(value));
}

/**
 * Formatea una MAC address a XX:XX:XX:XX:XX:XX.
 * Si Google Sheets la interpretó como hora (ej: "00:00:00"), o se perdieron
 * los delimitadores, intenta reconstruirla desde los datos crudos.
 */
function formatMac(raw) {
  if (!raw) return '';
  var s = String(raw).trim();
  // Si ya tiene formato correcto (6 grupos de 2 hex separados por ':')
  if (/^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$/.test(s)) return s.toUpperCase();
  // Quitar delimitadores y ver si queda un hex de 12 chars
  var hex = s.replace(/[:\-\.]/g, '');
  if (/^[0-9A-Fa-f]{12}$/.test(hex)) {
    return hex.match(/.{2}/g).join(':').toUpperCase();
  }
  // Fallback: devolver tal cual
  return s;
}

/**
 * Formatea una IP a formato dotted-decimal estándar.
 * Maneja el caso donde Google Sheets almacenó "192168100219" sin puntos.
 */
function formatIp(raw) {
  if (!raw) return '';
  var s = String(raw).trim();
  // Si ya tiene puntos y parece IP válida
  if (/^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$/.test(s)) return s;
  // Si es un número largo sin puntos (9-12 dígitos), intentar reconstruir
  // Usamos heurística: probar todas las particiones en 4 octetos válidos
  if (/^\d{4,12}$/.test(s)) {
    var result = _parseIpDigits(s, [], 0);
    if (result) return result;
  }
  // Fallback
  return s;
}

/**
 * Recursivamente intenta dividir una cadena de dígitos en 4 octetos IP válidos (0-255).
 */
function _parseIpDigits(digits, octets, pos) {
  if (octets.length === 4) {
    return pos === digits.length ? octets.join('.') : null;
  }
  for (var len = 1; len <= 3 && pos + len <= digits.length; len++) {
    var part = digits.substring(pos, pos + len);
    // No permitir octetos con cero líder (excepto "0" solo)
    if (part.length > 1 && part[0] === '0') continue;
    var num = parseInt(part, 10);
    if (num <= 255) {
      var remaining = 4 - octets.length - 1;
      var digitsLeft = digits.length - pos - len;
      // Podar: necesitamos al menos 'remaining' dígitos y como máximo 'remaining*3'
      if (digitsLeft >= remaining && digitsLeft <= remaining * 3) {
        octets.push(part);
        var result = _parseIpDigits(digits, octets, pos + len);
        if (result) return result;
        octets.pop();
      }
    }
  }
  return null;
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
    manual_brightness:                 Number(row[COL.DEVICES.MANUAL_BRIGHTNESS - 1])  || DEFAULTS.MANUAL_BRIGHTNESS,
    motion_mode:                       String(row[COL.DEVICES.MOTION_MODE - 1])         || DEFAULTS.MOTION_MODE,
    motion_blink_seconds:              Number(row[COL.DEVICES.MOTION_BLINK_SECONDS - 1])|| DEFAULTS.MOTION_BLINK_SECONDS,
    schedule_enabled:                  String(row[COL.DEVICES.SCHEDULE_ENABLED - 1])    === 'TRUE',
    schedule_start_time:               String(row[COL.DEVICES.SCHEDULE_START_TIME - 1]) || '',
    schedule_end_time:                 String(row[COL.DEVICES.SCHEDULE_END_TIME - 1])   || '',
    schedule_color_hex:                String(row[COL.DEVICES.SCHEDULE_COLOR_HEX - 1])  || '',
    schedule_brightness:               Number(row[COL.DEVICES.SCHEDULE_BRIGHTNESS - 1]) || DEFAULTS.SCHEDULE_BRIGHTNESS,
    reconnect_interval_seconds:        Number(row[COL.DEVICES.RECONNECT_INTERVAL_SECONDS - 1]) || DEFAULTS.RECONNECT_INTERVAL_SECONDS,
    recovery_bootstrap_every_n_cycles: Number(row[COL.DEVICES.RECOVERY_BOOTSTRAP_EVERY_N_CYCLES - 1]) || DEFAULTS.RECOVERY_BOOTSTRAP_EVERY_N_CYCLES
  };
}
