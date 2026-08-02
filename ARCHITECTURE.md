# HydroControl Architecture

## 1. Descripción del proyecto

HydroControl es un sistema de automatización y monitoreo para cultivo hidropónico basado en ESP32.

El objetivo del sistema es medir, controlar y registrar parámetros críticos del cultivo:

- pH de la solución nutritiva.
- EC (conductividad eléctrica).
- Temperatura de la solución.
- Nivel de agua.
- Temperatura y humedad del indoor.

El sistema permitirá la visualización y configuración mediante una interfaz web accesible por WiFi.

---

# 2. Arquitectura general
                Usuario
                   |
                   |
             Navegador Web
                   |
                   |
                WiFi LAN
                   |
                   |
                ESP32
                   |
    --------------------------------
    |              |               |
 Sensores      Controladores     Web Server
    |              |               |
    |              |               |
   pH            Bombas         HydroControl
   EC            Relés             UI
   Temp          Válvulas

   
---

# 3. Hardware principal

## Microcontrolador

ESP32 NodeMCU 38 Pines USB-C.

Responsabilidades:

- Comunicación WiFi.
- Procesamiento de datos.
- Lectura de sensores.
- Control de actuadores.
- Servidor web.
- Automatizaciones.

---

# 4. Software

## Firmware ESP32

Ubicación:
src/

Responsabilidades:

- Inicialización del sistema.
- Gestión WiFi.
- Lectura sensores.
- Control automático.
- Comunicación con interfaz web.

---

# 5. Interfaz Web

Ubicación:
data/

Los archivos serán almacenados en la memoria flash del ESP32 usando LittleFS.

Estructura:
data/

├── index.html

├── css/
│ └── style.css

├── js/
│ ├── app.js
│ ├── dashboard.js
│ ├── ph.js
│ └── api.js

└── pages/
├── dashboard.html
├── ph.html
├── ec.html
├── sensors.html
└── settings.html

---

# 6. Comunicación Web - ESP32

La comunicación será mediante API REST.

Ejemplo:

Solicitud:
GET /api/status

Respuesta:

```json
{
 "ph":5.8,
 "ec":1.4,
 "temperature":20.5
}

7. Organización del firmware

Estructura futura:
src/

├── main.cpp

├── wifi_manager.cpp
├── wifi_manager.h

├── web_server.cpp
├── web_server.h

├── sensor_manager.cpp
├── sensor_manager.h

├── ph_controller.cpp
├── ph_controller.h

├── ec_controller.cpp
├── ec_controller.h

└── config.cpp
    config.h

    8. Módulos principales
WiFi Manager

Funciones:

Conectar a red WiFi.
Reconectar automáticamente.
Obtener IP.
Configuración de red.
Web Server

Funciones:

Servir interfaz web.
Manejar solicitudes API.
Comunicación con frontend.
Sensor Manager

Funciones:

Lectura de sensores.
Conversión de valores.
Filtrado de mediciones.
pH Controller

Funciones:

Lectura del sensor pH.
Calibración.
Control de bombas pH+ y pH-.
EC Controller

Funciones:

Medición EC.
Control de nutrientes.
Ajuste automático.
9. Automatización

El sistema deberá permitir configurar:

pH

Ejemplo:
Objetivo:
5.8

Rango permitido:
5.6 - 6.0
Si:

pH > 6.0

Acción:

Activar bomba PH-

Si:

pH < 5.6

Acción:

Activar bomba PH+
10. Seguridad futura

Implementar:

Usuario y contraseña.
Acceso privado.
Actualización OTA.
Protección de configuración.
11. Filosofía de desarrollo

Cada funcionalidad debe:

Ser programada.
Ser probada.
Funcionar correctamente.
Tener un commit en Git.