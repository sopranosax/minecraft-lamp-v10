# DER-01 — Especificación General del Sistema

## 1. Información del documento
- **Nombre**: Documento de Especificación de Requerimientos — Sistema IoT de Lámpara WiFi
- **Código**: DER-01
- **Versión**: 1.1
- **Formato objetivo**: Markdown para uso en Antigravity
- **Estado**: Borrador funcional inicial actualizado con decisiones de implementación

## 2. Propósito
Definir los requerimientos funcionales, no funcionales, reglas de negocio, entidades principales y criterios de aceptación del sistema compuesto por:
- Web app móvil
- Backend basado en Google Sheets
- Dispositivo ESP32 con Arduino
- Sensor PIR HC-SR501
- Lámpara RGB WS2812B NeoPixel
- Módulo RTC DS3231

## 3. Alcance
El sistema permitirá:
1. Registrar y administrar una lámpara IoT identificada por su **MAC address**.
2. Configurar remotamente la red WiFi operativa del dispositivo.
3. Controlar manualmente el encendido, apagado y color de la lámpara.
4. Definir comportamiento ante detección de movimiento.
5. Programar encendido y apagado por horario usando RTC.
6. Registrar eventos de movimiento y cambios de estado cuando exista conectividad.
7. Mantener operación local aun cuando falle la red WiFi.
8. Reiniciar el dispositivo de forma segura mediante WatchDog si se bloquea.

## 4. Objetivos del producto
- Facilitar la puesta en marcha remota de un dispositivo IoT inicialmente conectado a una red de bootstrap.
- Permitir operación híbrida: **control remoto cuando hay red** y **continuidad operativa local cuando no hay red**.
- Centralizar configuración, estado y trazabilidad en un backend sencillo apoyado en Google Sheets.
- Proveer una base clara para que Antigravity pueda descomponer el trabajo por capas.

## 5. Definiciones
- **Bootstrap WiFi**: red WiFi por defecto, cuyas credenciales vienen compiladas en el firmware y permiten el primer contacto con el backend.
- **WiFi operativa**: red WiFi seleccionada por el usuario desde la web app para el funcionamiento normal del dispositivo.
- **MAC address**: identificador único físico del ESP32, usado como clave de registro del dispositivo.
- **Blink**: parpadeo temporal del NeoPixel por una cantidad de segundos.
- **RTC**: reloj de tiempo real DS3231, usado para mantener la programación horaria incluso sin internet.
- **Backend**: capa lógica y de persistencia apoyada en Google Sheets con Google Apps Script como capa de servicio HTTP/HTTPS para exponer operaciones al ESP32 y a la web app.
- **Estado online**: condición informada por el dispositivo al backend cuando logra comunicarse exitosamente.

## 6. Actores
### 6.1 Usuario autenticado
Persona que usa la web app móvil para:
- iniciar sesión,
- ver dispositivos online,
- configurar WiFi,
- controlar la lámpara,
- definir comportamiento por movimiento,
- programar horarios.

### 6.2 Dispositivo ESP32
Nodo IoT que:
- se registra por MAC,
- reporta estado,
- escanea redes,
- ejecuta comandos,
- detecta movimiento,
- aplica reglas locales,
- registra eventos cuando hay conectividad.

### 6.3 Backend
Componente lógico y de persistencia que:
- almacena dispositivos, configuraciones, redes detectadas y logs,
- recibe estados y eventos del dispositivo,
- expone información y comandos a la web app,
- sincroniza la configuración vigente del dispositivo.

## 7. Supuestos de arquitectura
> Estos puntos son **derivados**. No contradicen los requerimientos, pero conviene validarlos antes de construir.

1. El backend basado en Google Sheets usará **Google Apps Script** como capa de servicio para lectura/escritura transaccional desde ESP32 y web app.  
2. El dispositivo manejará al menos dos perfiles lógicos de red:
   - **red bootstrap** (incluida en código),
   - **última red operativa configurada** por el usuario.
3. Si falla la conexión a la red operativa, el dispositivo aplicará una estrategia de reconexión **mixta y escalonada**:
   - primero reintenta la **última red operativa válida** mediante reintentos rápidos locales,
   - si no la recupera, entra en **modo recuperación** y sigue operando offline,
   - en modo recuperación intenta periódicamente la red operativa y, cada cierto número de fallos, intenta conectarse temporalmente a la **red bootstrap** para reportar estado, publicar escaneos y recibir nuevas credenciales.
4. Cuando no exista conectividad, los eventos de movimiento **no se almacenarán para reenvío posterior**, salvo que en una iteración futura se agregue buffer local.
5. El RTC se considera fuente horaria local para la programación del encendido/apagado cuando el equipo esté offline.
6. Las credenciales WiFi podrán almacenarse en backend y enviarse al dispositivo sin capa adicional de protección en esta versión, por decisión explícita de alcance.

## 8. Fuera de alcance de esta versión
- Control simultáneo de múltiples LEDs o efectos complejos.
- Historial offline con sincronización diferida.
- OTA firmware update.
- Multi-tenant, organizaciones o permisos avanzados por rol.
- Geolocalización.
- Integraciones de voz.
- Administración masiva de dispositivos.

## 9. Descripción del flujo de alto nivel
1. El ESP32 arranca y se conecta a la red bootstrap.
2. Consulta al backend si su MAC existe en `DEVICES`.
3. Si no existe, se registra automáticamente.
4. Reporta estado online y publica redes WiFi detectadas con intensidad.
5. El usuario inicia sesión en la web app.
6. El usuario ve el dispositivo online y entra a su configuración.
7. Selecciona una red detectada e ingresa su password.
8. El backend deja disponible la nueva configuración para el dispositivo.
9. El ESP32 toma las nuevas credenciales e intenta conectarse.
10. Si conecta, actualiza su estado online en backend usando la red operativa.
11. Si falla, reporta el fallo, vuelve a publicar el escaneo y espera nuevas credenciales válidas.
12. Ya configurado, el usuario puede encender/apagar, elegir color y definir comportamiento por movimiento.
13. El usuario también puede definir horario de encendido/apagado y color programado.
14. Si hay movimiento:
    - siempre se registra en backend cuando haya conectividad,
    - la lámpara hace blink o no según configuración.
15. Si se pierde la WiFi:
    - el dispositivo sigue ejecutando horario y lógica de movimiento,
    - deja de registrar logs de movimiento,
    - intenta reconectarse con la frecuencia indicada en `DEVICES`.
16. Si el firmware se bloquea, el WatchDog reinicia el dispositivo de forma segura.

## 10. Casos de uso principales
### UC-01 Registrar dispositivo automáticamente
**Actor principal**: ESP32  
**Precondición**: el dispositivo dispone de red bootstrap.  
**Resultado esperado**: el dispositivo queda registrado por MAC.

### UC-02 Publicar redes WiFi detectadas
**Actor principal**: ESP32  
**Resultado esperado**: el backend dispone de una lista actualizada de SSID y RSSI por dispositivo.

### UC-03 Configurar red operativa del dispositivo
**Actor principal**: Usuario autenticado  
**Resultado esperado**: el dispositivo recibe las credenciales y cambia a la nueva red si son válidas.

### UC-04 Control manual de la lámpara
**Actor principal**: Usuario autenticado  
**Resultado esperado**: el usuario puede encender, apagar y cambiar color del NeoPixel.

### UC-05 Definir reacción ante movimiento
**Actor principal**: Usuario autenticado  
**Resultado esperado**: el dispositivo queda configurado para blink por X segundos o sin blink.

### UC-06 Registrar movimiento
**Actor principal**: ESP32  
**Resultado esperado**: cada detección PIR online genera un evento en `DEVICE_LOG`.

### UC-07 Ejecutar programación horaria
**Actor principal**: ESP32  
**Resultado esperado**: la lámpara se enciende/apaga según horario y color definidos.

### UC-08 Operar sin internet
**Actor principal**: ESP32  
**Resultado esperado**: el equipo mantiene reglas locales de horario y blink aunque no logre registrar eventos.

### UC-09 Recuperación de conectividad
**Actor principal**: ESP32  
**Resultado esperado**: el dispositivo recupera red operativa o entra en modo bootstrap de recuperación sin perder la lógica local.

### UC-10 Autorecuperación por bloqueo
**Actor principal**: ESP32 / WatchDog  
**Resultado esperado**: reinicio seguro y retorno a estado operativo.

## 11. Requerimientos funcionales del sistema
Los siguientes requerimientos son transversales y de extremo a extremo.

### 11.1 Registro y presencia del dispositivo
- **SYS-FR-001**: El sistema debe identificar cada dispositivo por su **MAC address**.
- **SYS-FR-002**: Al iniciar, el dispositivo debe conectarse primero a la red bootstrap definida en firmware.
- **SYS-FR-003**: El dispositivo debe consultar si su MAC ya existe en la tabla `DEVICES`.
- **SYS-FR-004**: Si la MAC no existe, el sistema debe crear el registro del dispositivo en `DEVICES`.
- **SYS-FR-005**: Si la MAC sí existe, el sistema debe actualizar su condición de presencia a **online**.
- **SYS-FR-006**: El backend debe almacenar la fecha/hora del último contacto del dispositivo.
- **SYS-FR-007**: El backend debe distinguir el estado del dispositivo al menos entre `ONLINE`, `OFFLINE`, `CONFIG_PENDING`, `WIFI_CONNECTING`, `WIFI_FAILED`.

### 11.2 Escaneo y configuración WiFi
- **SYS-FR-008**: El dispositivo debe detectar redes WiFi visibles y su intensidad.
- **SYS-FR-009**: El sistema debe almacenar por dispositivo el resultado del escaneo WiFi.
- **SYS-FR-010**: La web app debe permitir seleccionar una red detectada para un dispositivo específico.
- **SYS-FR-011**: La web app debe permitir ingresar el password de la red seleccionada.
- **SYS-FR-012**: El backend debe asociar las credenciales ingresadas con el dispositivo objetivo.
- **SYS-FR-013**: El dispositivo debe consultar periódicamente si existen credenciales WiFi pendientes para aplicar.
- **SYS-FR-014**: El dispositivo debe intentar conectarse a la red seleccionada.
- **SYS-FR-015**: Si logra conectarse, el sistema debe reflejar el nuevo estado online del dispositivo.
- **SYS-FR-016**: Si no logra conectarse, el sistema debe registrar un fallo de conexión y permitir nuevo ciclo de selección.
- **SYS-FR-017**: Ante fallo de conexión, el dispositivo debe volver a publicar redes disponibles cuando exista conectividad para hacerlo.

### 11.3 Control manual de iluminación
- **SYS-FR-018**: La web app debe permitir encender la lámpara.
- **SYS-FR-019**: La web app debe permitir apagar la lámpara.
- **SYS-FR-020**: La web app debe permitir seleccionar el color del NeoPixel mediante un color picker.
- **SYS-FR-021**: El dispositivo debe aplicar el estado ON/OFF y el color configurado por el usuario.
- **SYS-FR-022**: El backend debe persistir el último estado manual y color deseado del dispositivo.

### 11.4 Detección de movimiento y reacción
- **SYS-FR-023**: El dispositivo debe monitorear el sensor PIR.
- **SYS-FR-024**: Si el PIR detecta movimiento y el equipo está online, el sistema debe registrar el evento en `DEVICE_LOG`.
- **SYS-FR-025**: Cada evento de movimiento debe incluir timestamp y referencia al dispositivo.
- **SYS-FR-026**: El usuario debe poder definir si al detectar movimiento la lámpara:
  - haga blink por X segundos, o
  - no haga blink.
- **SYS-FR-027**: El parámetro X segundos debe ser configurable por el usuario.
- **SYS-FR-028**: Si la lámpara estaba encendida y corresponde blink, al finalizar el blink debe volver al estado encendido estable.
- **SYS-FR-029**: Si la lámpara estaba apagada y corresponde blink, al finalizar el blink debe volver al estado apagado.
- **SYS-FR-030**: Si la política configurada es `NO_BLINK`, el movimiento no debe alterar el estado visual de la lámpara.

### 11.5 Programación horaria
- **SYS-FR-031**: El usuario debe poder activar una programación horaria para el dispositivo.
- **SYS-FR-032**: La programación horaria debe permitir:
  - hora de inicio,
  - hora de término,
  - color programado.
- **SYS-FR-033**: El dispositivo debe usar el RTC como referencia para ejecutar la programación.
- **SYS-FR-034**: En la hora de inicio, la lámpara debe encenderse con el color programado.
- **SYS-FR-035**: En la hora de término, la lámpara debe apagarse.
- **SYS-FR-036**: La programación horaria debe seguir operando aun sin conectividad WiFi.
- **SYS-FR-037**: El backend debe persistir la configuración horaria vigente.

### 11.6 Operación offline y reconexión
- **SYS-FR-038**: Si se pierde la conexión WiFi, el dispositivo debe continuar ejecutando:
  - horario programado,
  - lógica de blink o no blink ante movimiento.
- **SYS-FR-039**: Si está offline, el dispositivo no debe registrar logs de movimiento en backend.
- **SYS-FR-040**: El intervalo de reintento de conexión WiFi debe ser configurable desde `DEVICES`.
- **SYS-FR-041**: El dispositivo debe intentar reconectarse a la última red operativa configurada usando el intervalo definido.
- **SYS-FR-042**: Al recuperar conectividad, el sistema debe actualizar el estado online del dispositivo.

### 11.7 Resiliencia
- **SYS-FR-043**: El dispositivo debe contar con WatchDog habilitado.
- **SYS-FR-044**: Si el firmware entra en bloqueo o deja de alimentar el WatchDog, el dispositivo debe reiniciarse automáticamente.
- **SYS-FR-045**: Después del reinicio, el sistema debe volver al flujo de arranque y registro.
- **SYS-FR-046**: El reinicio debe considerarse seguro, minimizando estados inconsistentes del NeoPixel o de la configuración en memoria.

## 12. Requerimientos no funcionales
### 12.1 Seguridad
- **SYS-NFR-001**: La web app debe exigir autenticación por login y password.
- **SYS-NFR-002**: Los passwords de usuarios no deben almacenarse en texto plano.
- **SYS-NFR-003**: Las comunicaciones entre web app, backend y dispositivo deben usar HTTPS cuando la plataforma lo permita.
- **SYS-NFR-004**: Las credenciales WiFi almacenadas en backend deben protegerse y mostrarse enmascaradas en interfaces de usuario.
- **SYS-NFR-005**: La web app no debe exponer el password WiFi ya almacenado.

### 12.2 Disponibilidad y tolerancia a fallas
- **SYS-NFR-006**: El dispositivo debe continuar operando localmente aunque el backend no esté disponible temporalmente.
- **SYS-NFR-007**: El backend debe tolerar reintentos idempotentes del dispositivo para registro, heartbeat y logging.
- **SYS-NFR-008**: El sistema debe evitar duplicados evidentes en registros de presencia y eventos por reintentos de red.

### 12.3 Desempeño
- **SYS-NFR-009**: El cambio manual ON/OFF solicitado por la web app debería reflejarse en el dispositivo en un tiempo objetivo de pocos segundos cuando exista conectividad normal.
- **SYS-NFR-010**: El escaneo WiFi debe completarse en un tiempo razonable para uso interactivo.
- **SYS-NFR-011**: La consulta periódica del dispositivo al backend debe equilibrar latencia y consumo de red/energía.

### 12.4 Mantenibilidad
- **SYS-NFR-012**: Los requerimientos deben implementarse por módulos claramente separados: autenticación, catálogo de dispositivos, configuración WiFi, control de luz, reglas PIR, programación horaria, logging y watchdog.
- **SYS-NFR-013**: Los identificadores funcionales de este DER deben poder trazarse a historias técnicas o tareas de implementación.
- **SYS-NFR-014**: Los nombres de estados, columnas y eventos deben mantenerse consistentes entre firmware, backend y web app.

### 12.5 Auditoría y trazabilidad
- **SYS-NFR-015**: Cada cambio relevante de configuración debería quedar trazable con fecha/hora y usuario.
- **SYS-NFR-016**: Los eventos de conexión fallida, conexión exitosa y detección de movimiento deben tener tipificación consistente.

## 13. Reglas de negocio
- **SYS-BR-001**: La MAC address es el identificador único del dispositivo.
- **SYS-BR-002**: Un dispositivo puede tener solo una configuración WiFi operativa vigente a la vez.
- **SYS-BR-003**: Un dispositivo puede tener múltiples resultados de escaneo WiFi, pero solo el más reciente debe marcarse como vigente para selección.
- **SYS-BR-004**: El modo de movimiento solo puede ser `BLINK` o `NO_BLINK`.
- **SYS-BR-005**: Si el modo es `BLINK`, el tiempo de blink debe ser mayor que 0.
- **SYS-BR-006**: Si el modo es `NO_BLINK`, el tiempo de blink puede almacenarse pero no debe aplicarse.
- **SYS-BR-007**: La programación horaria se ejecuta con la hora del RTC local del dispositivo, no con la hora del navegador del usuario.
- **SYS-BR-008**: Si la programación horaria está activa y también existe control manual, debe definirse precedencia.  
  **Decisión propuesta para v1**: el control manual modifica el estado actual, pero la siguiente marca horaria programada vuelve a gobernar.
- **SYS-BR-009**: En estado offline, el equipo no debe intentar registrar logs de movimiento hasta recuperar conectividad.
- **SYS-BR-010**: El watchdog no reemplaza la lógica de reconexión; ambas capacidades son complementarias.

## 14. Entidades principales
- `USERS`
- `DEVICES`
- `DEVICE_WIFI_SCAN`
- `DEVICE_WIFI_CREDENTIALS` o equivalente lógico
- `DEVICE_COMMANDS`
- `DEVICE_SCHEDULES`
- `DEVICE_LOG`

> El detalle propuesto de estas entidades se especifica en DER-03.

## 15. Matriz resumida de trazabilidad con requerimientos originales
| Requerimiento original | Cobertura principal |
|---|---|
| Registro por MAC y presencia online | SYS-FR-001 a SYS-FR-007 |
| Publicación de redes detectadas | SYS-FR-008 a SYS-FR-009 |
| Selección de SSID y envío de password | SYS-FR-010 a SYS-FR-017 |
| Encendido, apagado y color | SYS-FR-018 a SYS-FR-022 |
| PIR, log y blink | SYS-FR-023 a SYS-FR-030 |
| Programación con RTC | SYS-FR-031 a SYS-FR-037 |
| Operación offline y reconexión | SYS-FR-038 a SYS-FR-042 |
| WatchDog | SYS-FR-043 a SYS-FR-046 |

## 16. Criterios de aceptación de extremo a extremo
### CA-E2E-01 — Alta automática
**Dado** un ESP32 con firmware válido y red bootstrap disponible  
**cuando** inicia por primera vez  
**entonces** se registra en `DEVICES` y queda visible como online.

### CA-E2E-02 — Escaneo WiFi visible en web app
**Dado** un dispositivo online  
**cuando** completa un escaneo de redes  
**entonces** la web app muestra SSID e intensidad asociados al dispositivo.

### CA-E2E-03 — Cambio a red operativa válido
**Dado** que el usuario selecciona un SSID y password correctos  
**cuando** el dispositivo aplica la nueva configuración  
**entonces** logra conectarse, actualiza su estado online y queda usando la red operativa.

### CA-E2E-04 — Cambio a red operativa inválido
**Dado** que el usuario ingresa credenciales incorrectas  
**cuando** el dispositivo intenta conectarse  
**entonces** informa fallo, vuelve a disponibilizar redes detectadas y permite un nuevo intento.

### CA-E2E-05 — Control manual
**Dado** un dispositivo online  
**cuando** el usuario enciende, apaga o cambia color  
**entonces** el NeoPixel refleja la orden y el backend conserva el último estado.

### CA-E2E-06 — Blink con luz encendida
**Dado** un dispositivo con modo `BLINK` y lámpara encendida  
**cuando** el PIR detecta movimiento  
**entonces** se registra el evento y la lámpara parpadea por X segundos para luego quedar encendida.

### CA-E2E-07 — Blink con luz apagada
**Dado** un dispositivo con modo `BLINK` y lámpara apagada  
**cuando** el PIR detecta movimiento  
**entonces** se registra el evento y la lámpara parpadea por X segundos para luego quedar apagada.

### CA-E2E-08 — Sin blink
**Dado** un dispositivo con modo `NO_BLINK`  
**cuando** el PIR detecta movimiento  
**entonces** se registra el evento y la lámpara no cambia su estado visual.

### CA-E2E-09 — Operación offline
**Dado** un dispositivo con horario y modo de movimiento ya configurados  
**cuando** pierde conectividad  
**entonces** sigue cumpliendo la programación y la lógica local, pero no registra eventos en backend.

### CA-E2E-10 — WatchDog
**Dado** un bloqueo del firmware  
**cuando** vence el WatchDog  
**entonces** el dispositivo reinicia y reingresa al flujo normal de arranque.

## 17. Riesgos y puntos a validar
1. **Persistencia de credenciales WiFi en el dispositivo**: validar si se guardarán solo en RAM, NVS o ambas.
2. **Manejo de passwords WiFi en Google Sheets**: definir estrategia mínima de resguardo.
3. **Heartbeat y polling**: definir frecuencia exacta.
4. **Precedencia entre horario programado y control manual**: se propone la regla indicada en `SYS-BR-008`.
5. **Manejo horario de intervalos que cruzan medianoche**: debe validarse para v1.
6. **Debounce o anti-rebote PIR**: conviene definir ventana mínima entre eventos consecutivos.
7. **Fuente horaria inicial del RTC**: definir cómo se ajusta la hora del DS3231 al momento de provisión.
8. **Comportamiento visual exacto del blink**: frecuencia de parpadeo y restauración del color previo.
9. **Detección de offline en backend**: definir cuánto tiempo sin heartbeat implica `OFFLINE`.

## 18. Decisiones recomendadas para cerrar antes de implementación
- Confirmar si el backend será:
  - Google Sheets + Apps Script, o
  - Google Sheets + otra API intermedia.
- Confirmar estructura final de tablas.
- Confirmar intervalo de heartbeat.
- Confirmar intervalo de reconexión offline.
- Confirmar estrategia de cifrado/ocultamiento de password WiFi.
- Confirmar si se almacenará buffer local de eventos en una siguiente fase.

## 19. Anexos sugeridos
- Diagrama de estados del dispositivo
- Contrato API
- Diccionario de datos
- Plan de pruebas
