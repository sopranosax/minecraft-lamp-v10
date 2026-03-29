// ============================================================
// app.js — Lógica SPA: router, vistas y eventos UI
// IoT Lámpara WiFi v10
// ============================================================

// ── Estado global ──────────────────────────────────────────────
const App = {
  currentMac:    null,
  currentDevice: null,
  refreshTimer:  null,
  selectedSsid:  null,
  wifiLoaded:    false
};

// ── Helpers DOM ────────────────────────────────────────────────
const $  = (id) => document.getElementById(id);
const qs = (sel, ctx = document) => ctx.querySelector(sel);

function showView(id) {
  document.querySelectorAll('.view').forEach(v => v.classList.remove('active'));
  $(id).classList.add('active');
}

function toast(msg, type = '') {
  const el = $('toast');
  el.textContent = msg;
  el.className = `toast show ${type}`;
  setTimeout(() => el.classList.remove('show'), 3000);
}

function setLoading(btn, loading) {
  btn.disabled = loading;
  const text    = btn.querySelector('.btn-text');
  const spinner = btn.querySelector('.btn-spinner');
  if (text)    text.classList.toggle('hidden', loading);
  if (spinner) spinner.classList.toggle('hidden', !loading);
}

function formatDate(isoStr) {
  if (!isoStr) return '—';
  try {
    return new Date(isoStr).toLocaleString('es-CL', { dateStyle: 'short', timeStyle: 'short' });
  } catch { return isoStr; }
}

function rssiToSignal(rssi) {
  const r = Number(rssi);
  if (r >= -55) return 4;
  if (r >= -67) return 3;
  if (r >= -78) return 2;
  return 1;
}

function signalBars(rssi) {
  const s = rssiToSignal(rssi);
  return `<div class="signal-bars">
    ${[1,2,3,4].map(i => `<div class="signal-bar${i<=s?' lit':''}" style="height:${i*4}px"></div>`).join('')}
  </div>`;
}

// ── Toggle acordeón ────────────────────────────────────────────
document.querySelectorAll('.card-header[data-toggle]').forEach(header => {
  const bodyId = header.dataset.toggle;
  header.addEventListener('click', () => {
    const body = $(bodyId);
    if (!body) return;
    const isOpen = !body.classList.contains('collapsed');
    body.classList.toggle('collapsed', isOpen);
    header.classList.toggle('open', !isOpen);
  });
  // Abrir por defecto excepto logs
  if (bodyId !== 'logs-body') header.classList.add('open');
});

// ─────────────────────────────────────────────────────────────
// VISTA: LOGIN
// ─────────────────────────────────────────────────────────────
$('form-login').addEventListener('submit', async (e) => {
  e.preventDefault();
  const btn    = $('btn-login');
  const errEl  = $('login-error');
  errEl.classList.add('hidden');
  setLoading(btn, true);

  try {
    const res = await API.login($('login-user').value.trim(), $('login-pass').value);
    if (res.success) {
      showView('view-devices');
      loadDevices();
    } else {
      errEl.textContent = res.error || 'Credenciales inválidas.';
      errEl.classList.remove('hidden');
    }
  } catch (err) {
    errEl.textContent = 'Error de conexión. Revisa la URL del backend.';
    errEl.classList.remove('hidden');
  } finally {
    setLoading(btn, false);
  }
});

// ─────────────────────────────────────────────────────────────
// VISTA: LISTA DE DISPOSITIVOS
// ─────────────────────────────────────────────────────────────
$('btn-refresh').addEventListener('click', loadDevices);
$('btn-logout').addEventListener('click', () => {
  API.logout();
  clearInterval(App.refreshTimer);
  showView('view-login');
});

async function loadDevices() {
  $('devices-loading').classList.remove('hidden');
  $('devices-empty').classList.add('hidden');
  $('devices-list').innerHTML = '';

  try {
    const res = await API.getDevices();
    $('devices-loading').classList.add('hidden');

    if (!res.success || !res.devices?.length) {
      $('devices-empty').classList.remove('hidden');
      return;
    }

    res.devices.forEach(dev => {
      const isOn     = dev.manual_power_state === 'ON';
      const isOnline = dev.status === 'ONLINE';
      const card = document.createElement('div');
      card.className = 'device-card';
      card.dataset.mac = dev.mac_address;
      card.innerHTML = `
        <div class="device-led ${isOn ? 'on' : 'off'}">💡</div>
        <div class="device-info">
          <div class="device-name">${dev.device_alias || dev.mac_address}</div>
          <div class="device-mac">${dev.mac_address}</div>
          <div class="device-meta">
            <span class="status-pill ${isOnline ? 'online' : 'offline'}">
              ${isOnline ? 'Online' : 'Offline'}
            </span>
            &nbsp;${dev.current_wifi_ssid ? '📶 ' + dev.current_wifi_ssid : ''}
            &nbsp;${formatDate(dev.last_seen_at)}
          </div>
        </div>
      `;
      card.addEventListener('click', () => openDevice(dev.mac_address));
      $('devices-list').appendChild(card);
    });
  } catch (err) {
    $('devices-loading').classList.add('hidden');
    toast('Error al cargar dispositivos: ' + err.message, 'error');
  }
}

// ─────────────────────────────────────────────────────────────
// VISTA: DETALLE DEL DISPOSITIVO
// ─────────────────────────────────────────────────────────────
$('btn-back').addEventListener('click', () => {
  clearInterval(App.refreshTimer);
  _stopWifiPoll();
  showView('view-devices');
  loadDevices();
});

async function openDevice(mac) {
  App.currentMac = mac;
  App.wifiLoaded = false;
  showView('view-detail');
  await refreshDeviceDetail();
  loadWifiScan();
  App.refreshTimer = setInterval(refreshDeviceDetail, CONFIG.REFRESH_INTERVAL_MS);
}

async function refreshDeviceDetail() {
  try {
    const res = await API.getDevice(App.currentMac);
    if (!res.success) { toast('Error al cargar dispositivo', 'error'); return; }
    const dev = res.device;
    App.currentDevice = dev;
    _renderDeviceDetail(dev);
  } catch (err) {
    toast('Sin conexión al backend', 'error');
  }
}

function _renderDeviceDetail(dev) {
  const cfg    = dev.config || {};
  const online = dev.status === 'ONLINE';

  $('detail-title').textContent        = dev.device_alias || dev.mac_address;
  $('detail-status-badge').textContent = online ? 'Online' : 'Offline';
  $('detail-status-badge').className   = `status-badge ${online ? 'online' : 'offline'}`;

  $('d-mac').textContent      = dev.mac_address        || '—';
  $('d-ssid').textContent     = dev.current_wifi_ssid  || '—';
  $('d-fw').textContent       = dev.firmware_version ? 'v' + dev.firmware_version : '—';
  $('d-lastseen').textContent = formatDate(dev.last_seen_at);
  $('d-ip').textContent       = dev.last_ip            || '—';
  $('d-wifierr').textContent  = dev.last_wifi_error    || '—';
  $('d-offline-warn').classList.toggle('hidden', online);

  // Update WiFi current connection indicator
  if (dev.current_wifi_ssid) {
    $('wifi-current-ssid').textContent = dev.current_wifi_ssid;
    $('wifi-current').classList.remove('hidden');
  } else {
    $('wifi-current').classList.add('hidden');
  }

  // ── Control manual
  $('ctrl-power').checked = cfg.manual_power_state === 'ON';
  const color = (cfg.manual_color_hex || '#ffffff').toUpperCase();
  $('ctrl-color').value  = color.startsWith('#') ? color : '#' + color;
  $('ctrl-color-hex').textContent = color;
  const manBri = cfg.manual_brightness !== undefined ? cfg.manual_brightness : 200;
  const manPct = Math.round(manBri / 255 * 100);
  $('ctrl-brightness').value = manBri;
  $('ctrl-bri-val').textContent = manPct + '%';
  $('ctrl-brightness').style.setProperty('--val', manPct + '%');

  // ── Movimiento
  const mode = cfg.motion_mode || 'NO_BLINK';
  document.querySelector(`input[name="motion-mode"][value="${mode}"]`).checked = true;
  $('motion-secs').value = cfg.motion_blink_seconds || 5;
  $('blink-seconds-row').classList.toggle('hidden', mode !== 'BLINK');

  // -- Horario
  const schedOn = cfg.schedule_enabled === true || cfg.schedule_enabled === 'true';
  $('sched-enabled').checked = schedOn;
  $('sched-fields').classList.toggle('hidden', !schedOn);

  const hasSchedData = cfg.schedule_start_time && cfg.schedule_end_time;
  if (hasSchedData) {
    const schBri = cfg.schedule_brightness !== undefined ? cfg.schedule_brightness : 200;
    $('sched-start').value           = cfg.schedule_start_time || '';
    $('sched-end').value             = cfg.schedule_end_time   || '';
    const sc = (cfg.schedule_color_hex || '#ff6b00').toUpperCase();
    $('sched-color').value           = sc.startsWith('#') ? sc : '#ffffff';
    $('sched-color-hex').textContent = sc;
    $('sched-brightness').value = schBri;
    const schPct = Math.round(schBri / 255 * 100);
    $('sched-bri-val').textContent = schPct + '%';
    $('sched-brightness').style.setProperty('--val', schPct + '%');
    const tag = schedOn ? '[ON]' : '[OFF]';
    $('sched-summary').classList.remove('hidden');
    $('sched-summary').textContent = `${tag}  ${cfg.schedule_start_time} -> ${cfg.schedule_end_time}  ${sc}  ${schPct}%`;
  } else {
    $('sched-summary').classList.add('hidden');
  }
}

// ── Color pickers y sliders sincronizados ─────────────────────
$('ctrl-color').addEventListener('input', () => {
  $('ctrl-color-hex').textContent = $('ctrl-color').value.toUpperCase();
});
$('sched-color').addEventListener('input', () => {
  $('sched-color-hex').textContent = $('sched-color').value.toUpperCase();
});
$('ctrl-brightness').addEventListener('input', () => {
  const pct = Math.round($('ctrl-brightness').value / 255 * 100);
  $('ctrl-bri-val').textContent = pct + '%';
  $('ctrl-brightness').style.setProperty('--val', pct + '%');
});
$('sched-brightness').addEventListener('input', () => {
  const pct = Math.round($('sched-brightness').value / 255 * 100);
  $('sched-bri-val').textContent = pct + '%';
  $('sched-brightness').style.setProperty('--val', pct + '%');
});

// ── Botón control manual ───────────────────────────────────────
$('btn-control-save').addEventListener('click', async () => {
  const btn    = $('btn-control-save');
  const power  = $('ctrl-power').checked ? 'ON' : 'OFF';
  const color  = $('ctrl-color').value.toUpperCase();
  const brightness = parseInt($('ctrl-brightness').value);
  btn.disabled = true;
  btn.textContent = 'Guardando…';
  try {
    const res = await API.setControl(App.currentMac, power, color, brightness);
    toast(res.success ? '✅ Control actualizado' : '❌ ' + res.error, res.success ? 'success' : 'error');
    if (res.success) await refreshDeviceDetail();
  } catch (err) {
    toast('❌ Error: ' + err.message, 'error');
  } finally {
    btn.disabled = false;
    btn.textContent = 'Guardar control';
  }
});

// ── Radio motion mode ──────────────────────────────────────────
document.querySelectorAll('input[name="motion-mode"]').forEach(radio => {
  radio.addEventListener('change', () => {
    $('blink-seconds-row').classList.toggle('hidden', radio.value !== 'BLINK');
  });
});

// ── Botón movimiento ───────────────────────────────────────────
$('btn-motion-save').addEventListener('click', async () => {
  const btn  = $('btn-motion-save');
  const mode = document.querySelector('input[name="motion-mode"]:checked')?.value || 'NO_BLINK';
  const secs = parseInt($('motion-secs').value);

  if (mode === 'BLINK' && (!secs || secs <= 0)) {
    toast('⚠️ Ingresa segundos válidos (> 0)', 'error'); return;
  }
  btn.disabled = true; btn.textContent = 'Guardando…';
  try {
    const res = await API.setMotionConfig(App.currentMac, mode, secs);
    toast(res.success ? '✅ Movimiento actualizado' : '❌ ' + res.error, res.success ? 'success' : 'error');
  } catch (err) {
    toast('❌ Error: ' + err.message, 'error');
  } finally {
    btn.disabled = false; btn.textContent = 'Guardar movimiento';
  }
});

// ── Toggle horario ─────────────────────────────────────────────
$('sched-enabled').addEventListener('change', () => {
  $('sched-fields').classList.toggle('hidden', !$('sched-enabled').checked);
});

// ── Botón horario ──────────────────────────────────────────────
$('btn-sched-save').addEventListener('click', async () => {
  const btn        = $('btn-sched-save');
  const enabled    = $('sched-enabled').checked;
  const start      = $('sched-start').value;
  const end        = $('sched-end').value;
  const color      = $('sched-color').value.toUpperCase();
  const brightness = parseInt($('sched-brightness').value);

  if (enabled && (!start || !end)) {
    toast('⚠️ Ingresa hora de inicio y fin', 'error'); return;
  }
  btn.disabled = true; btn.textContent = 'Guardando…';
  try {
    const res = await API.setSchedule(App.currentMac, enabled, start, end, color, brightness);
    toast(res.success ? '✅ Horario actualizado' : '❌ ' + res.error, res.success ? 'success' : 'error');
    if (res.success) await refreshDeviceDetail();
  } catch (err) {
    toast('❌ Error: ' + err.message, 'error');
  } finally {
    btn.disabled = false; btn.textContent = 'Guardar horario';
  }
});

// ── Sección WiFi ───────────────────────────────────────────────
let _wifiPollTimer = null; // connection status polling timer

// Renders WiFi scan data from the device detail (no separate API call needed).
// Called after refreshDeviceDetail() which populates App.currentDevice.wifi_scan.
function loadWifiScan() {
  $('wifi-no-scan').classList.add('hidden');
  $('wifi-scan-list').innerHTML = '';
  $('wifi-scan-ts').classList.add('hidden');
  $('wifi-form').classList.add('hidden');
  $('wifi-connect-progress').classList.add('hidden');
  $('wifi-loading').classList.add('hidden');
  App.selectedSsid = null;

  const dev = App.currentDevice;
  if (!dev) return;

  // Show current SSID
  if (dev.current_wifi_ssid) {
    $('wifi-current-ssid').textContent = dev.current_wifi_ssid;
    $('wifi-current').classList.remove('hidden');
  } else {
    $('wifi-current').classList.add('hidden');
  }

  // Read scan from device detail
  const networks = dev.wifi_scan || [];
  if (!networks.length) {
    $('wifi-no-scan').classList.remove('hidden');
    return;
  }

  // Show scan timestamp from the first entry
  const ts = networks[0].ts || networks[0].scanned_at;
  if (ts) {
    $('wifi-scan-time').textContent = formatDate(ts);
    $('wifi-scan-ts').classList.remove('hidden');
  }

  // Sort by signal strength descending
  const sorted = [...networks].sort((a, b) => (b.rssi || 0) - (a.rssi || 0));

  sorted.forEach(net => {
    const authType = (net.auth || net.security_type || '').toUpperCase();
    const isOpen = authType === 'OPEN';
    const item = document.createElement('div');
    item.className = 'scan-item';
    item.innerHTML = `
      <span class="scan-auth ${isOpen ? 'scan-auth-open' : 'scan-auth-lock'}">${isOpen ? '🔓' : '🔒'}</span>
      <div class="scan-ssid">${net.ssid}</div>
      <div class="scan-rssi">${net.rssi} dBm</div>
      ${signalBars(net.rssi)}
    `;
    item.addEventListener('click', () => {
      document.querySelectorAll('.scan-item').forEach(i => i.classList.remove('selected'));
      item.classList.add('selected');
      App.selectedSsid = net.ssid;
      App._selectedIsOpen = isOpen;
      $('selected-ssid').textContent = net.ssid;
      $('wifi-pass').value = '';
      $('wifi-form').classList.remove('hidden');
      $('wifi-status-msg').classList.add('hidden');
      $('wifi-connect-progress').classList.add('hidden');

      // Hide password field for OPEN networks
      if (isOpen) {
        $('wifi-pass-field').classList.add('hidden');
      } else {
        $('wifi-pass-field').classList.remove('hidden');
        $('wifi-pass').focus();
      }
    });
    $('wifi-scan-list').appendChild(item);
  });
}

// ── Password visibility toggle ───────────────────────────────
$('btn-wifi-eye').addEventListener('click', () => {
  const passInput = $('wifi-pass');
  const btn = $('btn-wifi-eye');
  if (passInput.type === 'password') {
    passInput.type = 'text';
    btn.classList.add('active');
    btn.textContent = '🙈';
  } else {
    passInput.type = 'password';
    btn.classList.remove('active');
    btn.textContent = '👁';
  }
});

$('btn-wifi-cancel').addEventListener('click', () => {
  $('wifi-form').classList.add('hidden');
  $('wifi-connect-progress').classList.add('hidden');
  App.selectedSsid = null;
  App._selectedIsOpen = false;
  _stopWifiPoll();
  document.querySelectorAll('.scan-item').forEach(i => i.classList.remove('selected'));
});

$('btn-wifi-save').addEventListener('click', async () => {
  const btn  = $('btn-wifi-save');
  const pass = $('wifi-pass').value;
  const msg  = $('wifi-status-msg');
  const isOpen = App._selectedIsOpen || false;

  if (!App.selectedSsid) { toast('⚠️ Selecciona una red', 'error'); return; }
  if (!isOpen && !pass)   { toast('⚠️ Ingresa la contraseña', 'error'); return; }

  btn.disabled = true; btn.textContent = 'Enviando…';
  msg.className = 'status-msg pending'; msg.textContent = '⏳ Enviando credenciales al dispositivo…'; msg.classList.remove('hidden');

  try {
    const res = await API.setWifiConfig(App.currentMac, App.selectedSsid, isOpen ? '' : pass);
    if (res.success) {
      msg.classList.add('hidden');
      toast('✅ Credenciales guardadas', 'success');
      $('wifi-form').classList.add('hidden');
      // Start polling for connection result
      _startWifiPoll(App.selectedSsid);
    } else {
      msg.className = 'status-msg error';
      msg.textContent = '❌ ' + (res.error || 'Error al guardar.');
    }
  } catch (err) {
    msg.className = 'status-msg error';
    msg.textContent = '❌ Error de conexión.';
  } finally {
    btn.disabled = false; btn.textContent = 'APLICAR';
  }
});

// ── WiFi connection status polling ──────────────────────────
function _startWifiPoll(targetSsid) {
  _stopWifiPoll();
  const progress = $('wifi-connect-progress');
  const bar      = $('wifi-progress-bar');
  const text     = $('wifi-progress-text');
  const result   = $('wifi-progress-result');

  progress.classList.remove('hidden');
  result.classList.add('hidden');
  bar.className = 'progress-bar-fill';
  bar.style.width = '0%';
  text.textContent = `⏳ Esperando que el dispositivo conecte a "${targetSsid}"...`;

  const POLL_INTERVAL = 5000;  // 5 seconds
  const MAX_DURATION  = 60000; // 60 seconds total
  const startTime = Date.now();
  let pollCount = 0;

  _wifiPollTimer = setInterval(async () => {
    const elapsed = Date.now() - startTime;
    const pct = Math.min(100, (elapsed / MAX_DURATION) * 100);
    bar.style.width = pct + '%';
    pollCount++;

    try {
      const res = await API.getWifiStatus(App.currentMac);
      if (res.success && res.wifi_status) {
        const st = res.wifi_status;
        if (st.status === 'APPLIED') {
          // Success!
          _stopWifiPoll();
          bar.style.width = '100%';
          bar.classList.add('success');
          text.textContent = '✅ ¡Conectado exitosamente!';
          result.textContent = `✅ Dispositivo conectado a "${st.ssid}"`;
          result.className = 'wifi-progress-result success';
          result.classList.remove('hidden');
          // Refresh device detail to show new SSID
          await refreshDeviceDetail();
          loadWifiScan();
          return;
        } else if (st.status === 'FAILED') {
          // Failed
          _stopWifiPoll();
          bar.style.width = '100%';
          bar.classList.add('failed');
          text.textContent = '❌ Fallo la conexión';
          result.textContent = `❌ ${st.fail_reason || 'No se pudo conectar a "' + st.ssid + '"'}`;
          result.className = 'wifi-progress-result failed';
          result.classList.remove('hidden');
          return;
        }
        // Still PENDING — keep polling
      }
    } catch { /* ignore poll errors, keep trying */ }

    // Timeout
    if (elapsed >= MAX_DURATION) {
      _stopWifiPoll();
      bar.style.width = '100%';
      bar.classList.add('failed');
      text.textContent = '⏰ Tiempo agotado';
      result.textContent = '⚠️ Sin respuesta del dispositivo. Puede estar intentando conectar. El resultado aparecerá al refrescar.';
      result.className = 'wifi-progress-result failed';
      result.classList.remove('hidden');
    }
  }, POLL_INTERVAL);
}

function _stopWifiPoll() {
  if (_wifiPollTimer) {
    clearInterval(_wifiPollTimer);
    _wifiPollTimer = null;
  }
}

// ── Logs ───────────────────────────────────────────────────────
$('btn-load-logs').addEventListener('click', async () => {
  $('btn-load-logs').textContent = 'Cargando…';
  try {
    const res = await API.getLogs(App.currentMac, 50);
    $('logs-list').innerHTML = '';
    if (!res.success || !res.logs?.length) {
      $('logs-list').innerHTML = '<div class="info-msg">Sin eventos registrados.</div>';
    } else {
      res.logs.forEach(log => {
        const item = document.createElement('div');
        item.className = `log-item ${log.event_type}`;
        item.innerHTML = `
          <span class="log-ts">${formatDate(log.event_ts)}</span>
          <span class="log-type">${log.event_type}</span>
          <span class="log-value">${log.event_value || ''}</span>
        `;
        $('logs-list').appendChild(item);
      });
    }
    $('logs-list').classList.remove('hidden');
    $('btn-load-logs').classList.add('hidden');
  } catch (err) {
    toast('Error al cargar logs', 'error');
    $('btn-load-logs').textContent = 'Reintentar';
  }
});

// ── Init: Si hay token guardado, ir directo a devices ──────────
(function init() {
  if (sessionStorage.getItem('token')) {
    showView('view-devices');
    loadDevices();
  } else {
    showView('view-login');
  }
})();
