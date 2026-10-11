# HydroControl — arquitectura del firmware

El firmware está dividido por responsabilidad para evitar que `main.cpp` concentre WiFi, APIs, perfiles, reloj y almacenamiento.

## Módulos

- `app_state.*`: estructuras y estado compartido del sistema.
- `config.*`: carga y guardado de la configuración general en NVS (`Preferences`).
- `wifi_manager.*`: modo local, credenciales, conexión al router, mDNS y seguimiento del estado real de la conexión.
- `wifi_portal.*`: interfaz HTML del portal inicial de configuración WiFi.
- `clock_manager.*`: reloj manual, programación de luz, control automático/manual y API del reloj.
- `light_output.*`: salida física de iluminación, GPIO 25, estado seguro de arranque y accionamiento reforzado del relé.
- `event_logger.*`: registro persistente y consulta de eventos.
- `profile_manager.*`: creación, edición, comparación, aplicación y eliminación de perfiles de cultivo.
- `ph.*`: configuración y API general de pH.
- `ph_sensor.*`: lectura real del PH-4502C en GPIO34, filtrado ADC, diagnóstico de saturación y calibración persistente de dos puntos (pH 7 / pH 4).
- `pump_outputs.*`: salidas físicas de las bombas pH- (GPIO26) y pH+ (GPIO27), arranque seguro e interbloqueo.
- `manual_ph_dosing.*`: secuencias manuales no bloqueantes, límites de seguridad y accionamiento físico de bombas.
- `automatic_ph_dosing.*`: regulación automática física con sensor estable, reloj obligatorio, intervalo y cuota móvil de 24 horas.
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

El campo `maxDailyDoses` representa una cuota automática por 24 horas, no una cantidad de dosis consecutivas. El ejecutor automático guarda las marcas horarias de las dosis completadas y rechaza una corrección cuando la ventana móvil de 24 horas ya alcanzó esa cuota. Las dosis manuales se contabilizan por separado. Para garantizar esa ventana después de reinicios, el automático exige que el reloj interno esté configurado.

El perfil activo se conserva mientras la configuración incluida en el perfil siga coincidiendo. Guardar sin modificar valores no lo desactiva. La aplicación o edición del perfil activo queda bloqueada durante cualquier dosificación de pH, manual o automática.

## Salida física de iluminación

La iluminación ya no es solamente lógica. `light_output.*` conecta el estado calculado por `clock_manager.*` con el módulo de relé real.

- señal del relé: GPIO 25;
- módulo alimentado a 5 V de forma externa junto con el ESP32;
- jumper del módulo: `H`;
- lógica: `HIGH` en GPIO 25 = relé activado, `LOW` = relé desactivado;
- al arrancar, la salida se fuerza primero a OFF antes de aplicar el estado configurado;
- en modo manual, el switch del Dashboard gobierna directamente la salida;
- en modo automático, la salida sigue el horario guardado;
- si el horario automático está activo pero el reloj no está configurado, la salida permanece OFF;
- aplicar un perfil también termina reflejándose en el relé porque la salida se sincroniza continuamente en el `loop()`;
- los cambios físicos de ON/OFF se registran en Eventos.

La API de reloj/luz expone `outputAvailable`, `outputOn`, `outputPin` y `outputActiveLevel` para distinguir el estado físico del estado solicitado.

Nota de hardware: GPIO23 queda reservado/no recomendado para salidas críticas en esta placa por el comportamiento inestable observado después de un corto accidental. La iluminación se mantiene en GPIO25 y el firmware reafirma su nivel cada 100 ms.


## Bombas de corrección de pH

- bomba pH-: GPIO26 → TRIG del MOSFET;
- bomba pH+: GPIO27 → TRIG del MOSFET;
- lógica configurada: activa en HIGH;
- ambos GPIO se fuerzan a OFF antes de habilitarlos como salida durante el arranque;
- el firmware nunca permite encender pH- y pH+ al mismo tiempo;
- una secuencia manual desactiva el modo automático y acciona la bomba real;
- la regulación automática solo actúa con sensor calibrado y estable, reloj configurado, intervalo cumplido y cuota de 24 h disponible;
- la primera vez que se instala la versión con actuador automático físico, el modo automático queda desactivado por seguridad y debe habilitarse manualmente después de las pruebas.


## Sensor de pH PH-4502C

La salida analógica `PO` del PH-4502C se lee en `GPIO34`, que pertenece a ADC1 y puede utilizarse mientras el WiFi está activo.

- `V+` del módulo: 5 V.
- `GND` del módulo: GND común.
- `PO`: GPIO34.
- `DO`: no se utiliza.
- resolución ADC: 12 bits;
- atenuación: 11 dB;
- muestreo periódico: 500 ms;
- cada ciclo usa 21 muestras, descarta las 5 más bajas y las 5 más altas y promedia el resto;
- aplica mediana temporal de 5 ciclos y EMA adaptativo para estabilizar la lectura sin arrastrar excesivamente cambios reales;
- la calibración captura la mediana estable actual, no el EMA, para evitar que el buffer anterior contamine el nuevo punto;
- se mide la dispersión dentro de cada lote y entre ciclos; si supera los límites, `stable=false` y el pH publicado pasa a `null`;
- si el ADC queda cerca del máximo se marca `saturated` y no se calcula pH;
- antes de calibrar, HydroControl muestra ADC y tensión reales pero el valor de pH permanece sin dato;
- la calibración utiliza dos puntos: buffer pH 7.00 y buffer pH 4.00;
- ambos milivoltajes se guardan en `Preferences` dentro del namespace `phsensor`;
- el pH se calcula mediante interpolación lineal entre esos dos puntos;
- las rutas son `GET /api/ph/sensor`, `POST /api/ph/calibrate` y `POST /api/ph/calibration/reset`.

La página pH permite ver ADC/mV, estabilidad, ruido de lote/historial, capturar cada buffer y borrar la calibración. Los botones de captura quedan bloqueados mientras la señal sea inestable. El Dashboard recibe el pH real desde `/api/status`; no se usa una lectura simulada.

## Interfaz web

- Bootstrap se sirve desde LittleFS.
- Los iconos se sirven desde `data/css/icons.css` como máscaras SVG embebidas, sin fuentes ni conexión a Internet.
- Las páginas EC y Sensores muestran el estado real de integración y no generan lecturas simuladas.
- Los controles Bootstrap (`input`, `select` y grupos de unidades) usan el mismo tema oscuro, bordes y foco verde que el resto de HydroControl.
- El Dashboard recibe el perfil activo dentro de `/api/status`, lo muestra en vivo y permite abrir Perfiles desde el indicador.

## Regla de crecimiento

Los sensores y actuadores nuevos deben agregarse como módulos separados. `main.cpp` solo debe inicializarlos y ejecutar sus tareas periódicas.
