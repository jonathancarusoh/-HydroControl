#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <DNSServer.h>

// ======================================================
// SERVIDOR Y MEMORIA
// ======================================================

WebServer server(80);
DNSServer dnsServer;

Preferences hydroPreferences;
Preferences wifiPreferences;

// ======================================================
// CONFIGURACIÓN DEL PORTAL WIFI
// ======================================================

const char* SETUP_WIFI_NAME = "HydroControl-Setup";
const char* SETUP_WIFI_PASSWORD = "hydrocontrol";

const byte DNS_PORT = 53;

bool wifiSetupMode = false;

// ======================================================
// CONFIGURACIÓN DE HYDROCONTROL
// ======================================================

struct HydroConfig
{
    float targetPh = 5.80f;
    float phTolerance = 0.10f;

    uint32_t doseDurationMs = 500;
    uint32_t doseIntervalMinutes = 4;

    uint8_t maxConsecutiveDoses = 3;

    bool automaticMode = true;
};

HydroConfig config;

// ======================================================
// DATOS SIMULADOS
// Se reemplazarán después por sensores reales
// ======================================================

float currentPh = 5.82f;
float currentEc = 1.45f;
float waterTemperature = 18.5f;
float airHumidity = 61.0f;

// ======================================================
// UTILIDADES
// ======================================================

String escapeJson(const String& text)
{
    String result;
    result.reserve(text.length() + 10);

    for (size_t i = 0; i < text.length(); i++)
    {
        char character = text.charAt(i);

        switch (character)
        {
            case '"':
                result += "\\\"";
                break;

            case '\\':
                result += "\\\\";
                break;

            case '\n':
                result += "\\n";
                break;

            case '\r':
                result += "\\r";
                break;

            case '\t':
                result += "\\t";
                break;

            default:
                result += character;
                break;
        }
    }

    return result;
}

// ======================================================
// CONFIGURACIÓN PERSISTENTE DE HYDROCONTROL
// ======================================================

void loadConfig()
{
    hydroPreferences.begin("hydrocontrol", true);

    config.targetPh =
        hydroPreferences.getFloat("targetPh", 5.80f);

    config.phTolerance =
        hydroPreferences.getFloat("tolerance", 0.10f);

    config.doseDurationMs =
        hydroPreferences.getUInt("doseMs", 500);

    config.doseIntervalMinutes =
        hydroPreferences.getUInt("intervalMin", 4);

    config.maxConsecutiveDoses =
        hydroPreferences.getUChar("maxDoses", 3);

    config.automaticMode =
        hydroPreferences.getBool("autoMode", true);

    hydroPreferences.end();
}

void saveConfig()
{
    hydroPreferences.begin("hydrocontrol", false);

    hydroPreferences.putFloat(
        "targetPh",
        config.targetPh
    );

    hydroPreferences.putFloat(
        "tolerance",
        config.phTolerance
    );

    hydroPreferences.putUInt(
        "doseMs",
        config.doseDurationMs
    );

    hydroPreferences.putUInt(
        "intervalMin",
        config.doseIntervalMinutes
    );

    hydroPreferences.putUChar(
        "maxDoses",
        config.maxConsecutiveDoses
    );

    hydroPreferences.putBool(
        "autoMode",
        config.automaticMode
    );

    hydroPreferences.end();
}

// ======================================================
// CREDENCIALES WIFI
// ======================================================

String loadWifiSsid()
{
    wifiPreferences.begin("wifi", true);

    String ssid =
        wifiPreferences.getString("ssid", "");

    wifiPreferences.end();

    return ssid;
}

String loadWifiPassword()
{
    wifiPreferences.begin("wifi", true);

    String password =
        wifiPreferences.getString("password", "");

    wifiPreferences.end();

    return password;
}

void saveWifiCredentials(
    const String& ssid,
    const String& password
)
{
    wifiPreferences.begin("wifi", false);

    wifiPreferences.putString("ssid", ssid);
    wifiPreferences.putString("password", password);

    wifiPreferences.end();
}

void clearWifiCredentials()
{
    wifiPreferences.begin("wifi", false);
    wifiPreferences.clear();
    wifiPreferences.end();
}

// ======================================================
// CONEXIÓN WIFI
// ======================================================

bool connectToSavedWifi()
{
    String ssid = loadWifiSsid();
    String password = loadWifiPassword();

    if (ssid.isEmpty())
    {
        Serial.println(
            "No existen credenciales WiFi guardadas."
        );

        return false;
    }

    Serial.println();
    Serial.print("Conectando a WiFi: ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);

    WiFi.begin(
        ssid.c_str(),
        password.c_str()
    );

    const unsigned long connectionTimeout = 15000;
    const unsigned long startTime = millis();

    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - startTime < connectionTimeout
    )
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println(
            "No fue posible conectarse al WiFi guardado."
        );

        WiFi.disconnect(true);
        delay(200);

        return false;
    }

    Serial.println(
        "WiFi conectado correctamente."
    );

    Serial.print("Dirección IP: ");
    Serial.println(WiFi.localIP());

    return true;
}

// ======================================================
// PORTAL DE CONFIGURACIÓN WIFI
// ======================================================

void handleWifiSetupPage()
{
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="es">

<head>
    <meta charset="UTF-8">

    <meta
        name="viewport"
        content="width=device-width, initial-scale=1.0">

    <title>Configurar HydroControl</title>

    <style>
        * {
            box-sizing: border-box;
        }

        body {
            margin: 0;
            min-height: 100vh;
            padding: 24px;
            display: flex;
            justify-content: center;
            align-items: center;
            background: #1a1d21;
            color: #ffffff;
            font-family: Arial, sans-serif;
        }

        .card {
            width: 100%;
            max-width: 430px;
            padding: 28px;
            background: #252a31;
            border-radius: 18px;
            box-shadow: 0 15px 40px rgba(0, 0, 0, .35);
        }

        h1 {
            margin-top: 0;
            font-size: 27px;
        }

        p {
            color: #c4c9d0;
            line-height: 1.5;
        }

        label {
            display: block;
            margin-top: 18px;
            margin-bottom: 7px;
            font-weight: bold;
        }

        input {
            width: 100%;
            padding: 13px;
            border: 1px solid #4a5059;
            border-radius: 9px;
            background: #1a1d21;
            color: white;
            font-size: 16px;
        }

        button {
            width: 100%;
            margin-top: 24px;
            padding: 14px;
            border: none;
            border-radius: 9px;
            background: #198754;
            color: white;
            font-size: 16px;
            font-weight: bold;
            cursor: pointer;
        }

        .info {
            margin-top: 20px;
            padding: 12px;
            border-radius: 9px;
            background: #323840;
            color: #d7dbe0;
            font-size: 14px;
        }
    </style>
</head>

<body>

    <div class="card">

        <h1>🌱 HydroControl</h1>

        <p>
            Configurá la red WiFi donde quedará conectado
            el controlador.
        </p>

        <form method="POST" action="/save-wifi">

            <label for="ssid">
                Nombre de la red WiFi
            </label>

            <input
                id="ssid"
                name="ssid"
                type="text"
                autocomplete="off"
                required>

            <label for="password">
                Contraseña
            </label>

            <input
                id="password"
                name="password"
                type="password">

            <button type="submit">
                Guardar y conectar
            </button>

        </form>

        <div class="info">
            HydroControl guardará esta red y se conectará
            automáticamente cada vez que reciba alimentación.
        </div>

    </div>

</body>
</html>
)rawliteral";

    server.send(
        200,
        "text/html; charset=utf-8",
        html
    );
}

void handleSaveWifiForm()
{
    if (!server.hasArg("ssid"))
    {
        server.send(
            400,
            "text/plain; charset=utf-8",
            "Falta el nombre de la red WiFi."
        );

        return;
    }

    String ssid = server.arg("ssid");
    String password = server.arg("password");

    ssid.trim();

    if (ssid.isEmpty())
    {
        server.send(
            400,
            "text/plain; charset=utf-8",
            "El nombre de la red está vacío."
        );

        return;
    }

    saveWifiCredentials(ssid, password);

    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="es">

<head>
    <meta charset="UTF-8">

    <meta
        name="viewport"
        content="width=device-width, initial-scale=1.0">

    <title>WiFi guardado</title>

    <style>
        body {
            margin: 0;
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            background: #1a1d21;
            color: white;
            font-family: Arial, sans-serif;
            text-align: center;
        }

        .card {
            margin: 20px;
            padding: 30px;
            max-width: 420px;
            background: #252a31;
            border-radius: 18px;
        }
    </style>
</head>

<body>

    <div class="card">

        <h2>WiFi guardado correctamente</h2>

        <p>
            HydroControl se reiniciará e intentará conectarse
            a la nueva red.
        </p>

    </div>

</body>
</html>
)rawliteral";

    server.send(
        200,
        "text/html; charset=utf-8",
        html
    );

    delay(1800);
    ESP.restart();
}

void startWifiSetupMode()
{
    wifiSetupMode = true;

    Serial.println();
    Serial.println(
        "Iniciando modo de configuración WiFi..."
    );

    WiFi.disconnect(true);
    delay(200);

    WiFi.mode(WIFI_AP);

    bool accessPointStarted = WiFi.softAP(
        SETUP_WIFI_NAME,
        SETUP_WIFI_PASSWORD
    );

    if (!accessPointStarted)
    {
        Serial.println(
            "Error al crear la red HydroControl-Setup."
        );

        return;
    }

    IPAddress setupIp = WiFi.softAPIP();

    dnsServer.start(
        DNS_PORT,
        "*",
        setupIp
    );

    Serial.println(
        "Red de configuración creada."
    );

    Serial.print("Nombre: ");
    Serial.println(SETUP_WIFI_NAME);

    Serial.print("Contraseña: ");
    Serial.println(SETUP_WIFI_PASSWORD);

    Serial.print("Dirección: http://");
    Serial.println(setupIp);
}

// ======================================================
// API DE ESTADO GENERAL
// ======================================================

void handleApiStatus()
{
    String json = "{";

    json += "\"online\":true,";
    json += "\"ph\":" + String(currentPh, 2) + ",";
    json += "\"ec\":" + String(currentEc, 2) + ",";
    json += "\"waterTemp\":" +
        String(waterTemperature, 1) + ",";

    json += "\"humidity\":" +
        String(airHumidity, 0) + ",";

    json += "\"wifiRssi\":" +
        String(WiFi.RSSI());

    json += "}";

    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    server.send(
        200,
        "application/json",
        json
    );
}

// ======================================================
// API DE CONFIGURACIÓN DE PH
// ======================================================

void handleGetConfig()
{
    String json = "{";

    json += "\"targetPh\":" +
        String(config.targetPh, 2) + ",";

    json += "\"tolerance\":" +
        String(config.phTolerance, 2) + ",";

    json += "\"doseSeconds\":" +
        String(
            config.doseDurationMs / 1000.0f,
            1
        ) + ",";

    json += "\"intervalMinutes\":" +
        String(config.doseIntervalMinutes) + ",";

    json += "\"maxDoses\":" +
        String(config.maxConsecutiveDoses) + ",";

    json += "\"automaticMode\":";
    json += config.automaticMode
        ? "true"
        : "false";

    json += "}";

    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    server.send(
        200,
        "application/json",
        json
    );
}

void handleSaveConfig()
{
    if (
        !server.hasArg("targetPh") ||
        !server.hasArg("tolerance") ||
        !server.hasArg("doseSeconds") ||
        !server.hasArg("intervalMinutes") ||
        !server.hasArg("maxDoses") ||
        !server.hasArg("automaticMode")
    )
    {
        server.send(
            400,
            "application/json",
            "{\"success\":false,"
            "\"message\":\"Faltan datos\"}"
        );

        return;
    }

    float targetPh =
        server.arg("targetPh").toFloat();

    float tolerance =
        server.arg("tolerance").toFloat();

    float doseSeconds =
        server.arg("doseSeconds").toFloat();

    int intervalMinutes =
        server.arg("intervalMinutes").toInt();

    int maxDoses =
        server.arg("maxDoses").toInt();

    bool automaticMode =
        server.arg("automaticMode") == "true";

    if (
        targetPh < 4.0f ||
        targetPh > 8.0f ||
        tolerance < 0.01f ||
        tolerance > 1.0f ||
        doseSeconds < 0.1f ||
        doseSeconds > 30.0f ||
        intervalMinutes < 1 ||
        intervalMinutes > 120 ||
        maxDoses < 1 ||
        maxDoses > 10
    )
    {
        server.send(
            400,
            "application/json",
            "{\"success\":false,"
            "\"message\":\"Valores fuera de rango\"}"
        );

        return;
    }

    config.targetPh = targetPh;
    config.phTolerance = tolerance;

    config.doseDurationMs =
        static_cast<uint32_t>(
            doseSeconds * 1000.0f
        );

    config.doseIntervalMinutes =
        static_cast<uint32_t>(
            intervalMinutes
        );

    config.maxConsecutiveDoses =
        static_cast<uint8_t>(
            maxDoses
        );

    config.automaticMode = automaticMode;

    saveConfig();

    server.send(
        200,
        "application/json",
        "{\"success\":true,"
        "\"message\":\"Configuración guardada\"}"
    );
}

// ======================================================
// API DE WIFI
// ======================================================

void handleGetWifiStatus()
{
    String savedSsid = loadWifiSsid();

    String json = "{";

    json += "\"connected\":";
    json += WiFi.status() == WL_CONNECTED
        ? "true"
        : "false";

    json += ",\"setupMode\":";
    json += wifiSetupMode
        ? "true"
        : "false";

    json += ",\"ssid\":\"";

    if (WiFi.status() == WL_CONNECTED)
    {
        json += escapeJson(WiFi.SSID());
    }
    else
    {
        json += escapeJson(savedSsid);
    }

    json += "\"";

    json += ",\"ip\":\"";

    if (wifiSetupMode)
    {
        json += WiFi.softAPIP().toString();
    }
    else if (WiFi.status() == WL_CONNECTED)
    {
        json += WiFi.localIP().toString();
    }

    json += "\"";

    json += ",\"rssi\":";

    if (WiFi.status() == WL_CONNECTED)
    {
        json += String(WiFi.RSSI());
    }
    else
    {
        json += "0";
    }

    json += "}";

    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    server.send(
        200,
        "application/json",
        json
    );
}

void handleScanWifi()
{
    int networkCount = WiFi.scanNetworks();

    String json = "[";

    for (int i = 0; i < networkCount; i++)
    {
        if (i > 0)
        {
            json += ",";
        }

        json += "{";

        json += "\"ssid\":\"";
        json += escapeJson(WiFi.SSID(i));
        json += "\",";

        json += "\"rssi\":";
        json += String(WiFi.RSSI(i));
        json += ",";

        json += "\"secure\":";
        json += WiFi.encryptionType(i) == WIFI_AUTH_OPEN
            ? "false"
            : "true";

        json += "}";
    }

    json += "]";

    WiFi.scanDelete();

    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    server.send(
        200,
        "application/json",
        json
    );
}

void handleSaveWifiApi()
{
    if (!server.hasArg("ssid"))
    {
        server.send(
            400,
            "application/json",
            "{\"success\":false,"
            "\"message\":\"Falta el nombre del WiFi\"}"
        );

        return;
    }

    String ssid = server.arg("ssid");
    String password = server.arg("password");

    ssid.trim();

    if (ssid.isEmpty())
    {
        server.send(
            400,
            "application/json",
            "{\"success\":false,"
            "\"message\":\"El nombre del WiFi está vacío\"}"
        );

        return;
    }

    saveWifiCredentials(ssid, password);

    server.send(
        200,
        "application/json",
        "{\"success\":true,"
        "\"message\":\"WiFi guardado. Reiniciando...\"}"
    );

    delay(1500);
    ESP.restart();
}

void handleResetWifi()
{
    clearWifiCredentials();

    server.send(
        200,
        "application/json",
        "{\"success\":true,"
        "\"message\":\"Credenciales eliminadas. Reiniciando...\"}"
    );

    delay(1500);
    ESP.restart();
}

// ======================================================
// SERVIDOR DE ARCHIVOS LITTLEFS
// ======================================================

String getContentType(const String& path)
{
    if (path.endsWith(".html"))
    {
        return "text/html";
    }

    if (path.endsWith(".css"))
    {
        return "text/css";
    }

    if (path.endsWith(".js"))
    {
        return "application/javascript";
    }

    if (path.endsWith(".json"))
    {
        return "application/json";
    }

    if (path.endsWith(".png"))
    {
        return "image/png";
    }

    if (
        path.endsWith(".jpg") ||
        path.endsWith(".jpeg")
    )
    {
        return "image/jpeg";
    }

    if (path.endsWith(".svg"))
    {
        return "image/svg+xml";
    }

    if (path.endsWith(".ico"))
    {
        return "image/x-icon";
    }

    return "text/plain";
}

void handleFileRequest()
{
    if (wifiSetupMode)
    {
        handleWifiSetupPage();
        return;
    }

    String path = server.uri();

    if (path == "/")
    {
        path = "/index.html";
    }

    if (!LittleFS.exists(path))
    {
        server.send(
            404,
            "text/plain; charset=utf-8",
            "Archivo no encontrado"
        );

        return;
    }

    File file = LittleFS.open(path, "r");

    if (!file)
    {
        server.send(
            500,
            "text/plain; charset=utf-8",
            "No se pudo abrir el archivo"
        );

        return;
    }

    server.streamFile(
        file,
        getContentType(path)
    );

    file.close();
}

// ======================================================
// REGISTRO DE RUTAS
// ======================================================

void registerServerRoutes()
{
    server.on(
        "/api/status",
        HTTP_GET,
        handleApiStatus
    );

    server.on(
        "/api/config",
        HTTP_GET,
        handleGetConfig
    );

    server.on(
        "/api/config",
        HTTP_POST,
        handleSaveConfig
    );

    server.on(
        "/api/wifi/status",
        HTTP_GET,
        handleGetWifiStatus
    );

    server.on(
        "/api/wifi/scan",
        HTTP_GET,
        handleScanWifi
    );

    server.on(
        "/api/wifi/save",
        HTTP_POST,
        handleSaveWifiApi
    );

    server.on(
        "/api/wifi/reset",
        HTTP_POST,
        handleResetWifi
    );

    server.on(
        "/save-wifi",
        HTTP_POST,
        handleSaveWifiForm
    );

    server.onNotFound(
        handleFileRequest
    );
}

// ======================================================
// SETUP
// ======================================================

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println(
        "================================="
    );

    Serial.println(
        "       HYDRO CONTROL PRO"
    );

    Serial.println(
        "================================="
    );

    loadConfig();

    Serial.println(
        "Configuración cargada:"
    );

    Serial.printf(
        "Objetivo pH: %.2f\n",
        config.targetPh
    );

    Serial.printf(
        "Tolerancia: %.2f\n",
        config.phTolerance
    );

    Serial.printf(
        "Duración dosis: %lu ms\n",
        static_cast<unsigned long>(
            config.doseDurationMs
        )
    );

    Serial.printf(
        "Intervalo: %lu minutos\n",
        static_cast<unsigned long>(
            config.doseIntervalMinutes
        )
    );

    Serial.printf(
        "Máximo de dosis: %u\n",
        config.maxConsecutiveDoses
    );

    if (!LittleFS.begin())
    {
        Serial.println(
            "Error al iniciar LittleFS."
        );

        while (true)
        {
            delay(1000);
        }
    }

    Serial.println(
        "LittleFS iniciado correctamente."
    );

    registerServerRoutes();

    if (!connectToSavedWifi())
    {
        startWifiSetupMode();
    }

    server.begin();

    Serial.println(
        "Servidor web iniciado."
    );

    if (wifiSetupMode)
    {
        Serial.println(
            "Conectate a HydroControl-Setup"
        );

        Serial.println(
            "Abrí: http://192.168.4.1"
        );
    }
    else
    {
        Serial.print(
            "Abrí HydroControl en: http://"
        );

        Serial.println(
            WiFi.localIP()
        );
    }
}

// ======================================================
// LOOP
// ======================================================

void loop()
{
    if (wifiSetupMode)
    {
        dnsServer.processNextRequest();
    }

    server.handleClient();

    delay(2);
}