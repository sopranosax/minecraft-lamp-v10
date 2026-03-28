# DER-03 — Especificación de Requerimientos del Backend sobre Google Sheets

## 1. Información del documento
- **Nombre**: DER Backend Google Sheets
- **Código**: DER-03
- **Versión**: 1.1

## 2. Propósito
Definir la persistencia, lógica de integración y contratos funcionales mínimos del backend basado en Google Sheets para soportar web app y ESP32.

## 3. Consideración de arquitectura
Google Sheets por sí solo no provee una API de negocio lista para este caso. En esta versión el backend usará **Google Apps Script** como capa de servicio HTTP/HTTPS sobre las hojas, validando entradas, aplicando reglas y exponiendo operaciones para la web app y el ESP32.

## 4. Objetivos del backend
- Registrar dispositivos por MAC.
- Mantener su estado de presencia.
- Recibir y almacenar escaneos WiFi.
- Recibir credenciales WiFi configuradas por usuario.
- Entregar configuración vigente al dispositivo.
- Persistir comandos de control manual.
- Persistir configuración de movimiento y horario.
- Registrar eventos del dispositivo.
- Proveer autenticación a la web app.

## 5. Entidades y tablas propuestas
## 5.1 `USERS`
Propósito: autenticación de la web app.

Campos mínimos propuestos:
- `USER_ID` (string/uuid)
- `LOGIN`
- `PASSWORD_HASH`
- `STATUS` (`ACTIVE`/`INACTIVE`)
- `CREATED_AT`
- `UPDATED_AT`
- `LAST_LOGIN_AT`

Reglas:
- `LOGIN` debe ser único.
- Nunca almacenar password plano para credenciales de usuarios de la web app.

## 5.2 `DEVICES`
Propósito: catálogo maestro de dispositivos.

Campos mínimos propuestos:
- `DEVICE_ID` (uuid o correlativo)
- `MAC_ADDRESS` (único)
- `DEVICE_ALIAS`
- `STATUS` (`ONLINE`, `OFFLINE`, `CONFIG_PENDING`, `WIFI_CONNECTING`, `WIFI_FAILED`)
- `BOOTSTRAP_SSID` (opcional informativo)
- `CURRENT_WIFI_SSID`
- `LAST_SEEN_AT`
- `LAST_IP` (opcional)
- `MOTION_MODE` (`BLINK`, `NO_BLINK`)
- `MOTION_BLINK_SECONDS`
- `SCHEDULE_ENABLED` (`TRUE`/`FALSE`)
- `SCHEDULE_START_TIME`
- `SCHEDULE_END_TIME`
- `SCHEDULE_COLOR_HEX`
- `MANUAL_POWER_STATE` (`ON`, `OFF`)
- `MANUAL_COLOR_HEX`
- `RECONNECT_INTERVAL_SECONDS`
- `LAST_WIFI_ERROR`
- `RECOVERY_MODE` (`TRUE`/`FALSE`)
- `RECOVERY_BOOTSTRAP_EVERY_N_CYCLES`
- `LAST_RECOVERY_ATTEMPT_AT`
- `WATCHDOG_ENABLED`
- `CREATED_AT`
- `UPDATED_AT`

## 5.3 `DEVICE_WIFI_SCAN`
Propósito: guardar redes detectadas por dispositivo.

Campos mínimos propuestos:
- `SCAN_ID`
- `DEVICE_ID` o `MAC_ADDRESS`
- `SSID`
- `RSSI`
- `SECURITY_TYPE` (opcional)
- `SCANNED_AT`
- `IS_CURRENT` (`TRUE`/`FALSE`)

Reglas:
- Cada nuevo escaneo debería desmarcar el set anterior como no vigente.

## 5.4 `DEVICE_WIFI_CREDENTIALS`
Propósito: almacenar la configuración WiFi pendiente o vigente.

Campos mínimos propuestos:
- `WIFI_CONFIG_ID`
- `DEVICE_ID` o `MAC_ADDRESS`
- `SSID`
- `PASSWORD_ENCRYPTED_OR_MASKED_REFERENCE`
- `STATUS` (`PENDING`, `APPLIED`, `FAILED`)
- `REQUESTED_BY_USER_ID`
- `REQUESTED_AT`
- `APPLIED_AT`
- `FAILED_AT`
- `FAIL_REASON`

> En esta versión se permite almacenar la credencial WiFi con enfoque pragmático para acelerar la implementación. Aun así, se recomienda restringir el acceso a la hoja y exponer la lectura sólo mediante Apps Script.

## 5.5 `DEVICE_COMMANDS`
Propósito: cola liviana de comandos pendientes para el dispositivo.

Campos mínimos propuestos:
- `COMMAND_ID`
- `DEVICE_ID` o `MAC_ADDRESS`
- `COMMAND_TYPE` (`SET_POWER`, `SET_COLOR`, `SET_WIFI`, `SET_MOTION_CONFIG`, `SET_SCHEDULE`)
- `PAYLOAD_JSON`
- `STATUS` (`PENDING`, `DELIVERED`, `ACK`, `FAILED`, `CANCELLED`)
- `CREATED_BY_USER_ID`
- `CREATED_AT`
- `DELIVERED_AT`
- `ACK_AT`
- `FAILED_AT`
- `FAIL_REASON`

## 5.6 `DEVICE_LOG`
Propósito: auditoría y eventos operacionales.

Campos mínimos propuestos:
- `LOG_ID`
- `DEVICE_ID` o `MAC_ADDRESS`
- `EVENT_TYPE`
- `EVENT_TS`
- `EVENT_VALUE`
- `DETAILS_JSON`
- `SOURCE` (`DEVICE`, `BACKEND`, `WEBAPP`)

Eventos sugeridos:
- `DEVICE_REGISTERED`
- `DEVICE_ONLINE`
- `DEVICE_OFFLINE`
- `WIFI_SCAN_PUBLISHED`
- `WIFI_CONFIG_REQUESTED`
- `WIFI_CONNECTING`
- `WIFI_CONNECTED`
- `WIFI_FAILED`
- `MOTION_DETECTED`
- `POWER_ON`
- `POWER_OFF`
- `COLOR_CHANGED`
- `SCHEDULE_UPDATED`
- `WATCHDOG_RESTART`
- `RECOVERY_MODE_ENTERED`
- `RECOVERY_RETRY_OPERATIONAL`
- `RECOVERY_BOOTSTRAP_CONNECTED`
- `RECOVERY_BOOTSTRAP_FAILED`

## 5.7 `DEVICE_SCHEDULES` (opcional)
Si se desea separar programación del maestro `DEVICES`.

Campos mínimos:
- `SCHEDULE_ID`
- `DEVICE_ID`
- `ENABLED`
- `START_TIME`
- `END_TIME`
- `COLOR_HEX`
- `UPDATED_BY_USER_ID`
- `UPDATED_AT`

> Para una v1 simple, los campos pueden residir directamente en `DEVICES`.

## 6. Requerimientos funcionales del backend
### 6.1 Autenticación
- **BE-FR-001**: El backend debe validar login y password de la web app.
- **BE-FR-002**: El backend debe devolver un identificador de sesión o token de acceso.
- **BE-FR-003**: El backend debe rechazar usuarios inactivos.

### 6.2 Registro y presencia de dispositivos
- **BE-FR-004**: El backend debe buscar dispositivos por `MAC_ADDRESS`.
- **BE-FR-005**: Si no existe la MAC, debe crear un registro nuevo.
- **BE-FR-006**: Si existe, debe actualizar `LAST_SEEN_AT` y `STATUS`.
- **BE-FR-007**: El backend debe permitir que la web app liste dispositivos y sus estados.
- **BE-FR-008**: El backend debe calcular o exponer condición online/offline según último heartbeat.

### 6.3 Escaneos WiFi
- **BE-FR-009**: El backend debe recibir múltiples SSID con RSSI desde el dispositivo.
- **BE-FR-010**: El backend debe asociar cada escaneo al dispositivo correcto.
- **BE-FR-011**: El backend debe poder devolver a la web app el escaneo vigente de un dispositivo.
- **BE-FR-012**: El backend debe registrar el evento `WIFI_SCAN_PUBLISHED`.

### 6.4 Configuración WiFi
- **BE-FR-013**: El backend debe recibir desde la web app el SSID y password elegidos para un dispositivo.
- **BE-FR-014**: El backend debe persistir esta configuración con estado `PENDING`.
- **BE-FR-015**: El backend debe exponer al dispositivo la configuración WiFi pendiente que le corresponda.
- **BE-FR-016**: El backend debe actualizar el estado del intento de conexión a:
  - `WIFI_CONNECTING`,
  - `WIFI_CONNECTED`,
  - `WIFI_FAILED`.
- **BE-FR-017**: El backend debe guardar la causa de fallo cuando el dispositivo no logre conectarse.
- **BE-FR-018**: El backend debe permitir múltiples intentos históricos sin perder trazabilidad.
- **BE-FR-018A**: El backend debe exponer al dispositivo la estrategia de reconexión vigente, incluyendo `RECONNECT_INTERVAL_SECONDS` y `RECOVERY_BOOTSTRAP_EVERY_N_CYCLES`.
- **BE-FR-018B**: El backend debe aceptar eventos de recuperación como `RECOVERY_MODE_ENTERED` y `RECOVERY_BOOTSTRAP_CONNECTED`.

### 6.5 Comandos de control manual
- **BE-FR-019**: El backend debe recibir solicitudes de encendido/apagado.
- **BE-FR-020**: El backend debe recibir solicitudes de cambio de color.
- **BE-FR-021**: El backend debe persistir estos cambios como configuración vigente y/o comando pendiente.
- **BE-FR-022**: El backend debe exponer al dispositivo los comandos pendientes.
- **BE-FR-023**: El backend debe marcar un comando como `ACK` cuando el dispositivo confirme aplicación.

### 6.6 Configuración de movimiento
- **BE-FR-024**: El backend debe persistir el modo de movimiento (`BLINK` o `NO_BLINK`).
- **BE-FR-025**: El backend debe persistir los segundos de blink cuando aplique.
- **BE-FR-026**: El backend debe exponer esta configuración al dispositivo.
- **BE-FR-027**: El backend debe registrar cambios de esta configuración en auditoría.

### 6.7 Programación horaria
- **BE-FR-028**: El backend debe persistir horario de inicio, término y color.
- **BE-FR-029**: El backend debe exponer esta programación al dispositivo.
- **BE-FR-030**: El backend debe registrar actualizaciones de horario en auditoría.

### 6.8 Logging y auditoría
- **BE-FR-031**: El backend debe registrar eventos enviados por el dispositivo.
- **BE-FR-032**: El backend debe registrar eventos clave generados por la web app y la propia capa de servicio.
- **BE-FR-033**: El backend debe permitir consultar los últimos eventos de un dispositivo.
- **BE-FR-034**: El backend debe aceptar `MOTION_DETECTED` solo cuando el dispositivo reporte conectividad activa.
- **BE-FR-035**: El backend debería registrar reinicios por WatchDog cuando el dispositivo los informe al arrancar.

## 7. Contratos lógicos propuestos
> No son una obligación de ruta ni tecnología, pero sí una especificación funcional recomendada.

### 7.1 `POST /auth/login`
Entrada:
```json
{
  "login": "usuario",
  "password": "secreto"
}
```

Salida exitosa:
```json
{
  "success": true,
  "token": "session-or-jwt",
  "user": {
    "user_id": "U001",
    "login": "usuario"
  }
}
```

### 7.2 `POST /devices/bootstrap`
Uso: arranque del dispositivo desde red bootstrap.

Entrada:
```json
{
  "mac_address": "AA:BB:CC:DD:EE:FF",
  "firmware_version": "1.0.0",
  "ip": "192.168.1.20"
}
```

Salida:
```json
{
  "success": true,
  "device_id": "D001",
  "status": "ONLINE",
  "config": {
    "motion_mode": "BLINK",
    "motion_blink_seconds": 5,
    "schedule_enabled": false,
    "reconnect_interval_seconds": 60
  }
}
```

### 7.3 `POST /devices/{mac}/wifi-scan`
Entrada:
```json
{
  "scan_ts": "2026-03-27T12:00:00Z",
  "networks": [
    { "ssid": "RedA", "rssi": -55 },
    { "ssid": "RedB", "rssi": -71 }
  ]
}
```

### 7.4 `POST /devices/{mac}/wifi-config`
Uso: llamado por web app.

Entrada:
```json
{
  "ssid": "MiRed",
  "password": "mi-password"
}
```

Salida:
```json
{
  "success": true,
  "status": "PENDING"
}
```

### 7.5 `GET /devices/{mac}/pending-config`
Uso: polling del dispositivo.

Salida ejemplo:
```json
{
  "wifi_config": {
    "status": "PENDING",
    "ssid": "MiRed",
    "password": "mi-password"
  },
  "device_config": {
    "manual_power_state": "ON",
    "manual_color_hex": "#00FF00",
    "motion_mode": "BLINK",
    "motion_blink_seconds": 7,
    "schedule_enabled": true,
    "schedule_start_time": "19:00",
    "schedule_end_time": "23:30",
    "schedule_color_hex": "#FFA500",
    "reconnect_interval_seconds": 60
  }
}
```

### 7.6 `POST /devices/{mac}/status`
Entrada:
```json
{
  "status": "WIFI_CONNECTED",
  "current_wifi_ssid": "MiRed",
  "ip": "192.168.0.15",
  "ts": "2026-03-27T12:05:00Z"
}
```

### 7.7 `POST /devices/{mac}/events`
Entrada:
```json
{
  "event_type": "MOTION_DETECTED",
  "event_ts": "2026-03-27T12:06:00Z",
  "details": {
    "pir": true
  }
}
```

### 7.8 `GET /devices`
Uso: listado para web app.

### 7.9 `GET /devices/{mac}`
Uso: detalle para web app.

### 7.10 `POST /devices/{mac}/control`
Entrada:
```json
{
  "manual_power_state": "OFF",
  "manual_color_hex": "#FF0000"
}
```

### 7.11 `POST /devices/{mac}/motion-config`
Entrada:
```json
{
  "motion_mode": "BLINK",
  "motion_blink_seconds": 5
}
```

### 7.12 `POST /devices/{mac}/schedule`
Entrada:
```json
{
  "schedule_enabled": true,
  "schedule_start_time": "20:00",
  "schedule_end_time": "06:00",
  "schedule_color_hex": "#0000FF"
}
```

## 8. Validaciones de backend
- **BE-BR-001**: `MAC_ADDRESS` debe ser único.
- **BE-BR-002**: No aceptar `BLINK` con segundos nulos o menores o iguales a cero.
- **BE-BR-003**: No aceptar colores en formato no soportado.
- **BE-BR-004**: No asociar un SSID a un dispositivo distinto del que reportó el escaneo, salvo que se permita modo avanzado manual.
- **BE-BR-005**: Toda mutación relevante debe actualizar `UPDATED_AT`.
- **BE-BR-006**: No exponer password WiFi en respuestas a la web app.
- **BE-BR-007**: El dispositivo solo debe recibir configuraciones que le correspondan por MAC.
- **BE-BR-008**: Un comando pendiente anterior puede ser reemplazado por uno más reciente del mismo tipo si aún no fue aplicado, según política a definir.

## 9. Requerimientos no funcionales
### 9.1 Integridad de datos
- **BE-NFR-001**: La capa de servicio debe evitar registros duplicados por reintentos.
- **BE-NFR-002**: La capa de servicio debe mantener consistencia entre `DEVICES`, comandos y logs.
- **BE-NFR-003**: Las fechas y horas deben almacenarse en un formato consistente.

### 9.2 Seguridad
- **BE-NFR-004**: La autenticación debe verificar passwords hasheados.
- **BE-NFR-005**: Debe existir protección básica frente a llamadas no autenticadas desde la web app.
- **BE-NFR-006**: El acceso del ESP32 debe validarse al menos por MAC y una clave de dispositivo o secreto compartido si se decide reforzar seguridad.

### 9.3 Operabilidad
- **BE-NFR-007**: El backend debe ser fácilmente inspeccionable por personas no técnicas a nivel de Google Sheets.
- **BE-NFR-008**: Los nombres de hojas y columnas deben ser estables y documentados.
- **BE-NFR-009**: Debe ser posible depurar el flujo leyendo tablas y logs sin acceder al firmware.

## 10. Criterios de aceptación
### CA-BE-01
**Dado** un dispositivo nuevo  
**cuando** llama al endpoint de bootstrap  
**entonces** se crea su registro en `DEVICES`.

### CA-BE-02
**Dado** un dispositivo existente  
**cuando** reporta presencia  
**entonces** el backend actualiza `LAST_SEEN_AT` y estado.

### CA-BE-03
**Dado** un escaneo WiFi  
**cuando** el dispositivo lo envía  
**entonces** queda disponible para consulta desde la web app.

### CA-BE-04
**Dado** una configuración WiFi enviada desde la web app  
**cuando** el dispositivo la consulta  
**entonces** la recibe como pendiente.

### CA-BE-05
**Dado** un cambio de movimiento u horario  
**cuando** la web app lo guarda  
**entonces** el backend persiste la nueva configuración y la expone al dispositivo.

### CA-BE-06
**Dado** un evento de movimiento online  
**cuando** el dispositivo lo envía  
**entonces** se registra en `DEVICE_LOG` con timestamp y dispositivo.

## 11. Decisiones pendientes
1. **Resuelto**: se usará Google Apps Script como capa de servicio del backend.
2. Definir política exacta de online/offline por heartbeat.
3. **Resuelto para v1**: las credenciales WiFi no tendrán protección adicional más allá del control de acceso al Spreadsheet y a Apps Script.
4. Definir si la cola de comandos será explícita o si el dispositivo leerá directamente configuración vigente desde `DEVICES`.
5. Definir si los logs se limitarán a últimos N eventos visibles desde la web app.
6. Definir valor por defecto de `RECOVERY_BOOTSTRAP_EVERY_N_CYCLES` y si será editable desde web app.
