// ============================================================
// Setup.gs — Inicialización del Spreadsheet
// Ejecutar initializeSpreadsheet() UNA SOLA VEZ desde el editor
// IoT Lámpara WiFi v10
// ============================================================

/**
 * Crea todas las hojas con sus encabezados.
 * Si la hoja ya existe, solo verifica/agrega headers faltantes.
 * EJECUTAR UNA SOLA VEZ desde el editor de Apps Script.
 */
function initializeSpreadsheet() {
  const ss = SpreadsheetApp.getActiveSpreadsheet();

  Object.keys(HEADERS).forEach(sheetKey => {
    const sheetName = SHEET_NAMES[sheetKey] || sheetKey;
    let sheet = ss.getSheetByName(sheetName);
    if (!sheet) {
      sheet = ss.insertSheet(sheetName);
      Logger.log('Hoja creada: ' + sheetName);
    }
    // Escribir encabezados si la primera fila está vacía
    const firstRow = sheet.getRange(1, 1, 1, HEADERS[sheetKey].length).getValues()[0];
    if (!firstRow[0]) {
      sheet.getRange(1, 1, 1, HEADERS[sheetKey].length).setValues([HEADERS[sheetKey]]);
      sheet.getRange(1, 1, 1, HEADERS[sheetKey].length)
        .setFontWeight('bold')
        .setBackground('#1a1a2e')
        .setFontColor('#ffffff');
      Logger.log('Headers escritos en: ' + sheetName);
    }
  });

  Logger.log('✅ Spreadsheet inicializado correctamente.');
  SpreadsheetApp.getUi().alert('✅ Hojas creadas con éxito. Ahora inserta el primer usuario con createInitialUser().');
}

/**
 * Crea el usuario administrador inicial.
 * Modifica LOGIN y PASSWORD antes de ejecutar.
 * EJECUTAR UNA SOLA VEZ.
 */
function createInitialUser() {
  const LOGIN    = 'admin';        // ← cambia esto
  const PASSWORD = 'lampara123';   // ← cambia esto

  const sheet = getSheet(SHEET_NAMES.USERS);
  const existing = findRowByCol(sheet, COL.USERS.LOGIN, LOGIN);
  if (existing) {
    SpreadsheetApp.getUi().alert('⚠️ El usuario "' + LOGIN + '" ya existe.');
    return;
  }

  const nowTs = nowIso();
  const row = new Array(9).fill('');
  row[COL.USERS.USER_ID       - 1] = generateId();
  row[COL.USERS.LOGIN         - 1] = LOGIN;
  row[COL.USERS.PASSWORD_HASH - 1] = hashPassword(PASSWORD);
  row[COL.USERS.STATUS        - 1] = ST.ACTIVE;
  row[COL.USERS.CREATED_AT    - 1] = nowTs;
  row[COL.USERS.UPDATED_AT    - 1] = nowTs;

  sheet.appendRow(row);
  Logger.log('✅ Usuario creado: ' + LOGIN);
  SpreadsheetApp.getUi().alert('✅ Usuario "' + LOGIN + '" creado con éxito.');
}

/**
 * Muestra la URL de deployment del script (para copiar en la web app).
 * Requiere que el script esté deployado como Web App.
 */
function showDeploymentUrl() {
  const url = ScriptApp.getService().getUrl();
  SpreadsheetApp.getUi().alert('URL del Backend:\n\n' + url);
}
