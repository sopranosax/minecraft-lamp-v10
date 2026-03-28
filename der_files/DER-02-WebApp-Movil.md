# DER-02 — Especificación de Requerimientos de la Web App Móvil

## 1. Información del documento
- **Nombre**: DER Web App Móvil
- **Código**: DER-02
- **Versión**: 1.0

## 2. Propósito
Definir el comportamiento funcional y no funcional de la web app móvil que permitirá a usuarios autenticados administrar dispositivos ESP32 tipo lámpara WiFi.

## 3. Alcance específico
La web app deberá permitir:
- autenticación,
- listado de lámparas,
- visualización de estado online,
- configuración de red WiFi del dispositivo,
- control manual de encendido/apagado y color,
- configuración de reacción ante movimiento,
- programación horaria basada en RTC,
- visualización de información relevante de operación.

## 4. Perfiles de usuario
Para esta versión se considera un único perfil funcional:
- **Usuario autenticado**

> Si más adelante se requieren roles, se recomienda incorporar `ADMIN` y `VIEWER`, pero eso queda fuera del alcance base de este DER.

## 5. Objetivos de UX
- Interfaz simple, móvil y clara.
- Flujo corto para provisión WiFi del dispositivo.
- Baja fricción para control de la lámpara.
- Retroalimentación explícita de estado: online, offline, conectando, error WiFi.
- Protección contra acciones ambiguas o destructivas.

## 6. Mapa funcional
1. Pantalla de login
2. Pantalla de listado de dispositivos
3. Pantalla de detalle/configuración del dispositivo
4. Pantalla o secciones de:
   - configuración WiFi,
   - control manual,
   - comportamiento por movimiento,
   - programación horaria,
   - visualización básica de eventos/estado

## 7. Requerimientos funcionales
### 7.1 Autenticación
- **APP-FR-001**: La app debe presentar una pantalla de login con campos `login` y `password`.
- **APP-FR-002**: La app debe validar que ambos campos estén informados antes de enviar.
- **APP-FR-003**: La app debe autenticar al usuario contra el backend.
- **APP-FR-004**: La app debe informar error de autenticación cuando las credenciales no sean válidas.
- **APP-FR-005**: La app debe mantener una sesión autenticada hasta logout o expiración.
- **APP-FR-006**: La app debe permitir cerrar sesión.

### 7.2 Listado de dispositivos
- **APP-FR-007**: La app debe listar los dispositivos registrados visibles para el usuario.
- **APP-FR-008**: Cada elemento listado debe mostrar, como mínimo:
  - nombre o alias del dispositivo,
  - MAC address,
  - estado online/offline,
  - último contacto.
- **APP-FR-009**: La app debe permitir filtrar o resaltar dispositivos online.
- **APP-FR-010**: La app debe permitir ingresar al detalle de un dispositivo seleccionado.

### 7.3 Detalle del dispositivo
- **APP-FR-011**: La pantalla de detalle debe mostrar:
  - MAC address,
  - estado actual,
  - red WiFi operativa si aplica,
  - estado de la lámpara (encendida/apagada),
  - color actual,
  - modo ante movimiento,
  - segundos de blink configurados,
  - horario programado vigente,
  - intervalo de reconexión configurado.
- **APP-FR-012**: La app debe reflejar mensajes de error o advertencia del dispositivo, especialmente fallos WiFi.

### 7.4 Configuración WiFi
- **APP-FR-013**: La app debe mostrar la lista de redes detectadas por el dispositivo.
- **APP-FR-014**: Cada red detectada debe mostrar:
  - SSID,
  - intensidad o RSSI,
  - fecha/hora del escaneo.
- **APP-FR-015**: La app debe permitir seleccionar un SSID detectado.
- **APP-FR-016**: La app debe permitir ingresar el password de la red elegida.
- **APP-FR-017**: La app debe validar que el SSID seleccionado pertenezca al dispositivo consultado.
- **APP-FR-018**: La app debe enviar la configuración WiFi al backend asociada al dispositivo correcto.
- **APP-FR-019**: La app debe mostrar el estado del proceso:
  - pendiente,
  - aplicando,
  - conectado,
  - fallo.
- **APP-FR-020**: Si la conexión falla, la app debe permitir reintentar sin abandonar el detalle del dispositivo.

### 7.5 Control manual de la lámpara
- **APP-FR-021**: La app debe incluir un control para encender la lámpara.
- **APP-FR-022**: La app debe incluir un control para apagar la lámpara.
- **APP-FR-023**: La app debe incluir un color picker para seleccionar color.
- **APP-FR-024**: La app debe enviar al backend el estado manual deseado y el color seleccionado.
- **APP-FR-025**: La app debe mostrar el estado deseado mientras espera confirmación del dispositivo.
- **APP-FR-026**: La app debe indicar si el comando fue aceptado o si no pudo confirmarse.

### 7.6 Comportamiento ante movimiento
- **APP-FR-027**: La app debe permitir elegir entre:
  - `BLINK`,
  - `NO_BLINK`.
- **APP-FR-028**: Si el modo es `BLINK`, la app debe permitir ingresar la cantidad de segundos.
- **APP-FR-029**: La app debe validar que los segundos sean numéricos y mayores que cero.
- **APP-FR-030**: La app debe guardar la configuración de movimiento en el backend.
- **APP-FR-031**: La app debe mostrar claramente la política vigente configurada para el dispositivo.

### 7.7 Programación horaria
- **APP-FR-032**: La app debe permitir activar o desactivar la programación horaria.
- **APP-FR-033**: La app debe permitir definir hora de inicio.
- **APP-FR-034**: La app debe permitir definir hora de término.
- **APP-FR-035**: La app debe permitir seleccionar color programado mediante color picker.
- **APP-FR-036**: La app debe validar formato horario y coherencia mínima de los datos.
- **APP-FR-037**: La app debe guardar la programación horaria en el backend.
- **APP-FR-038**: La app debe mostrar la programación vigente y su estado.

### 7.8 Monitoreo básico
- **APP-FR-039**: La app debe mostrar si el dispositivo está online u offline.
- **APP-FR-040**: La app debe mostrar el último contacto informado por backend.
- **APP-FR-041**: La app debe mostrar el último fallo WiFi si existe.
- **APP-FR-042**: La app debería poder mostrar una vista básica del último movimiento detectado.
- **APP-FR-043**: La app debería advertir al usuario que, estando offline, el dispositivo seguirá operando localmente pero no generará logs en backend.

## 8. Requerimientos de interfaz
### 8.1 Login
Campos:
- Login
- Password
- Botón ingresar

Estados:
- vacío
- cargando
- error
- autenticado

### 8.2 Lista de dispositivos
Campos visibles por tarjeta o fila:
- Alias/nombre
- MAC address
- Estado
- Último contacto

Acciones:
- abrir detalle

### 8.3 Sección WiFi
Campos:
- listado SSID detectados
- RSSI
- password
- botón guardar/aplicar

Estados:
- sin escaneo disponible
- credenciales pendientes
- conectando
- conectado
- fallo

### 8.4 Sección control manual
Componentes:
- switch ON/OFF
- color picker
- indicador de último estado confirmado

### 8.5 Sección movimiento
Componentes:
- selector de modo (`BLINK`/`NO_BLINK`)
- input segundos
- texto explicativo de efecto esperado

### 8.6 Sección horario
Componentes:
- toggle activar horario
- selector hora inicio
- selector hora fin
- color picker
- resumen de la configuración activa

## 9. Reglas de validación en front
- **APP-BR-001**: No permitir envío de login vacío.
- **APP-BR-002**: No permitir envío de password vacío.
- **APP-BR-003**: No permitir guardar WiFi sin SSID seleccionado.
- **APP-BR-004**: No permitir guardar modo `BLINK` sin segundos válidos.
- **APP-BR-005**: No permitir guardar horarios con formato inválido.
- **APP-BR-006**: El color picker debe entregar un formato de color acordado con backend, idealmente HEX.
- **APP-BR-007**: Si el dispositivo está offline, la app puede permitir cambiar configuración, pero debe informar que la aplicación efectiva dependerá de la próxima sincronización.

## 10. Requerimientos no funcionales
### 10.1 Usabilidad
- **APP-NFR-001**: La interfaz debe ser usable principalmente desde móvil.
- **APP-NFR-002**: Los estados del dispositivo deben ser comprensibles sin lenguaje técnico excesivo.
- **APP-NFR-003**: Las acciones críticas deben entregar feedback inmediato.

### 10.2 Seguridad
- **APP-NFR-004**: La app no debe mostrar passwords de usuarios ni de redes WiFi en texto plano.
- **APP-NFR-005**: La sesión debe invalidarse al cerrar sesión.
- **APP-NFR-006**: Las llamadas al backend deben incluir un mecanismo de autenticación de sesión o token.

### 10.3 Robustez
- **APP-NFR-007**: La app debe manejar caídas temporales del backend mostrando mensajes claros.
- **APP-NFR-008**: La app debe evitar dobles envíos accidentales de formularios.
- **APP-NFR-009**: La app debe tolerar estados intermedios del dispositivo, por ejemplo `WIFI_CONNECTING`.

## 11. Criterios de aceptación
### CA-APP-01
**Dado** un usuario válido  
**cuando** ingresa login y password correctos  
**entonces** accede al listado de dispositivos.

### CA-APP-02
**Dado** un dispositivo con escaneo disponible  
**cuando** el usuario entra al detalle  
**entonces** ve la lista de SSID detectados con intensidad.

### CA-APP-03
**Dado** un dispositivo seleccionado  
**cuando** el usuario envía SSID y password  
**entonces** la app muestra el avance del proceso de conexión.

### CA-APP-04
**Dado** un dispositivo online  
**cuando** el usuario cambia estado ON/OFF o color  
**entonces** la app refleja la operación solicitada y el backend recibe el comando.

### CA-APP-05
**Dado** un dispositivo  
**cuando** el usuario configura `BLINK` con X segundos  
**entonces** la app guarda esa política y la muestra como vigente.

### CA-APP-06
**Dado** un dispositivo  
**cuando** el usuario define horario y color programado  
**entonces** la app persiste la configuración y la presenta resumida en pantalla.

## 12. Puntos de diseño sugeridos
- Usar polling liviano o refresh manual para estados del dispositivo.
- Presentar un alias editable para el dispositivo además de la MAC.
- Usar componentes visuales simples y legibles.
- Mostrar mensajes explícitos como:
  - “Conectando dispositivo a nueva red…”
  - “No se pudo conectar. Verifique el password e intente nuevamente.”
  - “El dispositivo está offline, pero seguirá operando con su última configuración local.”

## 13. Dependencias
- API/backend definido en DER-03
- Estados y capacidades del firmware definidos en DER-04
