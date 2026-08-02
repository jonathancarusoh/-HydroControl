#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

// ===============================
// CONFIGURACIÓN WIFI
// ===============================

const char* WIFI_SSID = "TU_RED_WIFI";
const char* WIFI_PASSWORD = "TU_CONTRASENA";

// Servidor HTTP en el puerto 80
WebServer server(80);

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

void setup()
{
    Serial.begin(115200);

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

    // Cualquier otra ruta se busca en LittleFS
    server.onNotFound(handleFileRequest);

    server.begin();

    Serial.println("Servidor web iniciado");
}

void loop()
{
    server.handleClient();
}