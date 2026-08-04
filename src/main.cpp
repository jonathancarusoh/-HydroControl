#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <esp_system.h>
#include <esp_timer.h>

// ======================================================
// SERVIDOR Y MEMORIA
// ======================================================

WebServer server(80);
DNSServer dnsServer;

Preferences hydroPreferences;
Preferences wifiPreferences;
Preferences profilePreferences;

// ======================================================
// CONFIGURACIÓN DEL PORTAL WIFI
// ======================================================

const char* SETUP_WIFI_NAME = "HydroControl-Setup";
const char* SETUP_WIFI_PASSWORD = "hydrocontrol";

const char* LOCAL_WIFI_NAME = "HydroControl";
const char* LOCAL_WIFI_PASSWORD = "hydrocontrol";

const char* MDNS_HOSTNAME = "hydrocontrol";
const char* FIRMWARE_VERSION = "0.1.0";
const byte DNS_PORT = 53;

bool wifiSetupMode = false;
bool localAccessMode = false;
bool restartPending = false;
unsigned long restartRequestedAt = 0;

const unsigned long RESTART_DELAY_MS = 3000;

// ======================================================
// CONFIGURACIÓN DE HYDROCONTROL
// ======================================================

struct HydroConfig
{
    float targetPh = 5.80f;
    float phTolerance = 0.10f;

    uint32_t doseDurationMs = 500;
    uint32_t doseIntervalMinutes = 4;

    uint8_t maxConsecutiveDoses = 3;

    bool automaticMode = true;

    // Valores generales que después usarán los módulos de EC y luz.
    float targetEc = 1.40f;
    uint8_t lightOnHour = 6;
    uint8_t lightOnMinute = 0;
    uint8_t lightOffHour = 18;
    uint8_t lightOffMinute = 0;
};

HydroConfig config;

const uint8_t MAX_CULTIVATION_PROFILES = 10;

struct CultivationProfile
{
    bool used = false;
    String name;

    float targetPh = 5.80f;
    float phTolerance = 0.10f;
    uint32_t doseDurationMs = 500;
    uint32_t doseIntervalMinutes = 4;
    uint8_t maxConsecutiveDoses = 3;
    bool automaticMode = true;

    float targetEc = 1.40f;
    uint8_t lightOnHour = 6;
    uint8_t lightOnMinute = 0;
    uint8_t lightOffHour = 18;
    uint8_t lightOffMinute = 0;
};

// ======================================================
// DATOS SIMULADOS
// Se reemplazarán después por sensores reales
// ======================================================

float currentPh = 5.82f;
float currentEc = 1.45f;
float waterTemperature = 18.5f;
float airHumidity = 61.0f;

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

void loadConfig()
{
    hydroPreferences.begin("hydrocontrol", true);

    config.targetPh =
        hydroPreferences.getFloat("targetPh", 5.80f);

    config.phTolerance =
        hydroPreferences.getFloat("tolerance", 0.10f);

    config.doseDurationMs =
        hydroPreferences.getUInt("doseMs", 500);

    config.doseIntervalMinutes =
        hydroPreferences.getUInt("intervalMin", 4);

    config.maxConsecutiveDoses =
        hydroPreferences.getUChar("maxDoses", 3);

    config.automaticMode =
        hydroPreferences.getBool("autoMode", true);

    config.targetEc =
        hydroPreferences.getFloat("targetEc", 1.40f);

    config.lightOnHour =
        hydroPreferences.getUChar("lightOnH", 6);

    config.lightOnMinute =
        hydroPreferences.getUChar("lightOnM", 0);

    config.lightOffHour =
        hydroPreferences.getUChar("lightOffH", 18);

    config.lightOffMinute =
        hydroPreferences.getUChar("lightOffM", 0);

    hydroPreferences.end();
}

void saveConfig()
{
    hydroPreferences.begin("hydrocontrol", false);

    hydroPreferences.putFloat(
        "targetPh",
        config.targetPh
    );

    hydroPreferences.putFloat(
        "tolerance",
        config.phTolerance
    );

    hydroPreferences.putUInt(
        "doseMs",
        config.doseDurationMs
    );

    hydroPreferences.putUInt(
        "intervalMin",
        config.doseIntervalMinutes
    );

    hydroPreferences.putUChar(
        "maxDoses",
        config.maxConsecutiveDoses
    );

    hydroPreferences.putBool(
        "autoMode",
        config.automaticMode
    );

    hydroPreferences.putFloat(
        "targetEc",
        config.targetEc
    );

    hydroPreferences.putUChar(
        "lightOnH",
        config.lightOnHour
    );

    hydroPreferences.putUChar(
        "lightOnM",
        config.lightOnMinute
    );

    hydroPreferences.putUChar(
        "lightOffH",
        config.lightOffHour
    );

    hydroPreferences.putUChar(
        "lightOffM",
        config.lightOffMinute
    );

    hydroPreferences.end();
}

// ======================================================
// CREDENCIALES WIFI
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

        WiFi.disconnect(true);
        delay(200);

        return false;
    }

    Serial.println(
        "WiFi conectado correctamente."
    );

    Serial.print("Dirección IP: ");
    Serial.println(WiFi.localIP());

    return true;
}

// ======================================================
// PORTAL DE CONFIGURACIÓN WIFI
// ======================================================

void handleWifiSetupPage()
{
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="es">

<head>
    <meta charset="UTF-8">

    <meta
        name="viewport"
        content="width=device-width, initial-scale=1.0">

    <meta
        name="theme-color"
        content="#1a1d21">

    <title>Configurar HydroControl</title>

    <style>
        * {
            box-sizing: border-box;
        }

        body {
            margin: 0;
            min-height: 100vh;
            padding: 20px;
            background: #1a1d21;
            color: #ffffff;
            font-family: Arial, sans-serif;
        }

        .container {
            width: 100%;
            max-width: 470px;
            margin: 20px auto;
        }

        .card {
            padding: 24px;
            background: #252a31;
            border: 1px solid #3c424b;
            border-radius: 18px;
            box-shadow: 0 15px 40px rgba(0, 0, 0, 0.35);
        }

        h1 {
            margin: 0 0 8px;
            font-size: 27px;
        }

        h2 {
            margin-top: 0;
            font-size: 20px;
        }

        p {
            color: #c4c9d0;
            line-height: 1.5;
        }

        button,
        input {
            width: 100%;
            min-height: 48px;
            padding: 12px 14px;
            border-radius: 10px;
            font-size: 16px;
        }

        button {
            border: none;
            cursor: pointer;
            font-weight: bold;
        }

        .refresh-button {
            margin-top: 10px;
            border: 1px solid #68707c;
            background: transparent;
            color: #ffffff;
        }

        .connect-button {
            margin-top: 20px;
            background: #198754;
            color: #ffffff;
        }

        .connect-button:disabled,
        .refresh-button:disabled {
            opacity: 0.6;
            cursor: not-allowed;
        }

        .network-list {
            display: flex;
            flex-direction: column;
            gap: 10px;
            margin-top: 18px;
        }

        .network-button {
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 12px;

            width: 100%;
            min-height: 62px;
            padding: 12px 14px;

            border: 1px solid #4a5059;
            border-radius: 11px;

            background: #1a1d21;
            color: #ffffff;

            text-align: left;
        }

        .network-button:hover {
            border-color: #198754;
        }

        .network-button.selected {
            border-color: #20c975;
            background: #173d2a;
        }

        .network-info {
            min-width: 0;
        }

        .network-name {
            overflow: hidden;
            font-weight: bold;
            white-space: nowrap;
            text-overflow: ellipsis;
        }

        .network-details {
            margin-top: 5px;
            color: #aeb4bc;
            font-size: 13px;
        }

        .network-icon {
            flex-shrink: 0;
            font-size: 20px;
        }

        .message {
            margin-top: 16px;
            padding: 12px;
            border-radius: 10px;
            background: #323840;
            color: #d7dbe0;
        }

        .message.error {
            border: 1px solid #dc3545;
            background: #442227;
            color: #ffb9c0;
        }

        .message.success {
            border: 1px solid #198754;
            background: #173d2a;
            color: #b7f5d1;
        }

        .selected-network {
            display: none;
            margin-top: 24px;
            padding-top: 22px;
            border-top: 1px solid #444a53;
        }

        .selected-network.visible {
            display: block;
        }

        .selected-box {
            margin-bottom: 16px;
            padding: 13px;
            border: 1px solid #198754;
            border-radius: 10px;
            background: #173d2a;
        }

        .selected-label {
            color: #aeb4bc;
            font-size: 13px;
        }

        .selected-name {
            margin-top: 4px;
            overflow-wrap: anywhere;
            font-size: 18px;
            font-weight: bold;
        }

        label {
            display: block;
            margin-bottom: 7px;
            font-weight: bold;
        }

        input {
            border: 1px solid #4a5059;
            background: #1a1d21;
            color: #ffffff;
        }

        .password-wrapper {
            position: relative;
        }

        .password-wrapper input {
            padding-right: 55px;
        }

        .toggle-password {
            position: absolute;
            top: 0;
            right: 0;

            width: 52px;
            min-height: 48px;
            padding: 0;

            background: transparent;
            color: #ffffff;
            font-size: 20px;
        }

        .hint {
            margin-top: 7px;
            color: #9da4ad;
            font-size: 13px;
        }

        .local-mode-box {
            margin-top: 26px;
            padding-top: 24px;
            border-top: 1px solid #444a53;
        }

        .local-mode-box h2 {
            margin-bottom: 8px;
        }

        .local-mode-button {
            margin-top: 10px;
            border: 1px solid #0dcaf0;
            background: #12343b;
            color: #bff5ff;
        }

        .local-mode-button:disabled {
            opacity: 0.6;
            cursor: not-allowed;
        }

        .spinner {
            display: inline-block;
            width: 18px;
            height: 18px;
            margin-right: 8px;

            border: 3px solid rgba(255, 255, 255, 0.3);
            border-top-color: #ffffff;
            border-radius: 50%;

            vertical-align: middle;
            animation: spin 0.8s linear infinite;
        }

        @keyframes spin {
            to {
                transform: rotate(360deg);
            }
        }

        @media (max-width: 500px) {
            body {
                padding: 12px;
            }

            .container {
                margin-top: 8px;
            }

            .card {
                padding: 19px;
            }

            h1 {
                font-size: 24px;
            }
        }
    </style>
</head>

<body>

    <div class="container">

        <div class="card">

            <h1>🌱 HydroControl</h1>

            <p>
                Seleccioná la red WiFi donde quedará conectado
                el controlador.
            </p>

            <button
                type="button"
                class="refresh-button"
                id="scanButton">

                Buscar redes cercanas

            </button>

            <div
                id="scanMessage"
                class="message">

                Buscando redes WiFi cercanas...

            </div>

            <div
                id="networkList"
                class="network-list">
            </div>

            <div
                id="selectedNetworkSection"
                class="selected-network">

                <h2>Conectar a la red</h2>

                <div class="selected-box">

                    <div class="selected-label">
                        Red seleccionada
                    </div>

                    <div
                        class="selected-name"
                        id="selectedNetworkName">
                    </div>

                </div>

                <form
                    method="POST"
                    action="/save-wifi"
                    id="wifiForm">

                    <input
                        id="ssid"
                        name="ssid"
                        type="hidden">

                    <label for="password">
                        Contraseña
                    </label>

                    <div class="password-wrapper">

                        <input
                            id="password"
                            name="password"
                            type="password"
                            autocomplete="new-password"
                            placeholder="Ingresá la contraseña">

                        <button
                            type="button"
                            class="toggle-password"
                            id="togglePassword"
                            aria-label="Mostrar contraseña">

                            👁

                        </button>

                    </div>

                    <div
                        id="passwordHint"
                        class="hint">

                        Para una red abierta, dejá este campo vacío.

                    </div>

                    <button
                        type="submit"
                        class="connect-button"
                        id="connectButton">

                        Guardar y conectar

                    </button>

                    <div
                        id="connectionMessage"
                        class="message"
                        hidden>
                    </div>

                </form>

            </div>

            <div class="local-mode-box">

                <h2>Usar sin una red WiFi</h2>

                <p>
                    El ESP32 creará su propia red para entrar
                    directamente desde el celular, aunque no haya
                    router ni conexión a Internet.
                </p>

                <button
                    type="button"
                    class="local-mode-button"
                    id="localModeButton">

                    Entrar en modo local

                </button>

                <div
                    id="localModeMessage"
                    class="message"
                    hidden>
                </div>

            </div>

        </div>

    </div>

    <script>
        const scanButton =
            document.getElementById("scanButton");

        const scanMessage =
            document.getElementById("scanMessage");

        const networkList =
            document.getElementById("networkList");

        const selectedNetworkSection =
            document.getElementById(
                "selectedNetworkSection"
            );

        const selectedNetworkName =
            document.getElementById(
                "selectedNetworkName"
            );

        const ssidInput =
            document.getElementById("ssid");

        const passwordInput =
            document.getElementById("password");

        const passwordHint =
            document.getElementById("passwordHint");

        const togglePassword =
            document.getElementById("togglePassword");

        const wifiForm =
            document.getElementById("wifiForm");

        const connectButton =
            document.getElementById("connectButton");

        const connectionMessage =
            document.getElementById(
                "connectionMessage"
            );

        const localModeButton =
            document.getElementById(
                "localModeButton"
            );

        const localModeMessage =
            document.getElementById(
                "localModeMessage"
            );

        function getSignalText(rssi) {
            if (rssi >= -50) {
                return "Señal excelente";
            }

            if (rssi >= -60) {
                return "Señal buena";
            }

            if (rssi >= -70) {
                return "Señal regular";
            }

            return "Señal débil";
        }

        function selectNetwork(network, button) {
            document
                .querySelectorAll(".network-button")
                .forEach(item => {
                    item.classList.remove("selected");
                });

            button.classList.add("selected");

            ssidInput.value = network.ssid;

            selectedNetworkName.textContent =
                network.ssid;

            passwordInput.value = "";

            passwordInput.required =
                network.secure === true;

            passwordInput.placeholder =
                network.secure
                    ? "Ingresá la contraseña"
                    : "Esta red no necesita contraseña";

            passwordHint.textContent =
                network.secure
                    ? "Ingresá la contraseña de esta red."
                    : "La red seleccionada es abierta.";

            selectedNetworkSection.classList.add(
                "visible"
            );

            setTimeout(() => {
                selectedNetworkSection.scrollIntoView({
                    behavior: "smooth",
                    block: "start"
                });

                if (network.secure) {
                    passwordInput.focus();
                }
            }, 100);
        }

        function createNetworkButton(network) {
            const button =
                document.createElement("button");

            button.type = "button";
            button.className = "network-button";

            const info =
                document.createElement("div");

            info.className = "network-info";

            const name =
                document.createElement("div");

            name.className = "network-name";
            name.textContent = network.ssid;

            const details =
                document.createElement("div");

            details.className = "network-details";

            details.textContent =
                getSignalText(network.rssi) +
                " · " +
                network.rssi +
                " dBm · " +
                (
                    network.secure
                        ? "Protegida"
                        : "Abierta"
                );

            const icon =
                document.createElement("div");

            icon.className = "network-icon";

            icon.textContent =
                network.secure ? "🔒" : "📶";

            info.appendChild(name);
            info.appendChild(details);

            button.appendChild(info);
            button.appendChild(icon);

            button.addEventListener("click", () => {
                selectNetwork(network, button);
            });

            return button;
        }

        async function scanNetworks() {
            scanButton.disabled = true;

            scanButton.innerHTML =
                '<span class="spinner"></span>' +
                'Buscando...';

            scanMessage.classList.remove("error");

            scanMessage.textContent =
                "Buscando redes WiFi cercanas...";

            networkList.innerHTML = "";

            selectedNetworkSection.classList.remove(
                "visible"
            );

            ssidInput.value = "";
            passwordInput.value = "";

            try {
                const response = await fetch(
                    "/api/wifi/scan",
                    {
                        cache: "no-store"
                    }
                );

                if (!response.ok) {
                    throw new Error(
                        "No se pudo realizar la búsqueda."
                    );
                }

                const networks =
                    await response.json();

                const uniqueNetworks = [];
                const registeredSsids = new Set();

                networks
                    .sort((a, b) => b.rssi - a.rssi)
                    .forEach(network => {
                        const ssid =
                            String(
                                network.ssid || ""
                            ).trim();

                        if (
                            !ssid ||
                            registeredSsids.has(ssid)
                        ) {
                            return;
                        }

                        registeredSsids.add(ssid);

                        uniqueNetworks.push({
                            ssid: ssid,
                            rssi: Number(network.rssi),
                            secure: Boolean(
                                network.secure
                            )
                        });
                    });

                if (uniqueNetworks.length === 0) {
                    scanMessage.textContent =
                        "No se encontraron redes WiFi.";

                    return;
                }

                scanMessage.textContent =
                    "Seleccioná una de las " +
                    uniqueNetworks.length +
                    " redes encontradas.";

                uniqueNetworks.forEach(network => {
                    networkList.appendChild(
                        createNetworkButton(network)
                    );
                });

            } catch (error) {
                console.error(
                    "Error buscando redes:",
                    error
                );

                scanMessage.textContent =
                    "No se pudieron buscar las redes. " +
                    "Presioná el botón para intentar nuevamente.";

                scanMessage.classList.add("error");

            } finally {
                scanButton.disabled = false;

                scanButton.textContent =
                    "Buscar nuevamente";
            }
        }

        togglePassword.addEventListener(
            "click",
            () => {
                const isHidden =
                    passwordInput.type === "password";

                passwordInput.type =
                    isHidden ? "text" : "password";

                togglePassword.textContent =
                    isHidden ? "🙈" : "👁";
            }
        );

        wifiForm.addEventListener(
            "submit",
            async event => {
                event.preventDefault();

                if (!ssidInput.value) {
                    scanMessage.textContent =
                        "Primero seleccioná una red.";

                    scanMessage.classList.add("error");

                    return;
                }

                connectionMessage.hidden = false;

                connectionMessage.classList.remove(
                    "error",
                    "success"
                );

                connectionMessage.textContent =
                    "Probando la conexión WiFi. " +
                    "Esto puede tardar unos segundos...";

                connectButton.disabled = true;
                scanButton.disabled = true;
                passwordInput.disabled = true;

                connectButton.innerHTML =
                    '<span class="spinner"></span>' +
                    'Conectando...';

                const data = new URLSearchParams({
                    ssid: ssidInput.value,
                    password: passwordInput.value
                });

                try {
                    const response = await fetch(
                        "/save-wifi",
                        {
                            method: "POST",

                            headers: {
                                "Content-Type":
                                    "application/x-www-form-urlencoded"
                            },

                            body: data.toString()
                        }
                    );

                    const result =
                        await response.json();

                    if (
                        !response.ok ||
                        !result.success
                    )
                    {
                        throw new Error(
                            result.message ||
                            "No se pudo conectar."
                        );
                    }

                    connectionMessage.classList.add(
                        "success"
                    );

                    connectionMessage.textContent =
                        "✅ Conexión exitosa a \"" +
                        result.ssid +
                        "\". HydroControl se reiniciará. " +
                        "Después conectate a esa misma red y abrí " +
                        "hydrocontrol.local.";

                    connectButton.innerHTML =
                        "Conectado correctamente";

                } catch (error) {
                    console.error(
                        "Error conectando al WiFi:",
                        error
                    );

                    connectionMessage.classList.add(
                        "error"
                    );

                    connectionMessage.textContent =
                        "❌ " + error.message;

                    passwordInput.value = "";
                    passwordInput.disabled = false;
                    passwordInput.focus();

                    connectButton.disabled = false;
                    scanButton.disabled = false;

                    connectButton.textContent =
                        "Volver a intentar";
                }
            }
        );

        localModeButton.addEventListener(
            "click",
            async () => {
                localModeButton.disabled = true;
                scanButton.disabled = true;

                localModeMessage.hidden = false;
                localModeMessage.classList.remove(
                    "error",
                    "success"
                );

                localModeMessage.textContent =
                    "Preparando el acceso local...";

                localModeButton.innerHTML =
                    '<span class="spinner"></span>' +
                    'Activando...';

                try {
                    const response = await fetch(
                        "/use-local-mode",
                        {
                            method: "POST"
                        }
                    );

                    const result =
                        await response.json();

                    if (
                        !response.ok ||
                        !result.success
                    ) {
                        throw new Error(
                            result.message ||
                            "No se pudo activar el modo local."
                        );
                    }

                    localModeMessage.classList.add(
                        "success"
                    );

                    localModeMessage.textContent =
                        "✅ Modo local activado. El ESP32 se " +
                        "reiniciará. Después conectate a la red " +
                        "HydroControl con la contraseña " +
                        "hydrocontrol y abrí 192.168.4.1.";

                    localModeButton.textContent =
                        "Modo local activado";

                } catch (error) {
                    console.error(
                        "Error activando modo local:",
                        error
                    );

                    localModeMessage.classList.add(
                        "error"
                    );

                    localModeMessage.textContent =
                        "❌ " + error.message;

                    localModeButton.disabled = false;
                    scanButton.disabled = false;

                    localModeButton.textContent =
                        "Volver a intentar";
                }
            }
        );

        scanButton.addEventListener(
            "click",
            scanNetworks
        );

        scanNetworks();
    </script>

</body>
</html>
)rawliteral";

    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    server.send(
        200,
        "text/html; charset=utf-8",
        html
    );
}

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
}


// ======================================================
// PERFILES DE CULTIVO
// ======================================================

String profileKey(
    const char* prefix,
    uint8_t slot
)
{
    return String(prefix) + String(slot);
}

String formatTime(
    uint8_t hour,
    uint8_t minute
)
{
    char buffer[6];

    snprintf(
        buffer,
        sizeof(buffer),
        "%02u:%02u",
        hour,
        minute
    );

    return String(buffer);
}

bool parseTimeValue(
    const String& value,
    uint8_t& hour,
    uint8_t& minute
)
{
    if (
        value.length() != 5 ||
        value.charAt(2) != ':'
    )
    {
        return false;
    }

    for (uint8_t index = 0; index < 5; index++)
    {
        if (index == 2)
        {
            continue;
        }

        if (!isDigit(value.charAt(index)))
        {
            return false;
        }
    }

    int parsedHour = value.substring(0, 2).toInt();
    int parsedMinute = value.substring(3, 5).toInt();

    if (
        parsedHour < 0 ||
        parsedHour > 23 ||
        parsedMinute < 0 ||
        parsedMinute > 59
    )
    {
        return false;
    }

    hour = static_cast<uint8_t>(parsedHour);
    minute = static_cast<uint8_t>(parsedMinute);

    return true;
}

int8_t getActiveProfileSlot()
{
    profilePreferences.begin("profiles", true);

    int8_t slot =
        profilePreferences.getChar("active", -1);

    profilePreferences.end();

    if (
        slot < 0 ||
        slot >= MAX_CULTIVATION_PROFILES
    )
    {
        return -1;
    }

    return slot;
}

void setActiveProfileSlot(int8_t slot)
{
    profilePreferences.begin("profiles", false);
    profilePreferences.putChar("active", slot);
    profilePreferences.end();
}

bool loadProfile(
    uint8_t slot,
    CultivationProfile& profile
)
{
    if (slot >= MAX_CULTIVATION_PROFILES)
    {
        return false;
    }

    profilePreferences.begin("profiles", true);

    String usedKey = profileKey("u", slot);

    profile.used = profilePreferences.getBool(
        usedKey.c_str(),
        false
    );

    if (!profile.used)
    {
        profilePreferences.end();
        return false;
    }

    String nameKey = profileKey("n", slot);
    String targetPhKey = profileKey("ph", slot);
    String toleranceKey = profileKey("pt", slot);
    String durationKey = profileKey("dm", slot);
    String intervalKey = profileKey("di", slot);
    String maxDosesKey = profileKey("md", slot);
    String autoModeKey = profileKey("am", slot);
    String targetEcKey = profileKey("ec", slot);
    String lightOnHourKey = profileKey("loh", slot);
    String lightOnMinuteKey = profileKey("lom", slot);
    String lightOffHourKey = profileKey("lfh", slot);
    String lightOffMinuteKey = profileKey("lfm", slot);

    profile.name = profilePreferences.getString(
        nameKey.c_str(),
        "Perfil"
    );

    profile.targetPh = profilePreferences.getFloat(
        targetPhKey.c_str(),
        5.80f
    );

    profile.phTolerance = profilePreferences.getFloat(
        toleranceKey.c_str(),
        0.10f
    );

    profile.doseDurationMs = profilePreferences.getUInt(
        durationKey.c_str(),
        500
    );

    profile.doseIntervalMinutes = profilePreferences.getUInt(
        intervalKey.c_str(),
        4
    );

    profile.maxConsecutiveDoses = profilePreferences.getUChar(
        maxDosesKey.c_str(),
        3
    );

    profile.automaticMode = profilePreferences.getBool(
        autoModeKey.c_str(),
        true
    );

    profile.targetEc = profilePreferences.getFloat(
        targetEcKey.c_str(),
        1.40f
    );

    profile.lightOnHour = profilePreferences.getUChar(
        lightOnHourKey.c_str(),
        6
    );

    profile.lightOnMinute = profilePreferences.getUChar(
        lightOnMinuteKey.c_str(),
        0
    );

    profile.lightOffHour = profilePreferences.getUChar(
        lightOffHourKey.c_str(),
        18
    );

    profile.lightOffMinute = profilePreferences.getUChar(
        lightOffMinuteKey.c_str(),
        0
    );

    profilePreferences.end();

    return true;
}

void saveProfile(
    uint8_t slot,
    const CultivationProfile& profile
)
{
    profilePreferences.begin("profiles", false);

    String usedKey = profileKey("u", slot);
    String nameKey = profileKey("n", slot);
    String targetPhKey = profileKey("ph", slot);
    String toleranceKey = profileKey("pt", slot);
    String durationKey = profileKey("dm", slot);
    String intervalKey = profileKey("di", slot);
    String maxDosesKey = profileKey("md", slot);
    String autoModeKey = profileKey("am", slot);
    String targetEcKey = profileKey("ec", slot);
    String lightOnHourKey = profileKey("loh", slot);
    String lightOnMinuteKey = profileKey("lom", slot);
    String lightOffHourKey = profileKey("lfh", slot);
    String lightOffMinuteKey = profileKey("lfm", slot);

    profilePreferences.putBool(
        usedKey.c_str(),
        true
    );

    profilePreferences.putString(
        nameKey.c_str(),
        profile.name
    );

    profilePreferences.putFloat(
        targetPhKey.c_str(),
        profile.targetPh
    );

    profilePreferences.putFloat(
        toleranceKey.c_str(),
        profile.phTolerance
    );

    profilePreferences.putUInt(
        durationKey.c_str(),
        profile.doseDurationMs
    );

    profilePreferences.putUInt(
        intervalKey.c_str(),
        profile.doseIntervalMinutes
    );

    profilePreferences.putUChar(
        maxDosesKey.c_str(),
        profile.maxConsecutiveDoses
    );

    profilePreferences.putBool(
        autoModeKey.c_str(),
        profile.automaticMode
    );

    profilePreferences.putFloat(
        targetEcKey.c_str(),
        profile.targetEc
    );

    profilePreferences.putUChar(
        lightOnHourKey.c_str(),
        profile.lightOnHour
    );

    profilePreferences.putUChar(
        lightOnMinuteKey.c_str(),
        profile.lightOnMinute
    );

    profilePreferences.putUChar(
        lightOffHourKey.c_str(),
        profile.lightOffHour
    );

    profilePreferences.putUChar(
        lightOffMinuteKey.c_str(),
        profile.lightOffMinute
    );

    profilePreferences.end();
}

void deleteProfile(uint8_t slot)
{
    profilePreferences.begin("profiles", false);

    const char* prefixes[] = {
        "u", "n", "ph", "pt", "dm", "di",
        "md", "am", "ec", "loh", "lom",
        "lfh", "lfm"
    };

    for (const char* prefix : prefixes)
    {
        String key = profileKey(prefix, slot);
        profilePreferences.remove(key.c_str());
    }

    profilePreferences.end();

    if (getActiveProfileSlot() == slot)
    {
        setActiveProfileSlot(-1);
    }
}

int8_t findFreeProfileSlot()
{
    for (
        uint8_t slot = 0;
        slot < MAX_CULTIVATION_PROFILES;
        slot++
    )
    {
        CultivationProfile profile;

        if (!loadProfile(slot, profile))
        {
            return slot;
        }
    }

    return -1;
}

void applyProfileToConfig(
    const CultivationProfile& profile
)
{
    config.targetPh = profile.targetPh;
    config.phTolerance = profile.phTolerance;
    config.doseDurationMs = profile.doseDurationMs;
    config.doseIntervalMinutes = profile.doseIntervalMinutes;
    config.maxConsecutiveDoses = profile.maxConsecutiveDoses;
    config.automaticMode = profile.automaticMode;

    config.targetEc = profile.targetEc;
    config.lightOnHour = profile.lightOnHour;
    config.lightOnMinute = profile.lightOnMinute;
    config.lightOffHour = profile.lightOffHour;
    config.lightOffMinute = profile.lightOffMinute;

    saveConfig();
}

void appendProfileJson(
    String& json,
    uint8_t slot,
    const CultivationProfile& profile,
    int8_t activeSlot
)
{
    json += "{";
    json += "\"id\":" + String(slot) + ",";
    json += "\"name\":\"";
    json += escapeJson(profile.name);
    json += "\",";
    json += "\"active\":";
    json += slot == activeSlot ? "true" : "false";
    json += ",\"targetPh\":" + String(profile.targetPh, 2);
    json += ",\"tolerance\":" + String(profile.phTolerance, 2);
    json += ",\"doseSeconds\":" +
        String(profile.doseDurationMs / 1000.0f, 2);
    json += ",\"intervalMinutes\":" +
        String(profile.doseIntervalMinutes);
    json += ",\"maxDoses\":" +
        String(profile.maxConsecutiveDoses);
    json += ",\"automaticMode\":";
    json += profile.automaticMode ? "true" : "false";
    json += ",\"targetEc\":" + String(profile.targetEc, 2);
    json += ",\"lightOn\":\"";
    json += formatTime(
        profile.lightOnHour,
        profile.lightOnMinute
    );
    json += "\"";
    json += ",\"lightOff\":\"";
    json += formatTime(
        profile.lightOffHour,
        profile.lightOffMinute
    );
    json += "\"";
    json += "}";
}

void handleGetProfiles()
{
    int8_t activeSlot = getActiveProfileSlot();

    String json;
    json.reserve(6500);

    json += "{";
    json += "\"success\":true,";
    json += "\"activeId\":" +
        String(static_cast<int>(activeSlot)) + ",";
    json += "\"maxProfiles\":" +
        String(MAX_CULTIVATION_PROFILES) + ",";
    json += "\"profiles\":[";

    bool firstProfile = true;

    for (
        uint8_t slot = 0;
        slot < MAX_CULTIVATION_PROFILES;
        slot++
    )
    {
        CultivationProfile profile;

        if (!loadProfile(slot, profile))
        {
            continue;
        }

        if (!firstProfile)
        {
            json += ",";
        }

        appendProfileJson(
            json,
            slot,
            profile,
            activeSlot
        );

        firstProfile = false;
    }

    json += "]}";

    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );
}

bool readProfileFromRequest(
    CultivationProfile& profile,
    String& errorMessage
)
{
    const char* requiredArguments[] = {
        "name",
        "targetPh",
        "tolerance",
        "doseSeconds",
        "intervalMinutes",
        "maxDoses",
        "automaticMode",
        "targetEc",
        "lightOn",
        "lightOff"
    };

    for (const char* argument : requiredArguments)
    {
        if (!server.hasArg(argument))
        {
            errorMessage =
                "Faltan datos para guardar el perfil.";

            return false;
        }
    }

    profile.name = server.arg("name");
    profile.name.trim();

    if (
        profile.name.isEmpty() ||
        profile.name.length() > 32
    )
    {
        errorMessage =
            "El nombre debe tener entre 1 y 32 caracteres.";

        return false;
    }

    profile.targetPh =
        server.arg("targetPh").toFloat();

    profile.phTolerance =
        server.arg("tolerance").toFloat();

    float doseSeconds =
        server.arg("doseSeconds").toFloat();

    int intervalMinutes =
        server.arg("intervalMinutes").toInt();

    int maxDoses =
        server.arg("maxDoses").toInt();

    profile.automaticMode =
        server.arg("automaticMode") == "true";

    profile.targetEc =
        server.arg("targetEc").toFloat();

    String lightOn = server.arg("lightOn");
    String lightOff = server.arg("lightOff");

    if (
        profile.targetPh < 4.0f ||
        profile.targetPh > 8.0f ||
        profile.phTolerance < 0.01f ||
        profile.phTolerance > 1.0f ||
        doseSeconds < 0.1f ||
        doseSeconds > 30.0f ||
        intervalMinutes < 1 ||
        intervalMinutes > 120 ||
        maxDoses < 1 ||
        maxDoses > 10 ||
        profile.targetEc < 0.10f ||
        profile.targetEc > 5.00f
    )
    {
        errorMessage =
            "Hay valores fuera del rango permitido.";

        return false;
    }

    if (
        !parseTimeValue(
            lightOn,
            profile.lightOnHour,
            profile.lightOnMinute
        ) ||
        !parseTimeValue(
            lightOff,
            profile.lightOffHour,
            profile.lightOffMinute
        )
    )
    {
        errorMessage =
            "Los horarios de luz no son válidos.";

        return false;
    }

    profile.doseDurationMs =
        static_cast<uint32_t>(doseSeconds * 1000.0f);

    profile.doseIntervalMinutes =
        static_cast<uint32_t>(intervalMinutes);

    profile.maxConsecutiveDoses =
        static_cast<uint8_t>(maxDoses);

    profile.used = true;

    return true;
}

void handleSaveProfile()
{
    CultivationProfile profile;
    String errorMessage;

    if (!readProfileFromRequest(profile, errorMessage))
    {
        String json = "{";
        json += "\"success\":false,";
        json += "\"message\":\"";
        json += escapeJson(errorMessage);
        json += "\"}";

        server.send(
            400,
            "application/json; charset=utf-8",
            json
        );

        return;
    }

    int requestedSlot = -1;

    if (server.hasArg("id"))
    {
        requestedSlot = server.arg("id").toInt();
    }

    if (
        server.hasArg("id") &&
        requestedSlot != -1 &&
        (
            requestedSlot < 0 ||
            requestedSlot >= MAX_CULTIVATION_PROFILES
        )
    )
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"El identificador del perfil no es válido.\"}"
        );

        return;
    }

    bool updating =
        requestedSlot >= 0 &&
        requestedSlot < MAX_CULTIVATION_PROFILES;

    int8_t slot = updating
        ? static_cast<int8_t>(requestedSlot)
        : findFreeProfileSlot();

    if (slot < 0)
    {
        server.send(
            409,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"Ya alcanzaste el máximo de 10 perfiles.\"}"
        );

        return;
    }

    if (updating)
    {
        CultivationProfile existingProfile;

        if (!loadProfile(slot, existingProfile))
        {
            server.send(
                404,
                "application/json; charset=utf-8",
                "{\"success\":false,"
                "\"message\":\"El perfil que querés editar no existe.\"}"
            );

            return;
        }
    }

    saveProfile(slot, profile);

    // Si se edita el perfil activo, los nuevos valores pasan
    // a ser la configuración activa inmediatamente.
    if (getActiveProfileSlot() == slot)
    {
        applyProfileToConfig(profile);
    }

    String json = "{";
    json += "\"success\":true,";
    json += "\"id\":" + String(slot) + ",";
    json += "\"message\":\"";
    json += updating
        ? "Perfil actualizado correctamente."
        : "Perfil guardado correctamente.";
    json += "\"}";

    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );
}

void handleApplyProfile()
{
    if (!server.hasArg("id"))
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"Falta seleccionar un perfil.\"}"
        );

        return;
    }

    int slot = server.arg("id").toInt();

    if (
        slot < 0 ||
        slot >= MAX_CULTIVATION_PROFILES
    )
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"El perfil seleccionado no es válido.\"}"
        );

        return;
    }

    CultivationProfile profile;

    if (!loadProfile(slot, profile))
    {
        server.send(
            404,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"El perfil seleccionado no existe.\"}"
        );

        return;
    }

    applyProfileToConfig(profile);
    setActiveProfileSlot(static_cast<int8_t>(slot));

    String json = "{";
    json += "\"success\":true,";
    json += "\"activeId\":" + String(slot) + ",";
    json += "\"message\":\"Perfil aplicado: ";
    json += escapeJson(profile.name);
    json += ".\"}";

    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );
}

void handleDeleteProfile()
{
    if (!server.hasArg("id"))
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"Falta seleccionar un perfil.\"}"
        );

        return;
    }

    int slot = server.arg("id").toInt();

    if (
        slot < 0 ||
        slot >= MAX_CULTIVATION_PROFILES
    )
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"El perfil seleccionado no es válido.\"}"
        );

        return;
    }

    CultivationProfile profile;

    if (!loadProfile(slot, profile))
    {
        server.send(
            404,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"El perfil seleccionado no existe.\"}"
        );

        return;
    }

    String deletedName = profile.name;
    deleteProfile(static_cast<uint8_t>(slot));

    String json = "{";
    json += "\"success\":true,";
    json += "\"message\":\"Perfil eliminado: ";
    json += escapeJson(deletedName);
    json += ".\"}";

    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );
}

// ======================================================
// API DE ESTADO GENERAL
// ======================================================

void handleApiStatus()
{
    String json = "{";

    json += "\"online\":true,";
    json += "\"ph\":" + String(currentPh, 2) + ",";
    json += "\"ec\":" + String(currentEc, 2) + ",";
    json += "\"waterTemp\":" +
        String(waterTemperature, 1) + ",";

    json += "\"humidity\":" +
        String(airHumidity, 0) + ",";

    json += "\"wifiRssi\":" +
        String(WiFi.RSSI());

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

// ======================================================
// API DE CONFIGURACIÓN DE PH
// ======================================================

void handleGetConfig()
{
    String json = "{";

    json += "\"targetPh\":" +
        String(config.targetPh, 2) + ",";

    json += "\"tolerance\":" +
        String(config.phTolerance, 2) + ",";

    json += "\"doseSeconds\":" +
        String(
            config.doseDurationMs / 1000.0f,
            1
        ) + ",";

    json += "\"intervalMinutes\":" +
        String(config.doseIntervalMinutes) + ",";

    json += "\"maxDoses\":" +
        String(config.maxConsecutiveDoses) + ",";

    json += "\"automaticMode\":";
    json += config.automaticMode
        ? "true"
        : "false";

    json += ",\"targetEc\":" +
        String(config.targetEc, 2);

    json += ",\"lightOn\":\"";
    json += formatTime(
        config.lightOnHour,
        config.lightOnMinute
    );
    json += "\"";

    json += ",\"lightOff\":\"";
    json += formatTime(
        config.lightOffHour,
        config.lightOffMinute
    );
    json += "\"";

    json += ",\"activeProfileId\":" +
        String(static_cast<int>(getActiveProfileSlot()));

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
            "{\"success\":false,"
            "\"message\":\"Faltan datos\"}"
        );

        return;
    }

    float targetPh =
        server.arg("targetPh").toFloat();

    float tolerance =
        server.arg("tolerance").toFloat();

    float doseSeconds =
        server.arg("doseSeconds").toFloat();

    int intervalMinutes =
        server.arg("intervalMinutes").toInt();

    int maxDoses =
        server.arg("maxDoses").toInt();

    bool automaticMode =
        server.arg("automaticMode") == "true";

    if (
        targetPh < 4.0f ||
        targetPh > 8.0f ||
        tolerance < 0.01f ||
        tolerance > 1.0f ||
        doseSeconds < 0.1f ||
        doseSeconds > 30.0f ||
        intervalMinutes < 1 ||
        intervalMinutes > 120 ||
        maxDoses < 1 ||
        maxDoses > 10
    )
    {
        server.send(
            400,
            "application/json",
            "{\"success\":false,"
            "\"message\":\"Valores fuera de rango\"}"
        );

        return;
    }

    config.targetPh = targetPh;
    config.phTolerance = tolerance;

    config.doseDurationMs =
        static_cast<uint32_t>(
            doseSeconds * 1000.0f
        );

    config.doseIntervalMinutes =
        static_cast<uint32_t>(
            intervalMinutes
        );

    config.maxConsecutiveDoses =
        static_cast<uint8_t>(
            maxDoses
        );

    config.automaticMode = automaticMode;

    saveConfig();

    // Una modificación manual deja de coincidir con el perfil
    // que estaba aplicado anteriormente.
    setActiveProfileSlot(-1);

    server.send(
        200,
        "application/json",
        "{\"success\":true,"
        "\"message\":\"Configuración guardada\"}"
    );
}

// ======================================================
// API DE SISTEMA Y DIAGNÓSTICO
// ======================================================

void handleGetSystemStatus()
{
    const bool routerConnected =
        !wifiSetupMode &&
        !localAccessMode &&
        WiFi.status() == WL_CONNECTED;

    String currentSsid;
    String currentIp;
    String wifiState;

    if (wifiSetupMode)
    {
        currentSsid = SETUP_WIFI_NAME;
        currentIp = WiFi.softAPIP().toString();
        wifiState = "Punto de acceso de configuración activo";
    }
    else if (localAccessMode)
    {
        currentSsid = LOCAL_WIFI_NAME;
        currentIp = WiFi.softAPIP().toString();
        wifiState = "Punto de acceso local activo";
    }
    else if (routerConnected)
    {
        currentSsid = WiFi.SSID();
        currentIp = WiFi.localIP().toString();
        wifiState = "Conectado";
    }
    else
    {
        currentSsid = loadWifiSsid();
        currentIp = "";
        wifiState = "Desconectado";
    }

    const size_t littleFsTotal = LittleFS.totalBytes();
    const size_t littleFsUsed = LittleFS.usedBytes();
    const size_t littleFsAvailable =
        littleFsTotal >= littleFsUsed
            ? littleFsTotal - littleFsUsed
            : 0;

    const uint64_t uptimeSeconds =
        static_cast<uint64_t>(esp_timer_get_time()) /
        1000000ULL;

    char uptimeBuffer[24];

    snprintf(
        uptimeBuffer,
        sizeof(uptimeBuffer),
        "%llu",
        static_cast<unsigned long long>(uptimeSeconds)
    );

    String json;
    json.reserve(950);

    json += "{";

    json += "\"uptimeSeconds\":";
    json += uptimeBuffer;

    json += ",\"memory\":{";
    json += "\"freeHeap\":" +
        String(static_cast<unsigned long>(ESP.getFreeHeap()));
    json += ",\"minimumFreeHeap\":" +
        String(static_cast<unsigned long>(ESP.getMinFreeHeap()));
    json += "}";

    json += ",\"flash\":{";
    json += "\"total\":" +
        String(static_cast<unsigned long>(ESP.getFlashChipSize()));
    json += "}";

    json += ",\"littlefs\":{";
    json += "\"total\":" +
        String(static_cast<unsigned long>(littleFsTotal));
    json += ",\"used\":" +
        String(static_cast<unsigned long>(littleFsUsed));
    json += ",\"available\":" +
        String(static_cast<unsigned long>(littleFsAvailable));
    json += "}";

    json += ",\"mode\":{";
    json += "\"code\":\"";
    json += getSystemModeCode();
    json += "\",\"label\":\"";
    json += escapeJson(getSystemModeLabel());
    json += "\"}";

    json += ",\"wifi\":{";
    json += "\"state\":\"";
    json += escapeJson(wifiState);
    json += "\",\"routerConnected\":";
    json += routerConnected ? "true" : "false";
    json += ",\"ssid\":\"";
    json += escapeJson(currentSsid);
    json += "\",\"ip\":\"";
    json += escapeJson(currentIp);
    json += "\",\"rssi\":";

    if (routerConnected)
    {
        json += String(WiFi.RSSI());
    }
    else
    {
        json += "null";
    }

    json += "}";

    json += ",\"mdns\":{";
    json += "\"url\":\"http://hydrocontrol.local\",";
    json += "\"available\":";
    json += routerConnected ? "true" : "false";
    json += "}";

    json += ",\"firmware\":{";
    json += "\"version\":\"";
    json += escapeJson(FIRMWARE_VERSION);
    json += "\",\"compiledAt\":\"";
    json += escapeJson(String(__DATE__) + " " + String(__TIME__));
    json += "\"}";

    json += ",\"reset\":{";
    json += "\"code\":" +
        String(static_cast<int>(esp_reset_reason()));
    json += ",\"reason\":\"";
    json += escapeJson(getResetReasonText());
    json += "\"}";

    json += "}";

    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );
}

void handleRestartSystem()
{
    server.send(
        200,
        "application/json; charset=utf-8",
        "{\"success\":true,"
        "\"message\":\"HydroControl se reiniciará en unos segundos.\"}"
    );

    restartPending = true;
    restartRequestedAt = millis();
}

// ======================================================
// API DE WIFI
// ======================================================

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
    clearWifiCredentials();

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
// REGISTRO DE RUTAS
// ======================================================

void registerServerRoutes()
{
    server.on(
        "/api/status",
        HTTP_GET,
        handleApiStatus
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

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println(
        "================================="
    );

    Serial.println(
        "       HYDRO CONTROL PRO"
    );

    Serial.println(
        "================================="
    );

    loadConfig();

    Serial.println(
        "Configuración cargada:"
    );

    Serial.printf(
        "Objetivo pH: %.2f\n",
        config.targetPh
    );

    Serial.printf(
        "Tolerancia: %.2f\n",
        config.phTolerance
    );

    Serial.printf(
        "Duración dosis: %lu ms\n",
        static_cast<unsigned long>(
            config.doseDurationMs
        )
    );

    Serial.printf(
        "Intervalo: %lu minutos\n",
        static_cast<unsigned long>(
            config.doseIntervalMinutes
        )
    );

    Serial.printf(
        "Máximo de dosis: %u\n",
        config.maxConsecutiveDoses
    );

    if (!LittleFS.begin())
    {
        Serial.println(
            "Error al iniciar LittleFS."
        );

        while (true)
        {
            delay(1000);
        }
    }

    Serial.println(
        "LittleFS iniciado correctamente."
    );

    registerServerRoutes();

    if (loadLocalAccessMode())
    {
        startLocalAccessMode();
    }
    else if (!connectToSavedWifi())
    {
        startWifiSetupMode();
    }

    server.begin();

Serial.println(
    "Servidor web iniciado."
);

if (wifiSetupMode)
{
    Serial.println(
        "Conectate a HydroControl-Setup"
    );

    Serial.println(
        "Abrí: http://192.168.4.1"
    );
}
else if (localAccessMode)
{
    Serial.println(
        "Conectate a la red HydroControl"
    );

    Serial.println(
        "Contraseña: hydrocontrol"
    );

    Serial.println(
        "Abrí: http://192.168.4.1"
    );
}
else
{
    startMdns();

    Serial.print(
        "IP actual: http://"
    );

    Serial.println(
        WiFi.localIP()
    );

    Serial.println(
        "Nombre permanente: http://hydrocontrol.local"
    );
}
}

// ======================================================
// LOOP
// ======================================================

void loop()
{
    if (wifiSetupMode || localAccessMode)
    {
        dnsServer.processNextRequest();
    }

    server.handleClient();

    if (
        restartPending &&
        millis() - restartRequestedAt >= RESTART_DELAY_MS
    )
    {
        ESP.restart();
    }

    delay(2);
}