# HydroControl — arquitectura del firmware

El firmware está dividido por responsabilidad para evitar que `main.cpp` concentre WiFi, APIs, perfiles, reloj y almacenamiento.

## Módulos

- `app_state.*`: estructuras y estado compartido del sistema.
- `config.*`: carga y guardado de la configuración general en NVS (`Preferences`).
- `wifi_manager.*`: modo local, credenciales, conexión al router, mDNS y seguimiento del estado real de la conexión.
- `wifi_portal.*`: interfaz HTML del portal inicial de configuración WiFi.
- `clock_manager.*`: reloj manual, programación de luz, control automático/manual y API del reloj.
- `light_output.*`: salida física de iluminación, GPIO 23, estado seguro de arranque y accionamiento del relé.
- `event_logger.*`: registro persistente y consulta de eventos.
- `profile_manager.*`: creación, edición, comparación, aplicación y eliminación de perfiles de cultivo.
- `ph.*`: configuración y API general de pH.
- `manual_ph_dosing.*`: secuencias manuales no bloqueantes, límites de seguridad y estado de progreso.
- `system_manager.*`: memoria, flash, LittleFS, reinicio y diagnóstico.
- `api.*`: estado general y versión del firmware.
- `web_server.*`: archivos de LittleFS y registro central de rutas HTTP.
- `utils.*`: funciones comunes de JSON, modo y motivo de reinicio.
- `main.cpp`: secuencia de arranque y ciclo principal.

## Perfiles de cultivo

Cada perfil guarda la configuración automática y de iluminación propia de una etapa de cultivo:

- objetivo, tolerancia y modo automático de pH;
- duración e intervalo de dosis automáticas;
- máximo de dosis automáticas permitido dentro de una ventana de 24 horas;
- EC objetivo;
- horario de luz;
- modo de iluminación automático, manual encendido o manual apagado.

La duración y el límite de las órdenes manuales pertenecen exclusivamente a la página pH y no forman parte de los perfiles. Por eso, modificar un límite manual no invalida el perfil activo y aplicar un perfil no altera la configuración manual.

El campo `maxDailyDoses` representa una cuota automática por 24 horas, no una cantidad de dosis consecutivas. Cuando se incorpore el ejecutor automático, deberá rechazar una corrección si las dosis automáticas completadas durante las 24 horas anteriores ya alcanzaron esa cuota. Las dosis manuales se contabilizan por separado.

El perfil activo se conserva mientras la configuración incluida en el perfil siga coincidiendo. Guardar sin modificar valores no lo desactiva. La aplicación o edición del perfil activo queda bloqueada durante una dosificación manual de pH.

## Salida física de iluminación

La iluminación ya no es solamente lógica. `light_output.*` conecta el estado calculado por `clock_manager.*` con el módulo de relé real.

- señal del relé: GPIO 23;
- módulo alimentado a 5 V de forma externa junto con el ESP32;
- jumper del módulo: `H`;
- lógica: `HIGH` en GPIO 23 = relé activado, `LOW` = relé desactivado;
- al arrancar, la salida se fuerza primero a OFF antes de aplicar el estado configurado;
- en modo manual, el switch del Dashboard gobierna directamente la salida;
- en modo automático, la salida sigue el horario guardado;
- si el horario automático está activo pero el reloj no está configurado, la salida permanece OFF;
- aplicar un perfil también termina reflejándose en el relé porque la salida se sincroniza continuamente en el `loop()`;
- los cambios físicos de ON/OFF se registran en Eventos.

La API de reloj/luz expone `outputAvailable`, `outputOn`, `outputPin` y `outputActiveLevel` para distinguir el estado físico del estado solicitado.

## Interfaz web

- Bootstrap se sirve desde LittleFS.
- Los iconos se sirven desde `data/css/icons.css` como máscaras SVG embebidas, sin fuentes ni conexión a Internet.
- Las páginas EC y Sensores muestran el estado real de integración y no generan lecturas simuladas.
- Los controles Bootstrap (`input`, `select` y grupos de unidades) usan el mismo tema oscuro, bordes y foco verde que el resto de HydroControl.
- El Dashboard recibe el perfil activo dentro de `/api/status`, lo muestra en vivo y permite abrir Perfiles desde el indicador.

## Regla de crecimiento

Los sensores y actuadores nuevos deben agregarse como módulos separados. `main.cpp` solo debe inicializarlos y ejecutar sus tareas periódicas.
