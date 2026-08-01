#include <Arduino.h>
#include <WiFi.h>

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
}

void loop()
{
    delay(1000);
}