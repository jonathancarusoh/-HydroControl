#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

WebServer server(80);

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

    String contentType = "text/plain";

    if (path.endsWith(".html"))
        contentType = "text/html";

    else if (path.endsWith(".css"))
        contentType = "text/css";

    else if (path.endsWith(".js"))
        contentType = "application/javascript";

    else if (path.endsWith(".png"))
        contentType = "image/png";

    else if (path.endsWith(".jpg"))
        contentType = "image/jpeg";

    server.streamFile(file, contentType);

    file.close();
}
// ===============================
// CONFIGURACIÓN WIFI
// ===============================

const char* WIFI_SSID = "Personal-014";
const char* WIFI_PASSWORD = "NyHJR88zGp";



void setup()
{
    Serial.begin(115200);

    Serial.println();
    Serial.println("=================================");
    Serial.println("      HYDRO CONTROL PRO");
    Serial.println("=================================");
    Serial.println("Iniciando sistema...");
    Serial.println();

    Serial.print("Conectando a ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

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

server.onNotFound(handleFileRequest);

server.begin();

Serial.println("Servidor Web iniciado");
}

void loop()
{
    server.handleClient();
}
