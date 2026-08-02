#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Preferences.h>

// ===============================
// CONFIGURACIÓN WIFI
// ===============================

const char* WIFI_SSID = "Personal-014";
const char* WIFI_PASSWORD = "NyHJR88zGp";

// Servidor HTTP en el puerto 80
WebServer server(80);

Preferences preferences;

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

// Datos simulados hasta conectar los sensores reales
float currentPh = 5.82f;
float currentEc = 1.45f;
float waterTemperature = 18.5f;
float airHumidity = 61.0f;

// ===============================
// API REST
// ===============================

void handleApiStatus()
{
    String json = "{";

    json += "\"online\":true,";
    json += "\"ph\":" + String(currentPh, 2) + ",";
    json += "\"ec\":" + String(currentEc, 2) + ",";
    json += "\"waterTemp\":" + String(waterTemperature, 1) + ",";
    json += "\"humidity\":" + String(airHumidity, 0) + ",";
    json += "\"wifiRssi\":" + String(WiFi.RSSI());

    json += "}";

    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
}

// ===============================
// ARCHIVOS DE LITTLEFS
// ===============================

String getContentType(const String& path)
{
    if (path.endsWith(".html")) return "text/html";
    if (path.endsWith(".css")) return "text/css";
    if (path.endsWith(".js")) return "application/javascript";
    if (path.endsWith(".json")) return "application/json";
    if (path.endsWith(".png")) return "image/png";
    if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
    if (path.endsWith(".svg")) return "image/svg+xml";
    if (path.endsWith(".ico")) return "image/x-icon";

    return "text/plain";
}

void handleFileRequest()
{
    String path = server.uri();

    if (path == "/")
    {
        path = "/index.html";
    }

    if (!LittleFS.exists(path))
    {
        server.send(404, "text/plain", "Archivo no encontrado");
        return;
    }

    File file = LittleFS.open(path, "r");

    if (!file)
    {
        server.send(500, "text/plain", "No se pudo abrir el archivo");
        return;
    }

    server.streamFile(file, getContentType(path));
    file.close();
}

// ===============================
// INICIO
// ===============================
void loadConfig()
{
    preferences.begin("hydrocontrol", true);

    config.targetPh =
        preferences.getFloat("targetPh", 5.80f);

    config.phTolerance =
        preferences.getFloat("tolerance", 0.10f);

    config.doseDurationMs =
        preferences.getUInt("doseMs", 500);

    config.doseIntervalMinutes =
        preferences.getUInt("intervalMin", 4);

    config.maxConsecutiveDoses =
        preferences.getUChar("maxDoses", 3);

    config.automaticMode =
        preferences.getBool("autoMode", true);

    preferences.end();
}

void saveConfig()
{
    preferences.begin("hydrocontrol", false);

    preferences.putFloat("targetPh", config.targetPh);
    preferences.putFloat("tolerance", config.phTolerance);
    preferences.putUInt("doseMs", config.doseDurationMs);
    preferences.putUInt("intervalMin", config.doseIntervalMinutes);
    preferences.putUChar("maxDoses", config.maxConsecutiveDoses);
    preferences.putBool("autoMode", config.automaticMode);

    preferences.end();
}

void handleGetConfig()
{
    String json = "{";

    json += "\"targetPh\":" + String(config.targetPh, 2) + ",";
    json += "\"tolerance\":" + String(config.phTolerance, 2) + ",";
    json += "\"doseSeconds\":" + String(config.doseDurationMs / 1000.0f, 1) + ",";
    json += "\"intervalMinutes\":" + String(config.doseIntervalMinutes) + ",";
    json += "\"maxDoses\":" + String(config.maxConsecutiveDoses) + ",";
    json += "\"automaticMode\":";
    json += config.automaticMode ? "true" : "false";

    json += "}";

    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
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
            "{\"success\":false,\"message\":\"Faltan datos\"}"
        );

        return;
    }

    float targetPh = server.arg("targetPh").toFloat();
    float tolerance = server.arg("tolerance").toFloat();
    float doseSeconds = server.arg("doseSeconds").toFloat();
    int intervalMinutes = server.arg("intervalMinutes").toInt();
    int maxDoses = server.arg("maxDoses").toInt();
    bool automaticMode =
        server.arg("automaticMode") == "true";

    if (
        targetPh < 4.0f || targetPh > 8.0f ||
        tolerance < 0.01f || tolerance > 1.0f ||
        doseSeconds < 0.1f || doseSeconds > 30.0f ||
        intervalMinutes < 1 || intervalMinutes > 120 ||
        maxDoses < 1 || maxDoses > 10
    )
    {
        server.send(
            400,
            "application/json",
            "{\"success\":false,\"message\":\"Valores fuera de rango\"}"
        );

        return;
    }

    config.targetPh = targetPh;
    config.phTolerance = tolerance;

    // La web trabaja en segundos.
    // El ESP32 guarda y controla la bomba en milisegundos.
    config.doseDurationMs =
        static_cast<uint32_t>(doseSeconds * 1000.0f);

    config.doseIntervalMinutes =
        static_cast<uint32_t>(intervalMinutes);

    config.maxConsecutiveDoses =
        static_cast<uint8_t>(maxDoses);

    config.automaticMode = automaticMode;

    saveConfig();

    server.send(
        200,
        "application/json",
        "{\"success\":true,\"message\":\"Configuración guardada\"}"
    );
}

void setup()
{
    Serial.begin(115200);
loadConfig();

Serial.println("Configuración cargada:");
Serial.printf("Objetivo pH: %.2f\n", config.targetPh);
Serial.printf("Tolerancia: %.2f\n", config.phTolerance);
Serial.printf("Duración dosis: %lu ms\n", config.doseDurationMs);
Serial.printf(
    "Intervalo: %lu minutos\n",
    config.doseIntervalMinutes
);
Serial.printf(
    "Máximo de dosis: %u\n",
    config.maxConsecutiveDoses
);

    Serial.println();
    Serial.println("=================================");
    Serial.println("      HYDRO CONTROL PRO");
    Serial.println("=================================");
    Serial.println("Iniciando sistema...");
    Serial.println();

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Conectando a ");
    Serial.println(WIFI_SSID);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("---------------------------------");
    Serial.println("WiFi conectado correctamente");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.println("---------------------------------");

    if (!LittleFS.begin())
    {
        Serial.println("Error al iniciar LittleFS");

        while (true)
        {
            delay(1000);
        }
    }

    Serial.println("LittleFS iniciado correctamente");

    // La API debe registrarse antes de onNotFound
    server.on("/api/status", HTTP_GET, handleApiStatus);
server.on("/api/config", HTTP_GET, handleGetConfig);
server.on("/api/config", HTTP_POST, handleSaveConfig);
    // Cualquier otra ruta se busca en LittleFS
    server.onNotFound(handleFileRequest);

    server.begin();

    Serial.println("Servidor web iniciado");
}

void loop()
{
    server.handleClient();
}