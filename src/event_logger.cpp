#include "event_logger.h"
#include "app_state.h"
#include "clock_manager.h"
#include "utils.h"
#include <LittleFS.h>
#include <FS.h>
#include <esp_timer.h>
namespace
{
    const char* EVENT_LOG_PATH = "/events.log";
    const char* EVENT_LOG_OLD_PATH = "/events.old.log";
    const size_t EVENT_LOG_ROTATE_BYTES = 96 * 1024;
    const size_t EVENT_API_MAX_ITEMS = 80;
}

// ======================================================
// REGISTRO PERSISTENTE DE EVENTOS
// ======================================================

void rotateEventLogIfNeeded()
{
    if (!littleFsReady || !LittleFS.exists(EVENT_LOG_PATH))
    {
        return;
    }

    File eventFile = LittleFS.open(EVENT_LOG_PATH, "r");

    if (!eventFile)
    {
        return;
    }

    size_t currentSize = eventFile.size();
    eventFile.close();

    if (currentSize < EVENT_LOG_ROTATE_BYTES)
    {
        return;
    }

    if (LittleFS.exists(EVENT_LOG_OLD_PATH))
    {
        LittleFS.remove(EVENT_LOG_OLD_PATH);
    }

    LittleFS.rename(
        EVENT_LOG_PATH,
        EVENT_LOG_OLD_PATH
    );
}

void logEvent(
    const String& category,
    const String& title,
    const String& detail
)
{
    if (!littleFsReady)
    {
        return;
    }

    rotateEventLogIfNeeded();

    File eventFile = LittleFS.open(
        EVENT_LOG_PATH,
        FILE_APPEND
    );

    if (!eventFile)
    {
        Serial.println(
            "No se pudo abrir el registro de eventos."
        );
        return;
    }

    uint32_t epoch = getCurrentClockEpoch();

    uint64_t uptimeSeconds =
        static_cast<uint64_t>(esp_timer_get_time()) /
        1000000ULL;

    char uptimeBuffer[24];

    snprintf(
        uptimeBuffer,
        sizeof(uptimeBuffer),
        "%llu",
        static_cast<unsigned long long>(uptimeSeconds)
    );

    String line;
    line.reserve(240);

    line += "{";
    line += "\"epoch\":" + String(epoch) + ",";
    line += "\"uptimeSeconds\":";
    line += uptimeBuffer;
    line += ",\"category\":\"";
    line += escapeJson(category);
    line += "\",\"title\":\"";
    line += escapeJson(title);
    line += "\",\"detail\":\"";
    line += escapeJson(detail);
    line += "\"}";

    eventFile.println(line);
    eventFile.flush();
    eventFile.close();

    Serial.print("[EVENTO] ");
    Serial.print(title);

    if (!detail.isEmpty())
    {
        Serial.print(" — ");
        Serial.print(detail);
    }

    Serial.println();
}

void recordDosageEvent(
    const String& channel,
    uint32_t durationMs,
    bool automatic
)
{
    String detail = channel;
    detail += " durante ";
    detail += String(durationMs / 1000.0f, 2);
    detail += " s · ";
    detail += automatic ? "automática" : "manual";

    logEvent(
        "dosage",
        "Dosificación ejecutada",
        detail
    );
}

void readEventFileIntoRing(
    const char* path,
    String* ring,
    size_t capacity,
    size_t& count,
    size_t& nextIndex
)
{
    if (!LittleFS.exists(path))
    {
        return;
    }

    File eventFile = LittleFS.open(path, "r");

    if (!eventFile)
    {
        return;
    }

    while (eventFile.available())
    {
        String line = eventFile.readStringUntil('\n');
        line.trim();

        if (line.isEmpty())
        {
            continue;
        }

        ring[nextIndex] = line;
        nextIndex = (nextIndex + 1) % capacity;

        if (count < capacity)
        {
            count++;
        }
    }

    eventFile.close();
}

size_t getEventLogTotalBytes()
{
    size_t total = 0;

    const char* paths[] = {
        EVENT_LOG_OLD_PATH,
        EVENT_LOG_PATH
    };

    for (const char* path : paths)
    {
        if (!LittleFS.exists(path))
        {
            continue;
        }

        File file = LittleFS.open(path, "r");

        if (file)
        {
            total += file.size();
            file.close();
        }
    }

    return total;
}

void handleGetEvents()
{
    String eventRing[EVENT_API_MAX_ITEMS];
    size_t eventCount = 0;
    size_t nextIndex = 0;

    readEventFileIntoRing(
        EVENT_LOG_OLD_PATH,
        eventRing,
        EVENT_API_MAX_ITEMS,
        eventCount,
        nextIndex
    );

    readEventFileIntoRing(
        EVENT_LOG_PATH,
        eventRing,
        EVENT_API_MAX_ITEMS,
        eventCount,
        nextIndex
    );

    int requestedLimit = EVENT_API_MAX_ITEMS;

    if (server.hasArg("limit"))
    {
        requestedLimit = server.arg("limit").toInt();

        if (requestedLimit < 1)
        {
            requestedLimit = 1;
        }

        if (
            requestedLimit >
            static_cast<int>(EVENT_API_MAX_ITEMS)
        )
        {
            requestedLimit = EVENT_API_MAX_ITEMS;
        }
    }

    size_t outputCount = eventCount;

    if (outputCount > static_cast<size_t>(requestedLimit))
    {
        outputCount = requestedLimit;
    }

    String json;
    json.reserve(1500 + outputCount * 180);

    json += "{";
    json += "\"success\":true,";
    json += "\"count\":" + String(outputCount) + ",";
    json += "\"fileBytes\":" +
        String(static_cast<unsigned long>(getEventLogTotalBytes())) + ",";
    json += "\"clockConfigured\":";
    json += clockConfigured ? "true" : "false";
    json += ",\"events\":[";

    for (size_t index = 0; index < outputCount; index++)
    {
        if (index > 0)
        {
            json += ",";
        }

        size_t ringIndex =
            (nextIndex + EVENT_API_MAX_ITEMS - 1 - index) %
            EVENT_API_MAX_ITEMS;

        json += eventRing[ringIndex];
    }

    json += "]}";

    server.sendHeader("Cache-Control", "no-store");
    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );
}

void handleClearEvents()
{
    if (LittleFS.exists(EVENT_LOG_PATH))
    {
        LittleFS.remove(EVENT_LOG_PATH);
    }

    if (LittleFS.exists(EVENT_LOG_OLD_PATH))
    {
        LittleFS.remove(EVENT_LOG_OLD_PATH);
    }

    logEvent(
        "system",
        "Registro de eventos limpiado",
        "El historial anterior fue eliminado manualmente."
    );

    server.send(
        200,
        "application/json; charset=utf-8",
        "{\"success\":true,"
        "\"message\":\"Registro de eventos limpiado.\"}"
    );
}
