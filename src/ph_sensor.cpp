#include "ph_sensor.h"

#include "app_state.h"
#include "event_logger.h"
#include "utils.h"

#include <Preferences.h>
#include <math.h>

namespace
{
constexpr unsigned long PH_SAMPLE_INTERVAL_MS = 500;
constexpr uint8_t PH_SAMPLE_COUNT = 15;
constexpr uint8_t PH_TRIM_COUNT = 3;
constexpr uint16_t PH_ADC_SATURATION_RAW = 4070;
constexpr float PH_MIN_CALIBRATION_SEPARATION_MV = 30.0f;

Preferences phPreferences;

bool sensorInitialized = false;
bool sensorSaturated = false;

uint16_t filteredRaw = 0;
uint32_t filteredMillivolts = 0;
unsigned long lastSampleAt = 0;

float calibration7Mv = NAN;
float calibration4Mv = NAN;

template <typename T>
void sortSmallArray(T* values, uint8_t count)
{
    for (uint8_t i = 1; i < count; i++)
    {
        T key = values[i];
        int8_t j = static_cast<int8_t>(i) - 1;

        while (j >= 0 && values[j] > key)
        {
            values[j + 1] = values[j];
            j--;
        }

        values[j + 1] = key;
    }
}

bool calibrationPointStored(float value)
{
    return isfinite(value) && value > 0.0f;
}

bool calibrationIsValid()
{
    return
        calibrationPointStored(calibration7Mv) &&
        calibrationPointStored(calibration4Mv) &&
        fabsf(calibration4Mv - calibration7Mv) >=
            PH_MIN_CALIBRATION_SEPARATION_MV;
}

void loadCalibration()
{
    phPreferences.begin("phsensor", true);

    if (phPreferences.isKey("cal7mv"))
    {
        calibration7Mv =
            phPreferences.getFloat("cal7mv", NAN);
    }

    if (phPreferences.isKey("cal4mv"))
    {
        calibration4Mv =
            phPreferences.getFloat("cal4mv", NAN);
    }

    phPreferences.end();
}

void saveCalibration()
{
    phPreferences.begin("phsensor", false);

    if (calibrationPointStored(calibration7Mv))
    {
        phPreferences.putFloat("cal7mv", calibration7Mv);
    }
    else
    {
        phPreferences.remove("cal7mv");
    }

    if (calibrationPointStored(calibration4Mv))
    {
        phPreferences.putFloat("cal4mv", calibration4Mv);
    }
    else
    {
        phPreferences.remove("cal4mv");
    }

    phPreferences.end();
}

float calculatePhFromMillivolts(float millivolts)
{
    if (!calibrationIsValid())
    {
        return NAN;
    }

    const float slope =
        (4.0f - 7.0f) /
        (calibration4Mv - calibration7Mv);

    return 7.0f +
        (millivolts - calibration7Mv) * slope;
}

void updateCurrentPh()
{
    if (
        !sensorInitialized ||
        sensorSaturated ||
        !calibrationIsValid()
    )
    {
        currentPh = NAN;
        return;
    }

    const float calculated =
        calculatePhFromMillivolts(
            static_cast<float>(filteredMillivolts)
        );

    // Un valor fuera del rango físico útil se considera inválido.
    currentPh =
        isfinite(calculated) &&
        calculated >= 0.0f &&
        calculated <= 14.0f
            ? calculated
            : NAN;
}

void sampleSensor()
{
    uint16_t rawSamples[PH_SAMPLE_COUNT];
    uint32_t mvSamples[PH_SAMPLE_COUNT];

    for (uint8_t i = 0; i < PH_SAMPLE_COUNT; i++)
    {
        rawSamples[i] = analogRead(PH_SENSOR_PIN);
        mvSamples[i] = analogReadMilliVolts(PH_SENSOR_PIN);
        delayMicroseconds(800);
    }

    sortSmallArray(rawSamples, PH_SAMPLE_COUNT);
    sortSmallArray(mvSamples, PH_SAMPLE_COUNT);

    uint32_t rawSum = 0;
    uint32_t mvSum = 0;
    uint8_t kept = 0;

    for (
        uint8_t i = PH_TRIM_COUNT;
        i < PH_SAMPLE_COUNT - PH_TRIM_COUNT;
        i++
    )
    {
        rawSum += rawSamples[i];
        mvSum += mvSamples[i];
        kept++;
    }

    if (kept == 0)
    {
        return;
    }

    filteredRaw =
        static_cast<uint16_t>(rawSum / kept);

    filteredMillivolts =
        static_cast<uint32_t>(mvSum / kept);

    sensorSaturated =
        filteredRaw >= PH_ADC_SATURATION_RAW;

    lastSampleAt = millis();
    updateCurrentPh();
}

String nullableFloatJson(float value, unsigned int decimals)
{
    if (!isfinite(value))
    {
        return "null";
    }

    return String(value, static_cast<unsigned int>(decimals));
}

String buildSensorStatusJson()
{
    String json = "{";
    json += "\"success\":true";
    json += ",\"initialized\":";
    json += sensorInitialized ? "true" : "false";
    json += ",\"pin\":" + String(PH_SENSOR_PIN);
    json += ",\"raw\":" + String(static_cast<unsigned int>(filteredRaw));
    json += ",\"millivolts\":" + String(filteredMillivolts);
    json += ",\"voltage\":" +
        String(filteredMillivolts / 1000.0f, 3);
    json += ",\"saturated\":";
    json += sensorSaturated ? "true" : "false";
    json += ",\"calibrated\":";
    json += calibrationIsValid() ? "true" : "false";
    json += ",\"calibration7Mv\":";
    json += nullableFloatJson(calibration7Mv, 1);
    json += ",\"calibration4Mv\":";
    json += nullableFloatJson(calibration4Mv, 1);
    json += ",\"ph\":";
    json += nullableFloatJson(currentPh, 2);
    json += ",\"lastSampleAgeMs\":";
    json += sensorInitialized
        ? String(static_cast<uint32_t>(millis() - lastSampleAt))
        : "0";
    json += "}";

    return json;
}

bool captureCalibrationPoint(float knownPh, String& errorMessage)
{
    forcePhSensorSample();

    if (!sensorInitialized)
    {
        errorMessage = "El sensor de pH todavía no está inicializado.";
        return false;
    }

    if (sensorSaturated)
    {
        errorMessage =
            "El ADC está saturado. Desconectá PO de GPIO34 y revisá la tensión antes de continuar.";
        return false;
    }

    if (filteredMillivolts < 50)
    {
        errorMessage =
            "La señal es demasiado baja para guardar una calibración válida.";
        return false;
    }

    const float previous7Mv = calibration7Mv;
    const float previous4Mv = calibration4Mv;

    if (knownPh == 7.0f)
    {
        calibration7Mv =
            static_cast<float>(filteredMillivolts);
    }
    else if (knownPh == 4.0f)
    {
        calibration4Mv =
            static_cast<float>(filteredMillivolts);
    }
    else
    {
        errorMessage = "Punto de calibración no soportado.";
        return false;
    }

    if (
        calibrationPointStored(calibration7Mv) &&
        calibrationPointStored(calibration4Mv) &&
        fabsf(calibration4Mv - calibration7Mv) <
            PH_MIN_CALIBRATION_SEPARATION_MV
    )
    {
        calibration7Mv = previous7Mv;
        calibration4Mv = previous4Mv;

        errorMessage =
            "Los puntos pH 7 y pH 4 quedaron demasiado cerca. Esperá a que la sonda se estabilice y repetí la captura.";
        return false;
    }

    saveCalibration();
    updateCurrentPh();

    return true;
}
}

void initializePhSensor()
{
    analogReadResolution(12);
    analogSetPinAttenuation(PH_SENSOR_PIN, ADC_11db);

    loadCalibration();

    sensorInitialized = true;
    forcePhSensorSample();

    Serial.printf(
        "Sensor pH listo: PH-4502C · GPIO %u · ADC1 · %lu mV · %s\n",
        PH_SENSOR_PIN,
        static_cast<unsigned long>(filteredMillivolts),
        calibrationIsValid()
            ? "calibrado"
            : "sin calibrar"
    );
}

void processPhSensor()
{
    if (!sensorInitialized)
    {
        return;
    }

    const unsigned long now = millis();

    if (
        static_cast<uint32_t>(now - lastSampleAt) <
        PH_SAMPLE_INTERVAL_MS
    )
    {
        return;
    }

    sampleSensor();
}

void forcePhSensorSample()
{
    if (!sensorInitialized)
    {
        return;
    }

    sampleSensor();
}

bool isPhSensorInitialized()
{
    return sensorInitialized;
}

bool isPhSensorCalibrated()
{
    return calibrationIsValid();
}

bool isPhSensorSaturated()
{
    return sensorSaturated;
}

uint16_t getPhSensorRaw()
{
    return filteredRaw;
}

uint32_t getPhSensorMillivolts()
{
    return filteredMillivolts;
}

float getPhSensorVoltage()
{
    return filteredMillivolts / 1000.0f;
}

float getPhCalibration7Millivolts()
{
    return calibration7Mv;
}

float getPhCalibration4Millivolts()
{
    return calibration4Mv;
}

void handleGetPhSensorStatus()
{
    server.sendHeader("Cache-Control", "no-store");
    server.send(
        200,
        "application/json; charset=utf-8",
        buildSensorStatusJson()
    );
}

void handleCapturePhCalibration()
{
    if (!server.hasArg("point"))
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,\"message\":\"Falta indicar el punto de calibración.\"}"
        );
        return;
    }

    const String point = server.arg("point");
    float knownPh = NAN;

    if (point == "7")
    {
        knownPh = 7.0f;
    }
    else if (point == "4")
    {
        knownPh = 4.0f;
    }
    else
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,\"message\":\"Solo se admiten los puntos pH 7 y pH 4.\"}"
        );
        return;
    }

    String errorMessage;

    if (!captureCalibrationPoint(knownPh, errorMessage))
    {
        String json = "{";
        json += "\"success\":false,\"message\":\"";
        json += escapeJson(errorMessage);
        json += "\"}";

        server.send(
            409,
            "application/json; charset=utf-8",
            json
        );
        return;
    }

    String detail = "pH ";
    detail += String(knownPh, 0);
    detail += " · ";
    detail += String(filteredMillivolts);
    detail += " mV · GPIO ";
    detail += String(PH_SENSOR_PIN);

    logEvent(
        "ph",
        "Calibración de sensor capturada",
        detail
    );

    String json = buildSensorStatusJson();
    server.sendHeader("Cache-Control", "no-store");
    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );
}

void handleResetPhCalibration()
{
    calibration7Mv = NAN;
    calibration4Mv = NAN;
    saveCalibration();
    updateCurrentPh();

    logEvent(
        "ph",
        "Calibración de sensor borrada",
        "PH-4502C · GPIO 34"
    );

    server.sendHeader("Cache-Control", "no-store");
    server.send(
        200,
        "application/json; charset=utf-8",
        buildSensorStatusJson()
    );
}
