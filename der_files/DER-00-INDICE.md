# Índice de DER — Sistema IoT Lámpara WiFi + Web App + Backend Google Sheets

## Objetivo del paquete
Este paquete separa la especificación en documentos por capas para facilitar el trabajo en Antigravity, la trazabilidad de requerimientos y la implementación por componentes.

## Documentos incluidos
1. **DER-01-Sistema-General.md**  
   Visión global del producto, alcance, actores, reglas de negocio, casos de uso, requerimientos transversales y criterios de aceptación de extremo a extremo.

2. **DER-02-WebApp-Movil.md**  
   Requerimientos específicos de la web app móvil: autenticación, listado de dispositivos, configuración WiFi, control manual, programación horaria y monitoreo.

3. **DER-03-Backend-Google-Sheets.md**  
   Requerimientos del backend apoyado en Google Sheets: modelo de datos, API lógica, validaciones, sincronización de estados, comandos y logging.

4. **DER-04-Firmware-ESP32.md**  
   Requerimientos del firmware sobre ESP32/Arduino: arranque, bootstrap WiFi, registro por MAC, escaneo de redes, ejecución local, PIR, RTC, watchdog y reconexión.

## Orden recomendado de uso
1. Leer **DER-01-Sistema-General.md**
2. Implementar o refinar contratos y tablas en **DER-03-Backend-Google-Sheets.md**
3. Implementar firmware con **DER-04-Firmware-ESP32.md**
4. Implementar interfaz y flujos de usuario con **DER-02-WebApp-Movil.md**

## Observación importante
En varios puntos, los requerimientos originales obligan a definir comportamientos no explicitados por completo. En esos casos, este paquete distingue entre:
- **Requerimiento explícito**: proviene directamente de tu texto.
- **Requerimiento derivado**: se deduce como necesario para que el sistema funcione de manera consistente.
- **Supuesto pendiente**: decisión que conviene validar antes de construir.

## Estado
Versión inicial derivada de los requerimientos entregados por Sergio.


## Actualización v1.1
- Backend confirmado: **Google Sheets + Google Apps Script**.
- Credenciales WiFi: sin protección adicional en esta versión por decisión de alcance.
- Estrategia de red recomendada: **reintentos rápidos sobre red operativa + modo recuperación + fallback periódico a bootstrap**.
