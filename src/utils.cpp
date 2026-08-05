#include "utils.h"
#include "app_state.h"
#include <esp_system.h>

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

String getResetReasonText()
{
    switch (esp_reset_reason())
    {
        case ESP_RST_POWERON:
            return "Encendido o alimentación conectada";

        case ESP_RST_EXT:
            return "Reinicio externo por pin";

        case ESP_RST_SW:
            return "Reinicio solicitado por software";

        case ESP_RST_PANIC:
            return "Error crítico del sistema";

        case ESP_RST_INT_WDT:
            return "Watchdog de interrupción";

        case ESP_RST_TASK_WDT:
            return "Watchdog de tarea";

        case ESP_RST_WDT:
            return "Watchdog del sistema";

        case ESP_RST_DEEPSLEEP:
            return "Salida de sueño profundo";

        case ESP_RST_BROWNOUT:
            return "Caída de tensión (brownout)";

        case ESP_RST_SDIO:
            return "Reinicio por SDIO";

        case ESP_RST_UNKNOWN:
        default:
            return "Motivo desconocido";
    }
}

String getSystemModeCode()
{
    if (wifiSetupMode)
    {
        return "setup";
    }

    if (localAccessMode)
    {
        return "local";
    }

    return "router";
}

String getSystemModeLabel()
{
    if (wifiSetupMode)
    {
        return "Configuración inicial";
    }

    if (localAccessMode)
    {
        return "Modo local";
    }

    return "Conectado al router";
}

// ======================================================
// CONFIGURACIÓN PERSISTENTE DE HYDROCONTROL
// ======================================================
