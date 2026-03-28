// ============================================================
// api.js — Capa de comunicación con el backend Apps Script
// IoT Lámpara WiFi v10
// ============================================================

const API = (() => {
  const url = () => CONFIG.BACKEND_URL;
  const token = () => sessionStorage.getItem('token') || '';

  // ── GET genérico ───────────────────────────────────────────
  async function get(params) {
    const qs = new URLSearchParams({ ...params, token: token() }).toString();
    const res = await fetch(`${url()}?${qs}`);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.json();
  }

  // ── POST genérico ────────────────────────────────────────────
  // Sin Content-Type header → "simple request" → sin preflight CORS
  // Apps Script recibe el body via e.postData.contents de igual forma
  async function post(action, data = {}) {
    const res = await fetch(`${url()}?action=${action}`, {
      method: 'POST',
      body:   JSON.stringify({ ...data, action, token: token() })
    });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.json();
  }

  // ── Auth ───────────────────────────────────────────────────
  async function login(loginVal, password) {
    const data = await post('login', { login: loginVal, password });
    if (data.success) {
      sessionStorage.setItem('token', data.token);
      sessionStorage.setItem('user',  JSON.stringify(data.user));
    }
    return data;
  }

  function logout() {
    sessionStorage.removeItem('token');
    sessionStorage.removeItem('user');
  }

  // ── Dispositivos ───────────────────────────────────────────
  const getDevices = () => get({ action: 'get_devices' });
  const getDevice  = (mac) => get({ action: 'get_device', mac });
  const getScan    = (mac) => get({ action: 'get_scan',   mac });
  const getLogs    = (mac, limit = 50) => get({ action: 'get_logs', mac, limit });

  // ── WiFi config (web app) ──────────────────────────────────
  const setWifiConfig = (mac, ssid, password) =>
    post('wifi_config', { mac, ssid, password });

  // ── Control manual ─────────────────────────────────────────
  const setControl = (mac, manualPowerState, manualColorHex) =>
    post('control', { mac, manual_power_state: manualPowerState, manual_color_hex: manualColorHex });

  // ── Movimiento ─────────────────────────────────────────────
  const setMotionConfig = (mac, motionMode, motionBlinkSeconds) =>
    post('motion_config', { mac, motion_mode: motionMode, motion_blink_seconds: motionBlinkSeconds });

  // ── Horario ────────────────────────────────────────────────
  const setSchedule = (mac, enabled, startTime, endTime, colorHex) =>
    post('schedule', {
      mac,
      schedule_enabled:    enabled,
      schedule_start_time: startTime,
      schedule_end_time:   endTime,
      schedule_color_hex:  colorHex
    });

  return { login, logout, getDevices, getDevice, getScan, getLogs,
           setWifiConfig, setControl, setMotionConfig, setSchedule };
})();
