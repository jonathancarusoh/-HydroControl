# HydroControl — arquitectura del firmware

El firmware está dividido por responsabilidad para evitar que `main.cpp` concentre WiFi, APIs, perfiles, reloj y almacenamiento.

## Módulos

- `app_state.*`: estructuras y estado compartido del sistema.
- `config.*`: carga y guardado de la configuración general en NVS (`Preferences`).
- `wifi_manager.*`: modo local, credenciales, conexión al router y mDNS.
- `wifi_portal.*`: interfaz HTML del portal inicial de configuración WiFi.
- `clock_manager.*`: reloj manual, programación de luz y API del reloj.
- `event_logger.*`: registro persistente y consulta de eventos.
- `profile_manager.*`: creación, edición, aplicación y eliminación de perfiles.
- `ph.*`: API de configuración de pH.
- `system_manager.*`: memoria, flash, LittleFS, reinicio y diagnóstico.
- `api.*`: estado general y versión del firmware.
- `web_server.*`: archivos de LittleFS y registro central de rutas HTTP.
- `utils.*`: funciones comunes de JSON, modo y motivo de reinicio.
- `main.cpp`: secuencia de arranque y ciclo principal.

## Regla de crecimiento

Los sensores y actuadores nuevos deben agregarse como módulos separados. `main.cpp` solo debe inicializarlos y ejecutar sus tareas periódicas.
