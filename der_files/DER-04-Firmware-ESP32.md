# DER-04 — Especificación de Requerimientos del Firmware ESP32

## 1. Información del documento
- **Nombre**: DER Firmware ESP32 + PIR + NeoPixel + RTC
- **Código**: DER-04
- **Versión**: 1.1

## 2. Propósito
Definir el comportamiento del firmware del ESP32 implementado con Arduino para soportar:
- bootstrap WiFi,
- registro por MAC,
- escaneo de redes,
- aplicación de configuración remota,
- control del NeoPixel WS2812B,
- lectura de PIR HC-SR501,
- programación horaria con DS3231,
- operación offline,
- reconexión,
- WatchDog.

## 3. Componentes de hardware
- **Microcontrolador**: ESP32
- **Sensor de movimiento**: PIR HC-SR501
- **Lámpara**: WS2812B NeoPixel RGB
- **RTC**: DS3231

## 4. Responsabilidades del firmware
1. Arrancar el dispositivo y cargar configuración local.
2. Conectarse a la red bootstrap.
3. Identificarse por MAC ante el backend.
4. Escanear redes WiFi y publicar resultados.
5. Obtener configuración remota vigente.
6. Aplicar credenciales WiFi operativas.
7. Controlar LED según estado manual, horario y eventos PIR.
8. Mantener lógica local cuando no haya internet.
9. Reintentar conexión según configuración.
10. Reiniciarse por WatchDog si el programa se bloquea.

## 5. Estados lógicos del dispositivo
Estados propuestos:
- `BOOTING`
- `BOOTSTRAP_CONNECTING`
- `BOOTSTRAP_ONLINE`
- `REGISTERING`
- `READY_FOR_WIFI_CONFIG`
- `WIFI_CONFIG_PENDING`
- `WIFI_CONNECTING`
- `WIFI_CONNECTED`
- `OFFLINE_LOCAL_MODE`
- `ERROR_RECOVERABLE`
- `WATCHDOG_RECOVERY`

## 6. Requerimientos funcionales del firmware
### 6.1 Arranque
- **FW-FR-001**: El firmware debe inicializar WiFi, NeoPixel, PIR, RTC y WatchDog.
- **FW-FR-002**: El firmware debe obtener su MAC address al arrancar.
- **FW-FR-003**: El firmware debe conectarse primero usando las credenciales WiFi bootstrap compiladas.
- **FW-FR-004**: Si logra conectarse a la red bootstrap, debe informar presencia al backend.
- **FW-FR-005**: Si la red bootstrap no está disponible, el firmware debe entrar en modo de reconexión periódica.

### 6.2 Registro y bootstrap
- **FW-FR-006**: Tras conectarse, el firmware debe consultar si el dispositivo ya existe por MAC.
- **FW-FR-007**: Si no existe, debe solicitar su registro automático.
- **FW-FR-008**: Si existe, debe marcarse online.
- **FW-FR-009**: El firmware debe descargar la configuración vigente del dispositivo tras bootstrap exitoso.

### 6.3 Escaneo WiFi
- **FW-FR-010**: El firmware debe ejecutar escaneo de redes WiFi visibles.
- **FW-FR-011**: El firmware debe capturar al menos SSID y RSSI de cada red.
- **FW-FR-012**: El firmware debe publicar el escaneo al backend.
- **FW-FR-013**: El firmware debe quedar a la espera de credenciales WiFi válidas enviadas desde la web app.
- **FW-FR-014**: Mientras no exista configuración válida aplicada, el firmware debe poder repetir el ciclo de escaneo-publicación-espera.

### 6.4 Aplicación de nueva WiFi operativa
- **FW-FR-015**: El firmware debe consultar periódicamente si hay credenciales WiFi pendientes.
- **FW-FR-016**: Si existen credenciales pendientes, debe intentar conectarse a esa red.
- **FW-FR-017**: Durante el intento debe reportar estado `WIFI_CONNECTING` cuando sea posible.
- **FW-FR-018**: Si la conexión resulta exitosa, debe:
  - actualizar estado online,
  - persistir la red operativa vigente,
  - descargar configuración del dispositivo.
- **FW-FR-019**: Si la conexión falla, debe:
  - reportar fallo cuando sea posible,
  - conservar o restaurar el flujo para nueva configuración,
  - volver a publicar redes detectadas.
- **FW-FR-020**: El firmware debe poder reusar la última red operativa válida como destino principal de reconexión.

### 6.5 Persistencia local
- **FW-FR-021**: El firmware debe persistir localmente, al menos, la última configuración operativa necesaria para seguir funcionando sin internet:
  - estado manual,
  - color manual,
  - modo de movimiento,
  - segundos de blink,
  - horario habilitado,
  - hora inicio,
  - hora fin,
  - color programado,
  - red operativa vigente,
  - intervalo de reconexión.
- **FW-FR-022**: Después de un reinicio, el firmware debe restaurar dicha configuración local antes de entrar en modo operativo.
- **FW-FR-023**: La persistencia local debe usar un mecanismo adecuado del ESP32, por ejemplo NVS/Preferences u otro equivalente.

### 6.6 Control del NeoPixel
- **FW-FR-024**: El firmware debe controlar el NeoPixel en al menos dos dimensiones:
  - encendido/apagado,
  - color RGB.
- **FW-FR-025**: El firmware debe aplicar el color manual seleccionado por el usuario.
- **FW-FR-026**: El firmware debe apagar completamente el NeoPixel cuando el estado sea `OFF`.
- **FW-FR-027**: El firmware debe poder restaurar el color previo después de un blink temporal.

### 6.7 Lógica PIR
- **FW-FR-028**: El firmware debe leer el estado del sensor PIR.
- **FW-FR-029**: Ante detección de movimiento, debe determinar la política vigente:
  - `BLINK`,
  - `NO_BLINK`.
- **FW-FR-030**: Si la política es `BLINK`, el firmware debe ejecutar blink por el número de segundos configurado.
- **FW-FR-031**: Si la lámpara estaba encendida antes del blink, debe volver a encendida con su color previo.
- **FW-FR-032**: Si la lámpara estaba apagada antes del blink, debe volver a apagada al finalizar.
- **FW-FR-033**: Si la política es `NO_BLINK`, no debe modificar el estado visual de la lámpara.
- **FW-FR-034**: Si hay conectividad, el firmware debe registrar el evento de movimiento en backend.
- **FW-FR-035**: Si no hay conectividad, no debe intentar almacenar buffer de movimientos para reenviar, salvo decisión futura distinta.

### 6.8 Programación horaria con RTC
- **FW-FR-036**: El firmware debe consultar el RTC DS3231 para conocer la hora local.
- **FW-FR-037**: Si la programación está habilitada, el firmware debe evaluar continuamente si está dentro o fuera del intervalo programado.
- **FW-FR-038**: Al entrar al intervalo programado, debe encender la lámpara con el color configurado.
- **FW-FR-039**: Al salir del intervalo programado, debe apagar la lámpara.
- **FW-FR-040**: La programación debe seguir operando aun sin conectividad WiFi.
- **FW-FR-041**: El firmware debe soportar intervalos normales y, si se aprueba, intervalos que crucen medianoche.

### 6.9 Sincronización de configuración
- **FW-FR-042**: El firmware debe consultar periódicamente la configuración vigente desde backend cuando haya conectividad.
- **FW-FR-043**: El firmware debe aplicar cambios en:
  - estado manual,
  - color,
  - movimiento,
  - horario,
  - intervalo de reconexión.
- **FW-FR-044**: Al aplicar cambios confirmados, debe persistirlos localmente.

### 6.10 Operación offline y reconexión
- **FW-FR-045**: Si la red WiFi cae, el firmware debe entrar a modo `OFFLINE_LOCAL_MODE`.
- **FW-FR-046**: En este modo debe seguir:
  - monitoreando PIR,
  - ejecutando blink o no blink,
  - ejecutando programación horaria.
- **FW-FR-047**: En modo offline, el firmware debe intentar reconectar primero a la **última red operativa válida** usando el intervalo configurado.
- **FW-FR-048**: El firmware debe ejecutar una fase de **reintentos rápidos** inmediatamente después de detectar la caída, antes de pasar al ciclo normal de reconexión.
- **FW-FR-049**: Si la reconexión falla repetidamente, debe seguir operando localmente sin bloquear el loop principal.
- **FW-FR-050**: Tras N ciclos fallidos configurables, el firmware debe intentar conectarse temporalmente a la **red bootstrap** para publicar diagnóstico, escaneo WiFi y consultar nuevas credenciales.
- **FW-FR-051**: Si logra conectarse a bootstrap en modo recuperación, debe informar el estado `RECOVERY_BOOTSTRAP_CONNECTED`.
- **FW-FR-052**: Si desde bootstrap obtiene nuevas credenciales válidas, debe intentar aplicarlas sin requerir intervención física.
- **FW-FR-053**: Si recupera conectividad a la red operativa, debe actualizar estado online, salir de modo recuperación y refrescar configuración desde backend.

### 6.11 WatchDog
- **FW-FR-054**: El firmware debe alimentar periódicamente el WatchDog dentro del ciclo principal y/o tareas críticas.
- **FW-FR-055**: Si una tarea bloquea la ejecución y el WatchDog expira, el microcontrolador debe reiniciarse.
- **FW-FR-056**: Tras reinicio por WatchDog, el firmware debe restaurar su configuración persistida.
- **FW-FR-057**: Tras reinicio por WatchDog, el firmware debería informar un evento de reinicio al backend cuando recupere conectividad.

## 7. Reglas de comportamiento
- **FW-BR-001**: La MAC address nunca cambia durante la vida lógica del dispositivo.
- **FW-BR-002**: La lógica local debe priorizar continuidad operativa antes que logging remoto.
- **FW-BR-003**: El blink es un efecto temporal y no debe destruir el estado persistente principal de la lámpara.
- **FW-BR-004**: Si existe horario activo, este gobierna la franja temporal; el control manual impacta el estado hasta la próxima transición horaria relevante.
- **FW-BR-005**: Las operaciones de red no deben bloquear indefinidamente el loop principal.
- **FW-BR-006**: Los reintentos de reconexión deben ser temporizados, no continuos.
- **FW-BR-007**: El RTC es la referencia horaria local mientras el dispositivo no disponga de sincronización externa.

## 8. Algoritmos y flujos recomendados
### 8.1 Ciclo de arranque
1. Inicializar hardware y variables.
2. Recuperar configuración persistida.
3. Conectar a bootstrap o a red operativa según estrategia definida.
4. Informar bootstrap/heartbeat al backend si hay conectividad.
5. Obtener configuración remota.
6. Escanear y publicar redes si corresponde.
7. Entrar al loop operativo.

### 8.2 Loop operativo
1. Alimentar WatchDog.
2. Revisar estado WiFi.
3. Si online, sincronizar configuración según polling.
4. Evaluar programación horaria con RTC.
5. Leer PIR.
6. Si detecta movimiento, ejecutar política y loguear si aplica.
7. Dormir o esperar de forma no bloqueante.
8. Intentar reconexión cuando corresponda.

### 8.3 Estrategia recomendada de reconexión simple
**Objetivo**: máxima simpleza con buena resiliencia y mínima complejidad de firmware.

#### Fase A — caída detectada
1. Marcar `OFFLINE_LOCAL_MODE`.
2. Guardar timestamp de caída y contador de fallos.
3. Ejecutar 3 reintentos rápidos a la última red operativa válida, por ejemplo cada 10 segundos.
4. Durante estos reintentos, nunca bloquear PIR, RTC ni NeoPixel.

#### Fase B — recuperación local
1. Si los 3 reintentos rápidos fallan, entrar en `RECOVERY_MODE`.
2. Seguir operando completamente en local con la última configuración persistida.
3. Intentar reconectar a la última red operativa cada `RECONNECT_INTERVAL_SECONDS`.

#### Fase C — retorno a bootstrap
1. Llevar un contador de ciclos fallidos de reconexión.
2. Cada `RECOVERY_BOOTSTRAP_EVERY_N_CYCLES` fallos, intentar conectarse por una ventana corta a la red bootstrap.
3. Si bootstrap conecta:
   - publicar `RECOVERY_BOOTSTRAP_CONNECTED`,
   - enviar último error WiFi,
   - publicar nuevo escaneo de redes,
   - consultar si existen nuevas credenciales.
4. Si no hay nuevas credenciales, salir de bootstrap y volver a `RECOVERY_MODE`.
5. Si hay nuevas credenciales, intentar aplicarlas inmediatamente.

#### Valores iniciales sugeridos
- Reintentos rápidos iniciales: **3**
- Intervalo entre reintentos rápidos: **10 s**
- `RECONNECT_INTERVAL_SECONDS`: **60 s**
- `RECOVERY_BOOTSTRAP_EVERY_N_CYCLES`: **5**
- Ventana máxima de conexión bootstrap para recuperación: **30–60 s**

#### Ventajas de esta estrategia
- Es simple de programar.
- No exige buffer offline ni colas complejas.
- Mantiene el comportamiento local del dispositivo.
- Permite reconfigurar remotamente el equipo cuando la red operativa ya no existe o cambió su password.

### 8.4 Blink recomendado
Secuencia propuesta:
1. Guardar estado previo (`power`, `color`).
2. Ejecutar alternancia ON/OFF o cambio visible por duración total.
3. Restaurar estado previo al terminar.

## 9. Requerimientos no funcionales
### 9.1 Confiabilidad
- **FW-NFR-001**: El firmware no debe usar delays largos que impidan alimentar el WatchDog o atender PIR.
- **FW-NFR-002**: El firmware debe estar organizado en módulos: WiFi, backend client, LED control, PIR, RTC, storage, watchdog.
- **FW-NFR-003**: El sistema debe ser capaz de recuperarse de caídas temporales de red sin reinicio manual.

### 9.2 Desempeño
- **FW-NFR-004**: La detección de movimiento debe ser respondida con latencia perceptiblemente baja.
- **FW-NFR-005**: Los intentos de red no deben afectar de forma grave la capacidad de reaccionar al PIR o al horario.

### 9.3 Mantenibilidad
- **FW-NFR-006**: Los parámetros configurables deben centralizarse.
- **FW-NFR-007**: Los nombres de estados y eventos deben coincidir con el backend.
- **FW-NFR-008**: Deben existir logs seriales de depuración controlables por bandera de compilación.

## 10. Criterios de aceptación
### CA-FW-01
**Dado** un dispositivo nuevo  
**cuando** inicia con red bootstrap disponible  
**entonces** se registra y publica redes detectadas.

### CA-FW-02
**Dado** credenciales WiFi válidas pendientes  
**cuando** las consulta y aplica  
**entonces** logra conectarse y reporta `WIFI_CONNECTED`.

### CA-FW-03
**Dado** credenciales inválidas  
**cuando** intenta conectarse  
**entonces** informa `WIFI_FAILED` y vuelve al ciclo de publicación de redes.

### CA-FW-04
**Dado** un cambio manual de color o encendido  
**cuando** sincroniza configuración  
**entonces** el NeoPixel refleja la nueva configuración.

### CA-FW-05
**Dado** modo `BLINK`  
**cuando** el PIR detecta movimiento  
**entonces** ejecuta blink y restaura el estado previo correcto.

### CA-FW-06
**Dado** programación activa y WiFi caída  
**cuando** llega la hora de inicio o término  
**entonces** la lámpara cambia de estado usando la hora del RTC.

### CA-FW-07
**Dado** pérdida de WiFi  
**cuando** transcurre el intervalo configurado  
**entonces** el firmware intenta reconectar sin congelar la ejecución principal.

### CA-FW-08
**Dado** un bloqueo del programa  
**cuando** expira el WatchDog  
**entonces** el equipo reinicia y recupera la configuración persistida.

## 11. Decisiones pendientes de firmware
1. **Resuelto**: se usará estrategia **mixta escalonada**: reintentos rápidos a la red operativa y fallback periódico a bootstrap en modo recuperación.
2. Definir ventana anti-rebote o cooldown del PIR.
3. Definir formato interno del color.
4. Definir frecuencia exacta del polling al backend.
5. Definir cómo se ajusta inicialmente el RTC.
6. Definir si el loop usará FreeRTOS tasks o un solo loop cooperativo.
