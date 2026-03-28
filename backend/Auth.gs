// ============================================================
// Auth.gs — Autenticación de usuarios de la web app
// IoT Lámpara WiFi v10
// ============================================================

function handleLogin(body) {
  const { login, password } = body;
  if (!login || !password) return errorResponse('login y password son requeridos', 'MISSING_FIELDS');

  const lock = LockService.getScriptLock();
  lock.waitLock(10000);
  try {
    const sheet = getSheet(SHEET_NAMES.USERS);
    const found = findRowByCol(sheet, COL.USERS.LOGIN, login);
    if (!found) return errorResponse('Credenciales inválidas', 'AUTH_FAILED');

    const row = found.rowData;
    if (String(row[COL.USERS.STATUS - 1]) !== ST.ACTIVE)
      return errorResponse('Usuario inactivo', 'USER_INACTIVE');

    const inputHash  = hashPassword(password);
    const storedHash = String(row[COL.USERS.PASSWORD_HASH - 1]);
    if (inputHash !== storedHash) return errorResponse('Credenciales inválidas', 'AUTH_FAILED');

    // Generar token con expiración
    const token     = generateId();
    const expiresAt = new Date();
    expiresAt.setHours(expiresAt.getHours() + DEFAULTS.TOKEN_EXPIRES_HOURS);
    const nowTs = nowIso();

    setCell(sheet, found.rowNumber, COL.USERS.SESSION_TOKEN,    token);
    setCell(sheet, found.rowNumber, COL.USERS.TOKEN_EXPIRES_AT, expiresAt.toISOString());
    setCell(sheet, found.rowNumber, COL.USERS.LAST_LOGIN_AT,    nowTs);
    setCell(sheet, found.rowNumber, COL.USERS.UPDATED_AT,       nowTs);

    return successResponse({
      token: token,
      expires_at: expiresAt.toISOString(),
      user: {
        user_id: String(row[COL.USERS.USER_ID - 1]),
        login:   login
      }
    });
  } finally {
    lock.releaseLock();
  }
}
