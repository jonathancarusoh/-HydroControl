#include "wifi_portal.h"
#include "app_state.h"

// ======================================================
// PÁGINA DEL PORTAL DE CONFIGURACIÓN WIFI
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
