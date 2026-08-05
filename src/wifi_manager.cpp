#include "wifi_manager.h"
#include "app_state.h"
#include "event_logger.h"
#include "utils.h"
#include <WiFi.h>
#include <Preferences.h>
#include <ESPmDNS.h>
namespace
{
    Preferences wifiPreferences;
    wl_status_t lastObservedWifiStatus = WL_IDLE_STATUS;
    unsigned long lastWifiMonitorAt = 0;
    const unsigned long WIFI_MONITOR_INTERVAL_MS = 2000;
}

// ======================================================
// GESTIÓN WIFI
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

void clearWifiPassword()
{
    // Conserva el nombre de la red y elimina solamente
    // la contraseña almacenada en la memoria NVS.
    wifiPreferences.begin("wifi", false);
    wifiPreferences.putString("password", "");
    wifiPreferences.end();
}

bool loadLocalAccessMode()
{
    wifiPreferences.begin("wifi", true);

    bool enabled =
        wifiPreferences.getBool("localMode", false);

    wifiPreferences.end();

    return enabled;
}

void saveLocalAccessMode(bool enabled)
{
    wifiPreferences.begin("wifi", false);

    wifiPreferences.putBool(
        "localMode",
        enabled
    );

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

    // El hostname debe configurarse antes de iniciar
    // la interfaz WiFi en modo estación.
    WiFi.setHostname(MDNS_HOSTNAME);
    WiFi.mode(WIFI_STA);

    // Evita pausas causadas por el ahorro de energía WiFi.
    WiFi.setSleep(false);

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

        logEvent(
            "wifi",
            "No se pudo conectar al WiFi",
            String("Red guardada: ") + ssid
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

    logEvent(
        "wifi",
        "Conectado al WiFi",
        String("Red ") + ssid +
            " · IP " + WiFi.localIP().toString()
    );

    return true;
}

// ======================================================
// PORTAL DE CONFIGURACIÓN WIFI
// ======================================================

bool testWifiCredentials(
    const String& ssid,
    const String& password,
    String& errorMessage
)
{
    Serial.println();
    Serial.print("Probando conexión a: ");
    Serial.println(ssid);

    // Mantiene HydroControl-Setup activo mientras prueba
    // la conexión al router.
    WiFi.mode(WIFI_AP_STA);
    WiFi.setAutoReconnect(false);

    // Desconecta solamente la parte estación.
    // No elimina ni apaga el punto de acceso.
    WiFi.disconnect(false, false);

    delay(300);

    WiFi.begin(
        ssid.c_str(),
        password.c_str()
    );

    const unsigned long timeoutMs = 20000;
    const unsigned long startTime = millis();

    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - startTime < timeoutMs
    )
    {
        wl_status_t status = WiFi.status();

        if (
            status == WL_CONNECT_FAILED ||
            status == WL_NO_SSID_AVAIL
        )
        {
            break;
        }

        delay(250);
    }

    wl_status_t finalStatus = WiFi.status();

    if (finalStatus == WL_CONNECTED)
    {
        Serial.println(
            "Conexión WiFi comprobada correctamente."
        );

        Serial.print("Nueva IP: ");
        Serial.println(WiFi.localIP());

        return true;
    }

    switch (finalStatus)
    {
        case WL_NO_SSID_AVAIL:
            errorMessage =
                "La red seleccionada ya no está disponible. "
                "Volvé a buscar las redes cercanas.";
            break;

        case WL_CONNECT_FAILED:
            errorMessage =
                "Contraseña incorrecta. "
                "Escribila nuevamente.";
            break;

        case WL_CONNECTION_LOST:
            errorMessage =
                "Se perdió la conexión durante la prueba. "
                "Intentá nuevamente.";
            break;

        default:
            errorMessage =
                "No se pudo conectar. Revisá la contraseña "
                "e intentá nuevamente.";
            break;
    }

    Serial.print("Falló la conexión: ");
    Serial.println(errorMessage);

    // Mantiene el portal HydroControl-Setup funcionando.
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_AP_STA);

    delay(200);

    return false;
}

void handleSaveWifiForm()
{
    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    if (!server.hasArg("ssid"))
    {
        server.send(
            400,
            "application/json",
            "{\"success\":false,"
            "\"message\":\"Primero seleccioná una red WiFi.\"}"
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
            "\"message\":\"La red seleccionada no es válida.\"}"
        );

        return;
    }

    String connectionError;

    bool connected = testWifiCredentials(
        ssid,
        password,
        connectionError
    );

    if (!connected)
    {
        String json = "{";

        json += "\"success\":false,";
        json += "\"message\":\"";
        json += escapeJson(connectionError);
        json += "\"";

        json += "}";

        server.send(
            422,
            "application/json",
            json
        );

        return;
    }

    // Al conectarse a un router, sale del modo local.
    saveLocalAccessMode(false);

    // Guarda las credenciales solamente después de comprobar
    // que el ESP32 logró conectarse correctamente.
    saveWifiCredentials(
        ssid,
        password
    );

    logEvent(
        "wifi",
        "Credenciales WiFi guardadas",
        String("Red: ") + ssid
    );

    String json = "{";

    json += "\"success\":true,";
    json += "\"message\":\"Conexión WiFi exitosa.\",";

    json += "\"ssid\":\"";
    json += escapeJson(ssid);
    json += "\",";

    json += "\"ip\":\"";
    json += WiFi.localIP().toString();
    json += "\",";

    json += "\"localUrl\":\"http://hydrocontrol.local\"";

    json += "}";

    server.send(
        200,
        "application/json",
        json
    );

    // El reinicio se hace desde loop() para darle tiempo al
    // navegador a mostrar el cartel de conexión exitosa.
    restartPending = true;
    restartRequestedAt = millis();
}

void handleUseLocalMode()
{
    saveLocalAccessMode(true);

    logEvent(
        "wifi",
        "Modo local solicitado",
        "El ESP32 reiniciará como punto de acceso HydroControl."
    );

    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    server.send(
        200,
        "application/json",
        "{\"success\":true,"
        "\"message\":\"Modo local activado. Reiniciando...\"}"
    );

    restartPending = true;
    restartRequestedAt = millis();
}

void startLocalAccessMode()
{
    wifiSetupMode = false;
    localAccessMode = true;

    Serial.println();
    Serial.println(
        "Iniciando modo local sin router..."
    );

    WiFi.disconnect(true);
    delay(200);

    // AP mantiene la conexión directa con el celular y
    // STA permite buscar redes desde la sección WiFi.
    WiFi.mode(WIFI_AP_STA);

    bool accessPointStarted = WiFi.softAP(
        LOCAL_WIFI_NAME,
        LOCAL_WIFI_PASSWORD
    );

    if (!accessPointStarted)
    {
        Serial.println(
            "Error al crear la red local HydroControl."
        );

        return;
    }

    IPAddress localIp = WiFi.softAPIP();

    dnsServer.start(
        DNS_PORT,
        "*",
        localIp
    );

    Serial.println(
        "Modo local iniciado correctamente."
    );

    Serial.print("Red: ");
    Serial.println(LOCAL_WIFI_NAME);

    Serial.print("Contraseña: ");
    Serial.println(LOCAL_WIFI_PASSWORD);

    Serial.print("Aplicación: http://");
    Serial.println(localIp);

    logEvent(
        "wifi",
        "Modo local iniciado",
        String("Red ") + LOCAL_WIFI_NAME +
            " · IP " + localIp.toString()
    );
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

    WiFi.mode(WIFI_AP_STA);

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

    logEvent(
        "wifi",
        "Portal de configuración iniciado",
        String("Red ") + SETUP_WIFI_NAME +
            " · IP " + setupIp.toString()
    );
}


// ======================================================
// PERFILES DE CULTIVO
// ======================================================

void monitorRouterWifiState()
{
    if (wifiSetupMode || localAccessMode)
    {
        return;
    }

    if (
        millis() - lastWifiMonitorAt <
        WIFI_MONITOR_INTERVAL_MS
    )
    {
        return;
    }

    lastWifiMonitorAt = millis();
    wl_status_t currentStatus = WiFi.status();

    if (currentStatus == lastObservedWifiStatus)
    {
        return;
    }

    bool wasConnected =
        lastObservedWifiStatus == WL_CONNECTED;

    bool isConnected =
        currentStatus == WL_CONNECTED;

    if (wasConnected && !isConnected)
    {
        logEvent(
            "wifi",
            "WiFi desconectado",
            String("Se perdió la conexión con ") +
                loadWifiSsid() + "."
        );
    }
    else if (!wasConnected && isConnected)
    {
        logEvent(
            "wifi",
            "WiFi recuperado",
            String("Conectado a ") +
                WiFi.SSID() +
                " · IP " +
                WiFi.localIP().toString()
        );
    }

    lastObservedWifiStatus = currentStatus;
}

void handleGetWifiStatus()
{
    String savedSsid = loadWifiSsid();
    String savedPassword = loadWifiPassword();

    String json = "{";

    json += "\"connected\":";
    json += WiFi.status() == WL_CONNECTED
        ? "true"
        : "false";

    json += ",\"setupMode\":";
    json += wifiSetupMode
        ? "true"
        : "false";

    json += ",\"localMode\":";
    json += localAccessMode
        ? "true"
        : "false";

    json += ",\"hasSavedCredentials\":";
    json += savedSsid.isEmpty()
        ? "false"
        : "true";

    json += ",\"hasSavedPassword\":";
    json += savedPassword.isEmpty()
        ? "false"
        : "true";

    json += ",\"savedSsid\":\"";
    json += escapeJson(savedSsid);
    json += "\"";

    json += ",\"ssid\":\"";

    if (localAccessMode)
    {
        json += escapeJson(LOCAL_WIFI_NAME);
    }
    else if (WiFi.status() == WL_CONNECTED)
    {
        json += escapeJson(WiFi.SSID());
    }
    else
    {
        json += escapeJson(savedSsid);
    }

    json += "\"";

    json += ",\"ip\":\"";

    if (wifiSetupMode || localAccessMode)
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
    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    if (!server.hasArg("ssid"))
    {
        server.send(
            400,
            "application/json; charset=utf-8",
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
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"La red seleccionada no es válida\"}"
        );

        return;
    }

    // Cuando la app está abierta desde el punto de acceso local
    // o desde HydroControl-Setup, podemos comprobar la clave sin
    // cortar la conexión entre el celular y el ESP32.
    if (localAccessMode || wifiSetupMode)
    {
        String connectionError;

        bool connected = testWifiCredentials(
            ssid,
            password,
            connectionError
        );

        if (!connected)
        {
            String json = "{";
            json += "\"success\":false,";
            json += "\"message\":\"";
            json += escapeJson(connectionError);
            json += "\"";
            json += "}";

            server.send(
                422,
                "application/json; charset=utf-8",
                json
            );

            return;
        }
    }

    saveLocalAccessMode(false);
    saveWifiCredentials(ssid, password);

    logEvent(
        "wifi",
        "Nueva red WiFi guardada",
        String("Red: ") + ssid
    );

    String json = "{";
    json += "\"success\":true,";
    json += "\"message\":\"WiFi comprobado y guardado. Reiniciando...\",";
    json += "\"ssid\":\"";
    json += escapeJson(ssid);
    json += "\"";
    json += "}";

    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );

    // El reinicio se programa desde loop() para garantizar
    // que el navegador reciba la respuesta completa.
    restartPending = true;
    restartRequestedAt = millis();
}

void handleResetWifi()
{
    String removedSsid = loadWifiSsid();
    clearWifiCredentials();

    logEvent(
        "wifi",
        "Credenciales WiFi eliminadas",
        removedSsid.isEmpty()
            ? "No había una red guardada."
            : String("Red eliminada: ") + removedSsid
    );

    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    server.send(
        200,
        "application/json; charset=utf-8",
        "{\"success\":true,"
        "\"message\":\"Credenciales eliminadas. Reiniciando...\"}"
    );

    // Evita bloquear el servidor con delay() justo después
    // de responder la petición.
    restartPending = true;
    restartRequestedAt = millis();
}

void handleDisconnectWifi()
{
    String savedSsid = loadWifiSsid();

    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    if (savedSsid.isEmpty())
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"No hay una red WiFi guardada.\"}"
        );

        return;
    }

    // Conserva el SSID y la contraseña, pero evita que
    // el ESP32 se conecte automáticamente al router.
    // En el siguiente arranque levantará la red local.
    saveLocalAccessMode(true);

    logEvent(
        "wifi",
        "Desconexión del router solicitada",
        String("Se conserva la red: ") + savedSsid
    );

    String json = "{";
    json += "\"success\":true,";
    json += "\"message\":\"Desconectando del router. La red guardada se conservará.\",";
    json += "\"localNetwork\":\"";
    json += escapeJson(LOCAL_WIFI_NAME);
    json += "\",";
    json += "\"localUrl\":\"http://192.168.4.1\"";
    json += "}";

    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );

    Serial.println(
        "Solicitud de desconexión recibida. "
        "El ESP32 reiniciará en modo local."
    );

    // El reinicio se hace desde loop(), luego de que el
    // navegador haya recibido el JSON completo.
    restartPending = true;
    restartRequestedAt = millis();
}

void handleConnectSavedWifi()
{
    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    String requestedSsid = server.arg("ssid");
    String savedSsid = loadWifiSsid();
    String savedPassword = loadWifiPassword();

    requestedSsid.trim();

    if (savedSsid.isEmpty())
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"No hay credenciales WiFi guardadas.\"}"
        );

        return;
    }

    if (requestedSsid.isEmpty() || requestedSsid != savedSsid)
    {
        server.send(
            409,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"La red seleccionada no coincide con la red guardada.\"}"
        );

        return;
    }

    if (savedPassword.isEmpty())
    {
        server.send(
            409,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"La contraseña guardada fue eliminada. Ingresá una nueva contraseña.\"}"
        );

        return;
    }

    if (
        WiFi.status() == WL_CONNECTED &&
        !localAccessMode &&
        !wifiSetupMode &&
        WiFi.SSID() == savedSsid
    )
    {
        String json = "{";
        json += "\"success\":true,";
        json += "\"alreadyConnected\":true,";
        json += "\"message\":\"HydroControl ya está conectado a esta red.\",";
        json += "\"ssid\":\"";
        json += escapeJson(savedSsid);
        json += "\"";
        json += "}";

        server.send(
            200,
            "application/json; charset=utf-8",
            json
        );

        return;
    }

    // En modo local el celular continúa conectado al AP del ESP32,
    // por eso podemos probar la contraseña almacenada y devolver un
    // mensaje claro sin cerrar la página si la red no responde.
    if (localAccessMode || wifiSetupMode)
    {
        String connectionError;

        bool connected = testWifiCredentials(
            savedSsid,
            savedPassword,
            connectionError
        );

        if (!connected)
        {
            String json = "{";
            json += "\"success\":false,";
            json += "\"message\":\"";
            json += escapeJson(connectionError);
            json += "\"";
            json += "}";

            server.send(
                422,
                "application/json; charset=utf-8",
                json
            );

            return;
        }
    }

    saveLocalAccessMode(false);

    logEvent(
        "wifi",
        "Conexión a red guardada solicitada",
        savedSsid
    );

    String json = "{";
    json += "\"success\":true,";
    json += "\"alreadyConnected\":false,";
    json += "\"message\":\"Conexión comprobada. Reiniciando HydroControl...\",";
    json += "\"ssid\":\"";
    json += escapeJson(savedSsid);
    json += "\"";
    json += "}";

    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );

    Serial.print(
        "Conexión solicitada usando las credenciales guardadas de: "
    );
    Serial.println(savedSsid);

    restartPending = true;
    restartRequestedAt = millis();
}

void handleDeleteSavedWifiPassword()
{
    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    String savedSsid = loadWifiSsid();
    String savedPassword = loadWifiPassword();

    if (savedSsid.isEmpty())
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"No hay una red WiFi guardada.\"}"
        );

        return;
    }

    if (savedPassword.isEmpty())
    {
        String json = "{";
        json += "\"success\":true,";
        json += "\"alreadyDeleted\":true,";
        json += "\"message\":\"La contraseña ya estaba eliminada.\",";
        json += "\"savedSsid\":\"";
        json += escapeJson(savedSsid);
        json += "\"";
        json += "}";

        server.send(
            200,
            "application/json; charset=utf-8",
            json
        );

        return;
    }

    clearWifiPassword();

    logEvent(
        "wifi",
        "Contraseña WiFi eliminada",
        String("SSID conservado: ") + savedSsid
    );

    String json = "{";
    json += "\"success\":true,";
    json += "\"alreadyDeleted\":false,";
    json += "\"message\":\"Contraseña guardada eliminada. El nombre de la red se conserva.\",";
    json += "\"savedSsid\":\"";
    json += escapeJson(savedSsid);
    json += "\"";
    json += "}";

    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );

    Serial.print(
        "Contraseña WiFi eliminada. SSID conservado: "
    );
    Serial.println(savedSsid);
}

// ======================================================
// SERVIDOR DE ARCHIVOS LITTLEFS
// ======================================================

bool startMdns()
{
    if (wifiSetupMode || localAccessMode)
    {
        return false;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println(
            "No se puede iniciar mDNS: WiFi desconectado."
        );

        return false;
    }

    if (!MDNS.begin(MDNS_HOSTNAME))
    {
        Serial.println(
            "No se pudo iniciar el servicio mDNS."
        );

        return false;
    }

    MDNS.addService(
        "http",
        "tcp",
        80
    );

    Serial.println(
        "Servicio mDNS iniciado correctamente."
    );

    Serial.print(
        "Acceso local: http://"
    );

    Serial.print(
        MDNS_HOSTNAME
    );

    Serial.println(
        ".local"
    );

    return true;
}
// ======================================================
// SETUP
// ======================================================
