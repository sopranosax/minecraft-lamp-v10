// ============================================================
// ControlService.gs — Control manual, movimiento y horario
// IoT Lámpara WiFi v10
// ============================================================

// ─── Control manual ON/OFF + color (POST action=control) ─────
function handleControl(mac, body) {
  const userId = validateToken(body.token);
  if (!userId) return errorResponse('No autorizado', 'UNAUTHORIZED');
  if (!mac)    return errorResponse('mac es requerido', 'MISSING_MAC');

  const { manual_power_state, manual_color_hex } = body;
  if (!manual_power_state && !manual_color_hex)
    return errorResponse('Se requiere manual_power_state y/o manual_color_hex', 'MISSING_FIELDS');

  const lock = LockService.getScriptLock();
  lock.waitLock(10000);
  try {
    const sheet = getSheet(SHEET_NAMES.DEVICES);
    const found = findRowByCol(sheet, COL.DEVICES.MAC_ADDRESS, mac);
    if (!found) return errorResponse('Dispositivo no encontrado', 'NOT_FOUND');

    const nowTs = nowIso();
    if (manual_power_state) {
      const ps = manual_power_state.toUpperCase();
      if (ps !== ST.ON && ps !== ST.OFF)
        return errorResponse('manual_power_state debe ser ON o OFF', 'INVALID_VALUE');
      setCell(sheet, found.rowNumber, COL.DEVICES.MANUAL_POWER_STATE, ps);
      addLog(mac, ps === ST.ON ? EV.POWER_ON : EV.POWER_OFF, nowTs, ps, '', 'WEBAPP');
    }
    if (manual_color_hex) {
      if (!/^#[0-9A-Fa-f]{6}$/.test(manual_color_hex))
        return errorResponse('manual_color_hex debe tener formato #RRGGBB', 'INVALID_COLOR');
      setCell(sheet, found.rowNumber, COL.DEVICES.MANUAL_COLOR_HEX, manual_color_hex.toUpperCase());
      addLog(mac, EV.COLOR_CHANGED, nowTs, manual_color_hex, '', 'WEBAPP');
    }
    setCell(sheet, found.rowNumber, COL.DEVICES.UPDATED_AT, nowTs);

    return successResponse({ updated: true, manual_power_state, manual_color_hex });
  } finally {
    lock.releaseLock();
  }
}

// ─── Config de movimiento PIR (POST action=motion_config) ─────
function handleMotionConfig(mac, body) {
  const userId = validateToken(body.token);
  if (!userId) return errorResponse('No autorizado', 'UNAUTHORIZED');
  if (!mac)    return errorResponse('mac es requerido', 'MISSING_MAC');

  const { motion_mode, motion_blink_seconds } = body;
  if (!motion_mode) return errorResponse('motion_mode es requerido', 'MISSING_MODE');

  const mode = motion_mode.toUpperCase();
  if (mode !== ST.BLINK && mode !== ST.NO_BLINK)
    return errorResponse('motion_mode debe ser BLINK o NO_BLINK', 'INVALID_MODE');
  if (mode === ST.BLINK) {
    const secs = Number(motion_blink_seconds);
    if (!secs || secs <= 0)
      return errorResponse('motion_blink_seconds debe ser > 0 cuando mode=BLINK', 'INVALID_SECONDS');
  }

  const lock = LockService.getScriptLock();
  lock.waitLock(10000);
  try {
    const sheet = getSheet(SHEET_NAMES.DEVICES);
    const found = findRowByCol(sheet, COL.DEVICES.MAC_ADDRESS, mac);
    if (!found) return errorResponse('Dispositivo no encontrado', 'NOT_FOUND');

    const nowTs = nowIso();
    setCell(sheet, found.rowNumber, COL.DEVICES.MOTION_MODE,          mode);
    setCell(sheet, found.rowNumber, COL.DEVICES.MOTION_BLINK_SECONDS, Number(motion_blink_seconds) || 0);
    setCell(sheet, found.rowNumber, COL.DEVICES.UPDATED_AT,           nowTs);
    addLog(mac, 'MOTION_CONFIG_UPDATED', nowTs,
           mode + (mode === ST.BLINK ? ':' + motion_blink_seconds + 's' : ''), '', 'WEBAPP');

    return successResponse({ motion_mode: mode, motion_blink_seconds: Number(motion_blink_seconds) || 0 });
  } finally {
    lock.releaseLock();
  }
}

// ─── Programación horaria (POST action=schedule) ──────────────
function handleSchedule(mac, body) {
  const userId = validateToken(body.token);
  if (!userId) return errorResponse('No autorizado', 'UNAUTHORIZED');
  if (!mac)    return errorResponse('mac es requerido', 'MISSING_MAC');

  const { schedule_enabled, schedule_start_time, schedule_end_time, schedule_color_hex } = body;

  const enabled = String(schedule_enabled).toUpperCase() === 'TRUE' || schedule_enabled === true;

  // Si se activa, validar campos requeridos
  if (enabled) {
    if (!schedule_start_time || !schedule_end_time || !schedule_color_hex)
      return errorResponse('start_time, end_time y color_hex son requeridos cuando schedule_enabled=true', 'MISSING_FIELDS');
    if (!/^\d{2}:\d{2}$/.test(schedule_start_time) || !/^\d{2}:\d{2}$/.test(schedule_end_time))
      return errorResponse('Formato de hora inválido, usar HH:MM', 'INVALID_TIME_FORMAT');
    if (!/^#[0-9A-Fa-f]{6}$/.test(schedule_color_hex))
      return errorResponse('schedule_color_hex debe tener formato #RRGGBB', 'INVALID_COLOR');
  }

  const lock = LockService.getScriptLock();
  lock.waitLock(10000);
  try {
    const sheet = getSheet(SHEET_NAMES.DEVICES);
    const found = findRowByCol(sheet, COL.DEVICES.MAC_ADDRESS, mac);
    if (!found) return errorResponse('Dispositivo no encontrado', 'NOT_FOUND');

    const nowTs = nowIso();
    setCell(sheet, found.rowNumber, COL.DEVICES.SCHEDULE_ENABLED,    enabled ? 'TRUE' : 'FALSE');
    if (enabled) {
      setCell(sheet, found.rowNumber, COL.DEVICES.SCHEDULE_START_TIME,  schedule_start_time);
      setCell(sheet, found.rowNumber, COL.DEVICES.SCHEDULE_END_TIME,    schedule_end_time);
      setCell(sheet, found.rowNumber, COL.DEVICES.SCHEDULE_COLOR_HEX,   schedule_color_hex.toUpperCase());
    }
    setCell(sheet, found.rowNumber, COL.DEVICES.UPDATED_AT, nowTs);
    addLog(mac, EV.SCHEDULE_UPDATED, nowTs,
           enabled ? schedule_start_time + '-' + schedule_end_time : 'DISABLED', '', 'WEBAPP');

    return successResponse({
      schedule_enabled: enabled,
      schedule_start_time: schedule_start_time || '',
      schedule_end_time:   schedule_end_time   || '',
      schedule_color_hex:  schedule_color_hex  || ''
    });
  } finally {
    lock.releaseLock();
  }
}
