#include "web_server.h"
#include "api.h"
#include "app_state.h"
#include "clock_manager.h"
#include "event_logger.h"
#include "ph.h"
#include "profile_manager.h"
#include "system_manager.h"
#include "wifi_manager.h"
#include "wifi_portal.h"
#include "utils.h"
#include <LittleFS.h>

// ======================================================
// SERVIDOR WEB Y RUTAS
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
    String path = server.uri();

    // Las APIs siempre deben responder JSON. Así el frontend
    // nunca intenta interpretar "Archivo no encontrado" o una
    // página HTML como si fueran JSON.
    if (path.startsWith("/api/"))
    {
        String json = "{";
        json += "\"success\":false,";
        json += "\"message\":\"Ruta API no encontrada o método incorrecto.\",";
        json += "\"path\":\"";
        json += escapeJson(path);
        json += "\"";
        json += "}";

        server.sendHeader(
            "Cache-Control",
            "no-store"
        );

        server.send(
            404,
            "application/json; charset=utf-8",
            json
        );

        return;
    }

    if (wifiSetupMode)
    {
        handleWifiSetupPage();
        return;
    }

    if (path == "/")
    {
        path = "/index.html";
    }

    if (!LittleFS.exists(path))
    {
        // En modo local se usa index.html como página de
        // respaldo para rutas visuales, pero nunca para APIs.
        if (localAccessMode && LittleFS.exists("/index.html"))
        {
            path = "/index.html";
        }
        else
        {
            server.send(
                404,
                "text/plain; charset=utf-8",
                "Archivo no encontrado"
            );

            return;
        }
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

    server.sendHeader(
        "Cache-Control",
        path.endsWith(".html") ||
        path.endsWith(".js") ||
        path.endsWith(".css")
            ? "no-cache"
            : "public, max-age=86400"
    );

    server.streamFile(
        file,
        getContentType(path)
    );

    file.close();
}

// ======================================================
// API DE COMPATIBILIDAD Y VERSIÓN
// ======================================================

void registerServerRoutes()
{
    server.on(
        "/api/status",
        HTTP_GET,
        handleApiStatus
    );

    server.on(
        "/api/runtime",
        HTTP_GET,
        handleGetRuntimeInfo
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
        "/api/clock/status",
        HTTP_GET,
        handleGetClockStatus
    );

    // Alias compatible con la primera implementación del reloj.
    server.on(
        "/api/clock",
        HTTP_GET,
        handleGetClockStatus
    );

    server.on(
        "/api/clock/set",
        HTTP_POST,
        handleSetClock
    );

    server.on(
        "/api/light/schedule",
        HTTP_POST,
        handleSaveLightSchedule
    );

    server.on(
        "/api/events",
        HTTP_GET,
        handleGetEvents
    );

    // Alias de compatibilidad para clientes que usen /list.
    server.on(
        "/api/events/list",
        HTTP_GET,
        handleGetEvents
    );

    server.on(
        "/api/events/clear",
        HTTP_POST,
        handleClearEvents
    );

    server.on(
        "/api/profiles",
        HTTP_GET,
        handleGetProfiles
    );

    server.on(
        "/api/profiles/save",
        HTTP_POST,
        handleSaveProfile
    );

    server.on(
        "/api/profiles/apply",
        HTTP_POST,
        handleApplyProfile
    );

    server.on(
        "/api/profiles/delete",
        HTTP_POST,
        handleDeleteProfile
    );

    server.on(
        "/api/system/status",
        HTTP_GET,
        handleGetSystemStatus
    );

    server.on(
        "/api/system/restart",
        HTTP_POST,
        handleRestartSystem
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
        "/api/wifi/disconnect",
        HTTP_POST,
        handleDisconnectWifi
    );

    server.on(
        "/api/wifi/connect-saved",
        HTTP_POST,
        handleConnectSavedWifi
    );

    server.on(
        "/api/wifi/delete-password",
        HTTP_POST,
        handleDeleteSavedWifiPassword
    );

    server.on(
        "/save-wifi",
        HTTP_POST,
        handleSaveWifiForm
    );

    server.on(
        "/use-local-mode",
        HTTP_POST,
        handleUseLocalMode
    );

    server.onNotFound(
        handleFileRequest
    );
}
