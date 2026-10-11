#include "ph_sensor.h"

#include "app_state.h"
#include "event_logger.h"
#include "utils.h"

#include <Preferences.h>
#include <math.h>

namespace
{
constexpr unsigned long PH_SAMPLE_INTERVAL_MS = 500;
constexpr uint8_t PH_SAMPLE_COUNT = 21;
constexpr uint8_t PH_TRIM_COUNT = 5;
constexpr uint8_t PH_HISTORY_COUNT = 5;
constexpr uint8_t PH_STABLE_SAMPLES_REQUIRED = 120;

constexpr float PH_EMA_ALPHA = 0.12f;
constexpr float PH_EMA_FAST_ALPHA = 0.35f;
constexpr float PH_EMA_FAST_THRESHOLD_MV = 80.0f;

constexpr uint32_t PH_MAX_BATCH_SAFETY_SPREAD_MV = 180;
constexpr uint32_t PH_MAX_STABILITY_WINDOW_SPREAD_MV = 10;
constexpr uint16_t PH_ADC_SATURATION_RAW = 4070;
constexpr float PH_MIN_CALIBRATION_SEPARATION_MV = 30.0f;

Preferences phPreferences;

bool sensorInitialized = false;
bool sensorSaturated = false;
bool sensorStable = false;

uint16_t filteredRaw = 0;
uint32_t filteredMillivolts = 0;
uint32_t medianMillivolts = 0;

float smoothedRaw = NAN;
float smoothedMillivolts = NAN;

uint32_t batchSpreadMv = 0;
uint32_t historySpreadMv = 0;
uint32_t filteredStepMv = 0;
uint32_t previousFilteredMillivolts = 0;
bool hasPreviousFilteredMillivolts = false;

uint32_t mvHistory[PH_HISTORY_COUNT] = {};
uint16_t rawHistory[PH_HISTORY_COUNT] = {};
uint8_t historyCount = 0;
uint8_t historyIndex = 0;
uint8_t stableSampleCount = 0;
uint32_t stabilityWindowMv[PH_STABLE_SAMPLES_REQUIRED] = {};
uint8_t stabilityWindowCount = 0;
uint8_t stabilityWindowIndex = 0;
uint32_t stabilityWindowSpreadMv = 0;
uint32_t stabilityWindowMinimumMv = 0;
uint32_t stabilityWindowMaximumMv = 0;
float stabilityWindowAverageMv = NAN;
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
    // Una vez calibrado, mostramos el pH continuamente aunque la señal
    // todavía esté estabilizándose. La estabilidad sigue siendo obligatoria
    // para calibrar y para dosificar en automático.
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

    // Se toman dos conversiones muy próximas por muestra:
    // raw sirve de diagnóstico y mV es la referencia usada para el pH.
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

    for (
        uint8_t i = PH_TRIM_COUNT;
        i < PH_SAMPLE_COUNT - PH_TRIM_COUNT;
        i++
    )
    {
        rawSum += rawSamples[i];
        mvSum += mvSamples[i];
    }

    constexpr uint8_t kept =
        PH_SAMPLE_COUNT - 2 * PH_TRIM_COUNT;

    const uint16_t measuredRaw =
        static_cast<uint16_t>(rawSum / kept);
    const uint32_t measuredMv = mvSum / kept;

    batchSpreadMv =
        mvSamples[PH_SAMPLE_COUNT - PH_TRIM_COUNT - 1] -
        mvSamples[PH_TRIM_COUNT];

    mvHistory[historyIndex] = measuredMv;
    rawHistory[historyIndex] = measuredRaw;

    historyIndex =
        (historyIndex + 1) % PH_HISTORY_COUNT;

    if (historyCount < PH_HISTORY_COUNT)
    {
        historyCount++;
    }

    uint32_t sortedMvHistory[PH_HISTORY_COUNT] = {};
    uint16_t sortedRawHistory[PH_HISTORY_COUNT] = {};

    for (uint8_t i = 0; i < historyCount; i++)
    {
        sortedMvHistory[i] = mvHistory[i];
        sortedRawHistory[i] = rawHistory[i];
    }

    sortSmallArray(sortedMvHistory, historyCount);
    sortSmallArray(sortedRawHistory, historyCount);

    medianMillivolts =
        sortedMvHistory[historyCount / 2];

    const uint16_t medianRaw =
        sortedRawHistory[historyCount / 2];

    historySpreadMv =
        sortedMvHistory[historyCount - 1] -
        sortedMvHistory[0];

    if (!isfinite(smoothedMillivolts))
    {
        smoothedMillivolts =
            static_cast<float>(medianMillivolts);
    }
    else
    {
        const float difference =
            fabsf(
                static_cast<float>(medianMillivolts) -
                smoothedMillivolts
            );

        const float alpha =
            difference >= PH_EMA_FAST_THRESHOLD_MV
                ? PH_EMA_FAST_ALPHA
                : PH_EMA_ALPHA;

        smoothedMillivolts +=
            alpha *
            (static_cast<float>(medianMillivolts) -
            smoothedMillivolts);
    }

    if (!isfinite(smoothedRaw))
    {
        smoothedRaw = static_cast<float>(medianRaw);
    }
    else
    {
        const float rawDifference =
            fabsf(
                static_cast<float>(medianRaw) -
                smoothedRaw
            );

        const float rawAlpha =
            rawDifference >= 100.0f
                ? PH_EMA_FAST_ALPHA
                : PH_EMA_ALPHA;

        smoothedRaw +=
            rawAlpha *
            (static_cast<float>(medianRaw) - smoothedRaw);
    }

    filteredMillivolts =
        static_cast<uint32_t>(smoothedMillivolts + 0.5f);
    filteredRaw =
        static_cast<uint16_t>(smoothedRaw + 0.5f);

    if (hasPreviousFilteredMillivolts)
    {
        filteredStepMv =
            filteredMillivolts > previousFilteredMillivolts
                ? filteredMillivolts - previousFilteredMillivolts
                : previousFilteredMillivolts - filteredMillivolts;
    }
    else
    {
        filteredStepMv = 0;
        hasPreviousFilteredMillivolts = true;
    }

    previousFilteredMillivolts = filteredMillivolts;

    sensorSaturated =
        rawSamples[PH_SAMPLE_COUNT - 1] >=
        PH_ADC_SATURATION_RAW;

    // Para calibrar exigimos 60 segundos CONTINUOS dentro de una banda
    // total de 10 mV. Si la lectura se desplaza más de 10 mV, la ventana se
    // reinicia desde la lectura actual. Así el contador representa progreso
    // real hacia la estabilidad y no queda clavado en 60/60 mientras arrastra
    // valores de la solución anterior.
    //
    // El ruido bruto del lote se sigue mostrando como diagnóstico. Solo un
    // pico muy grande (>180 mV), saturación o una lectura inválida reinician
    // por seguridad la ventana completa.
    const bool validForStability =
        historyCount == PH_HISTORY_COUNT &&
        !sensorSaturated &&
        measuredMv >= 50 &&
        batchSpreadMv <= PH_MAX_BATCH_SAFETY_SPREAD_MV;

    if (!validForStability)
    {
        stabilityWindowCount = 0;
        stabilityWindowIndex = 0;
        stableSampleCount = 0;
        stabilityWindowSpreadMv = 0;
        stabilityWindowMinimumMv = 0;
        stabilityWindowMaximumMv = 0;
        stabilityWindowAverageMv = NAN;
        sensorStable = false;
    }
    else
    {
        stabilityWindowMv[stabilityWindowIndex] =
            filteredMillivolts;

        stabilityWindowIndex =
            (stabilityWindowIndex + 1) %
            PH_STABLE_SAMPLES_REQUIRED;

        if (stabilityWindowCount < PH_STABLE_SAMPLES_REQUIRED)
        {
            stabilityWindowCount++;
        }

        uint32_t minimumMv = stabilityWindowMv[0];
        uint32_t maximumMv = stabilityWindowMv[0];
        uint32_t sumMv = 0;

        for (uint8_t i = 0; i < stabilityWindowCount; i++)
        {
            const uint32_t value = stabilityWindowMv[i];
            minimumMv = value < minimumMv ? value : minimumMv;
            maximumMv = value > maximumMv ? value : maximumMv;
            sumMv += value;
        }

        const uint32_t candidateSpreadMv = maximumMv - minimumMv;

        if (candidateSpreadMv > PH_MAX_STABILITY_WINDOW_SPREAD_MV)
        {
            // La meseta salió de la banda. Reiniciamos el minuto desde esta
            // lectura, que pasa a ser el nuevo punto de partida.
            stabilityWindowMv[0] = filteredMillivolts;
            stabilityWindowCount = 1;
            stabilityWindowIndex = 1;
            stableSampleCount = 1;
            stabilityWindowSpreadMv = 0;
            stabilityWindowMinimumMv = filteredMillivolts;
            stabilityWindowMaximumMv = filteredMillivolts;
            stabilityWindowAverageMv =
                static_cast<float>(filteredMillivolts);
            sensorStable = false;
        }
        else
        {
            stableSampleCount = stabilityWindowCount;
            stabilityWindowSpreadMv = candidateSpreadMv;
            stabilityWindowMinimumMv = minimumMv;
            stabilityWindowMaximumMv = maximumMv;
            stabilityWindowAverageMv =
                static_cast<float>(sumMv) /
                static_cast<float>(stabilityWindowCount);

            sensorStable =
                stabilityWindowCount == PH_STABLE_SAMPLES_REQUIRED;
        }
    }

    lastSampleAt = millis();
    updateCurrentPh();
}

String nullableFloatJson(float value, unsigned int decimals)
{
    if (!isfinite(value))
    {
        return "null";
    }

    return String(value, decimals);
}

String buildSensorStatusJson()
{
    String json = "{";
    json += "\"success\":true";
    json += ",\"initialized\":";
    json += sensorInitialized ? "true" : "false";
    json += ",\"pin\":" + String(PH_SENSOR_PIN);
    json += ",\"raw\":" +
        String(static_cast<unsigned int>(filteredRaw));
    json += ",\"millivolts\":" +
        String(filteredMillivolts);
    json += ",\"medianMillivolts\":" +
        String(medianMillivolts);
    json += ",\"voltage\":" +
        String(filteredMillivolts / 1000.0f, 3);
    json += ",\"stable\":";
    json += sensorStable ? "true" : "false";
    json += ",\"stableSamples\":" +
        String(stableSampleCount);
    json += ",\"stableSamplesRequired\":" +
        String(PH_STABLE_SAMPLES_REQUIRED);
    json += ",\"batchSpreadMv\":" +
        String(batchSpreadMv);
    json += ",\"historySpreadMv\":" +
        String(historySpreadMv);
    json += ",\"filteredStepMv\":" +
        String(filteredStepMv);
    json += ",\"stabilityWindowSpreadMv\":" +
        String(stabilityWindowSpreadMv);
    json += ",\"stabilityWindowMaxSpreadMv\":" +
        String(PH_MAX_STABILITY_WINDOW_SPREAD_MV);
    json += ",\"stabilityWindowMinMv\":" +
        String(stabilityWindowMinimumMv);
    json += ",\"stabilityWindowMaxMv\":" +
        String(stabilityWindowMaximumMv);
    json += ",\"stabilityElapsedMs\":" +
        String(
            static_cast<uint32_t>(stableSampleCount) *
            PH_SAMPLE_INTERVAL_MS
        );
    json += ",\"stabilityRequiredMs\":" +
        String(
            static_cast<uint32_t>(PH_STABLE_SAMPLES_REQUIRED) *
            PH_SAMPLE_INTERVAL_MS
        );
    json += ",\"stabilityAverageMv\":";
    json += nullableFloatJson(stabilityWindowAverageMv, 1);
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
        ? String(
            static_cast<uint32_t>(
                millis() - lastSampleAt
            )
        )
        : "0";
    json += "}";

    return json;
}

bool captureCalibrationPoint(float knownPh, String& errorMessage)
{
    if (!sensorInitialized)
    {
        errorMessage =
            "El sensor de pH todavía no está inicializado.";
        return false;
    }

    if (
        sensorInitialized &&
        static_cast<uint32_t>(millis() - lastSampleAt) > 1500
    )
    {
        sampleSensor();
    }

    if (sensorSaturated)
    {
        errorMessage =
            "El ADC está saturado. Revisá la tensión de PO antes de continuar.";
        return false;
    }

    if (!sensorStable)
    {
        errorMessage =
            "La señal de pH está inestable. Esperá a que el estado indique ESTABLE antes de capturar.";
        return false;
    }

    if (medianMillivolts < 50)
    {
        errorMessage =
            "La señal es demasiado baja para guardar una calibración válida.";
        return false;
    }

    const float previous7Mv = calibration7Mv;
    const float previous4Mv = calibration4Mv;

    // La calibración no guarda una lectura instantánea: usa el promedio
    // de las 120 lecturas filtradas del último minuto estable. Así el punto
    // capturado representa la meseta completa y no un valor aislado.
    const float captureMillivolts =
        isfinite(stabilityWindowAverageMv)
            ? stabilityWindowAverageMv
            : static_cast<float>(filteredMillivolts);

    if (knownPh == 7.0f)
    {
        calibration7Mv = captureMillivolts;
    }
    else if (knownPh == 4.0f)
    {
        calibration4Mv = captureMillivolts;
    }
    else
    {
        errorMessage =
            "Punto de calibración no soportado.";
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

    // Sincronizamos el filtro visual con el promedio realmente capturado.
    smoothedMillivolts = captureMillivolts;
    filteredMillivolts =
        static_cast<uint32_t>(captureMillivolts + 0.5f);
    previousFilteredMillivolts = filteredMillivolts;
    filteredStepMv = 0;

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

bool isPhSensorStable()
{
    return sensorStable;
}

uint16_t getPhSensorRaw()
{
    return filteredRaw;
}

uint32_t getPhSensorMillivolts()
{
    return filteredMillivolts;
}

uint32_t getPhSensorBatchSpreadMillivolts()
{
    return batchSpreadMv;
}

uint32_t getPhSensorHistorySpreadMillivolts()
{
    return historySpreadMv;
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
    const float capturedMv =
        knownPh == 7.0f ? calibration7Mv : calibration4Mv;
    detail += String(capturedMv, 1);
    detail += " mV (promedio 60 s) · GPIO ";
    detail += String(PH_SENSOR_PIN);

    logEvent(
        "ph",
        "Calibración de sensor capturada",
        detail
    );

    server.sendHeader("Cache-Control", "no-store");
    server.send(
        200,
        "application/json; charset=utf-8",
        buildSensorStatusJson()
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
