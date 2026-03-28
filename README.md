# MineCraft Lamp v10 — Sistema IoT Lámpara WiFi

Control remoto de lámparas NeoPixel WS2812B vía ESP32, con sensor PIR, RTC DS3231, backend en Google Sheets + Apps Script, y Web App móvil.

---

## Estructura del proyecto

```
MineCraft Lamp v10/
├── der_files/               ← Documentos de especificación (DER)
├── backend/                 ← Google Apps Script (copiar en el editor)
│   ├── Config.gs            ← Constantes y columnas
│   ├── Utils.gs             ← Helpers compartidos
│   ├── Setup.gs             ← Inicialización del Spreadsheet
│   ├── Code.gs              ← Router doGet / doPost
│   ├── Auth.gs              ← Login y tokens
│   ├── Devices.gs           ← Registro, presencia, estado
│   ├── WiFiService.gs       ← Escaneo y config WiFi
│   ├── ControlService.gs    ← Control, movimiento, horario
│   └── LogService.gs        ← Logging y auditoría
├── webapp/                  ← Web App móvil (abrir index.html)
│   ├── index.html
│   ├── style.css
│   ├── config.js            ← ⚠️ Configurar BACKEND_URL aquí
│   ├── api.js
│   └── app.js
└── firmware/
    └── minecraft_lamp/      ← Sketch Arduino
        ├── minecraft_lamp.ino
        ├── Config.h          ← ⚠️ Configurar BOOTSTRAP_SSID y BACKEND_URL aquí
        ├── Storage.h
        ├── LEDControl.h
        ├── PIRSensor.h
        ├── RTCManager.h
        ├── WatchDog.h
        ├── WiFiManager.h
        └── BackendClient.h
```

---

## Pasos de configuración

### 1. Backend — Google Apps Script

1. Crear un nuevo **Google Spreadsheet** en Drive.
2. Ir a **Extensiones → Apps Script**.
3. Crear un archivo `.gs` por cada archivo en `backend/` y pegar el contenido.
4. Ejecutar `initializeSpreadsheet()` desde el editor (crea las hojas con headers).
5. Ejecutar `createInitialUser()` (edita LOGIN y PASSWORD en Setup.gs antes).
6. Ir a **Deploy → New deployment → Web App**:
   - Execute as: **Me**
   - Who has access: **Anyone** (o Anyone with Google Account)
7. Copiar la URL del deployment.
8. Ejecutar `showDeploymentUrl()` para confirmar la URL.

### 2. Web App

1. Abrir `webapp/config.js` y reemplazar `XXXXXXXXXX` con el ID del deployment.
2. Abrir `webapp/index.html` en el navegador (o servirlo con cualquier servidor estático).

### 3. Firmware ESP32

#### Librerías requeridas (Arduino IDE Library Manager):
- `Adafruit NeoPixel`
- `RTClib` (Adafruit)
- `ArduinoJson` (Benoit Blanchon)

#### Board: ESP32 Dev Module (Espressif Systems)

1. Editar `firmware/minecraft_lamp/Config.h`:
   - `BOOTSTRAP_SSID` / `BOOTSTRAP_PASSWORD` → tu red bootstrap
   - `BACKEND_URL` → URL del deployment de Apps Script
   - Ajustar pines GPIO si es necesario
2. Compilar y cargar al ESP32.
3. Abrir el Monitor Serie a 115200 baud para ver logs de depuración.

---

## Decisiones de diseño confirmadas

| # | Decisión | Valor |
|---|---|---|
| D1 | Polling ESP32 al backend | 30 segundos |
| D2 | OFFLINE tras sin heartbeat | 5 minutos |
| D3 | Debounce PIR | 30 segundos |
| D4 | Sincronización RTC | NTP en bootstrap |
| D5 | Loop firmware | Cooperativo (`millis()`) |
| D6 | Comandos | Directo desde DEVICES |
| D7 | Logs en web app | Últimos 50 eventos |
| D8 | Recovery bootstrap | Cada 5 ciclos fallidos |

---

## Hardware

| Componente | GPIO |
|---|---|
| NeoPixel WS2812B | 5 |
| PIR HC-SR501 | 14 |
| RTC DS3231 SDA | 21 |
| RTC DS3231 SCL | 22 |

> Ajustar pines en `Config.h` según tu circuito real.

---

## Estrategia de reconexión WiFi (3 fases)

1. **Fase A** — 3 reintentos rápidos a la red operativa (cada 10s)
2. **Fase B** — Modo offline local, reintentos cada 60s
3. **Fase C** — Cada 5 ciclos fallidos, conectar a bootstrap para publicar diagnóstico y recibir nuevas credenciales
