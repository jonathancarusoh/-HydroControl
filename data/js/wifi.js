function showWifiMessage(elementId, message, success) {
    const element = document.getElementById(elementId);

    if (!element) {
        return;
    }

    element.textContent = message;

    element.classList.remove(
        "d-none",
        "alert-success",
        "alert-danger",
        "alert-info"
    );

    element.classList.add(
        success ? "alert-success" : "alert-danger"
    );
}

function getWifiSignalLabel(rssi) {
    if (rssi >= -50) {
        return `${rssi} dBm · Excelente`;
    }

    if (rssi >= -60) {
        return `${rssi} dBm · Buena`;
    }

    if (rssi >= -70) {
        return `${rssi} dBm · Regular`;
    }

    return `${rssi} dBm · Débil`;
}

async function loadWifiStatus() {
    try {
        const response = await fetch("/api/wifi/status", {
            cache: "no-store"
        });

        if (!response.ok) {
            throw new Error(`Error HTTP: ${response.status}`);
        }

        const data = await response.json();

        const statusElement =
            document.getElementById("wifiConnectionStatus");

        const ssidElement =
            document.getElementById("wifiCurrentSsid");

        const ipElement =
            document.getElementById("wifiCurrentIp");

        const signalElement =
            document.getElementById("wifiSignal");

        if (statusElement) {
            if (data.connected) {
                statusElement.textContent = "Conectado";
                statusElement.classList.remove("text-danger");
                statusElement.classList.add("text-success");
            } else if (data.setupMode) {
                statusElement.textContent = "Modo configuración";
                statusElement.classList.remove("text-danger");
                statusElement.classList.add("text-warning");
            } else {
                statusElement.textContent = "Desconectado";
                statusElement.classList.remove("text-success");
                statusElement.classList.add("text-danger");
            }
        }

        if (ssidElement) {
            ssidElement.textContent =
                data.ssid || "Sin red guardada";
        }

        if (ipElement) {
            ipElement.textContent =
                data.ip || "---";
        }

        if (signalElement) {
            signalElement.textContent =
                data.connected
                    ? getWifiSignalLabel(data.rssi)
                    : "---";
        }

    } catch (error) {
        console.error(
            "Error cargando el estado WiFi:",
            error
        );

        const statusElement =
            document.getElementById("wifiConnectionStatus");

        if (statusElement) {
            statusElement.textContent = "Error de conexión";
            statusElement.classList.add("text-danger");
        }
    }
}

function createWifiNetworkButton(network) {
    const button = document.createElement("button");

    button.type = "button";

    button.className =
        "list-group-item list-group-item-action " +
        "bg-dark text-white border-secondary";

    const wrapper = document.createElement("div");

    wrapper.className =
        "d-flex justify-content-between align-items-center";

    const networkInfo = document.createElement("div");

    const networkName = document.createElement("div");

    networkName.className = "fw-bold";
    networkName.textContent =
        network.ssid || "Red sin nombre";

    const networkDetails = document.createElement("small");

    networkDetails.className = "text-secondary";

    networkDetails.textContent =
        `${network.rssi} dBm · ` +
        `${network.secure ? "Protegida" : "Abierta"}`;

    networkInfo.appendChild(networkName);
    networkInfo.appendChild(networkDetails);

    const icon = document.createElement("i");

    icon.className = network.secure
        ? "bi bi-lock-fill"
        : "bi bi-unlock-fill";

    wrapper.appendChild(networkInfo);
    wrapper.appendChild(icon);

    button.appendChild(wrapper);

    button.addEventListener("click", () => {
        const ssidInput =
            document.getElementById("wifiSsid");

        const passwordInput =
            document.getElementById("wifiPassword");

        if (ssidInput) {
            ssidInput.value = network.ssid;
        }

        if (passwordInput) {
            passwordInput.value = "";
            passwordInput.focus();
        }

        document
            .querySelectorAll("#wifiNetworkList button")
            .forEach(item => {
                item.classList.remove(
                    "active",
                    "border-success"
                );
            });

        button.classList.add(
            "active",
            "border-success"
        );
    });

    return button;
}

async function scanWifiNetworks() {
    const scanButton =
        document.getElementById("scanWifiButton");

    const networkList =
        document.getElementById("wifiNetworkList");

    const scanMessage =
        document.getElementById("wifiScanMessage");

    if (!networkList) {
        return;
    }

    if (scanButton) {
        scanButton.disabled = true;

        scanButton.innerHTML = `
            <span
                class="spinner-border spinner-border-sm me-2">
            </span>
            Buscando...
        `;
    }

    if (scanMessage) {
        scanMessage.textContent =
            "Buscando redes WiFi cercanas...";

        scanMessage.classList.remove(
            "d-none",
            "alert-danger"
        );

        scanMessage.classList.add("alert-info");
    }

    networkList.innerHTML = "";

    try {
        const response = await fetch("/api/wifi/scan", {
            cache: "no-store"
        });

        if (!response.ok) {
            throw new Error(`Error HTTP: ${response.status}`);
        }

        const networks = await response.json();

        if (!Array.isArray(networks)) {
            throw new Error("Respuesta WiFi inválida");
        }

        const uniqueNetworks = [];

        const seenSsids = new Set();

        networks
            .sort((a, b) => b.rssi - a.rssi)
            .forEach(network => {
                const ssid = String(network.ssid || "").trim();

                if (!ssid || seenSsids.has(ssid)) {
                    return;
                }

                seenSsids.add(ssid);

                uniqueNetworks.push({
                    ssid,
                    rssi: Number(network.rssi),
                    secure: Boolean(network.secure)
                });
            });

        if (uniqueNetworks.length === 0) {
            networkList.innerHTML = `
                <div class="text-secondary py-3">
                    No se encontraron redes WiFi.
                </div>
            `;
        } else {
            uniqueNetworks.forEach(network => {
                networkList.appendChild(
                    createWifiNetworkButton(network)
                );
            });
        }

        if (scanMessage) {
            scanMessage.textContent =
                `Se encontraron ${uniqueNetworks.length} redes.`;

            scanMessage.classList.remove(
                "alert-danger"
            );

            scanMessage.classList.add("alert-info");
        }

    } catch (error) {
        console.error(
            "Error buscando redes WiFi:",
            error
        );

        networkList.innerHTML = `
            <div class="text-danger py-3">
                No se pudieron buscar redes WiFi.
            </div>
        `;

        if (scanMessage) {
            scanMessage.textContent =
                "No se pudo completar la búsqueda.";

            scanMessage.classList.remove(
                "alert-info"
            );

            scanMessage.classList.add(
                "alert-danger"
            );
        }

    } finally {
        if (scanButton) {
            scanButton.disabled = false;

            scanButton.innerHTML = `
                <i class="bi bi-arrow-clockwise"></i>
                Buscar redes
            `;
        }
    }
}

async function saveWifiSettings() {
    const ssidInput =
        document.getElementById("wifiSsid");

    const passwordInput =
        document.getElementById("wifiPassword");

    const saveButton =
        document.getElementById("saveWifiButton");

    const ssid = ssidInput?.value.trim() || "";
    const password = passwordInput?.value || "";

    if (!ssid) {
        showWifiMessage(
            "wifiSaveMessage",
            "Ingresá o seleccioná una red WiFi.",
            false
        );

        return;
    }

    if (saveButton) {
        saveButton.disabled = true;

        saveButton.innerHTML = `
            <span
                class="spinner-border spinner-border-sm me-2">
            </span>
            Guardando...
        `;
    }

    const data = new URLSearchParams({
        ssid,
        password
    });

    try {
        const response = await fetch("/api/wifi/save", {
            method: "POST",

            headers: {
                "Content-Type":
                    "application/x-www-form-urlencoded"
            },

            body: data.toString()
        });

        const result = await response.json();

        if (!response.ok) {
            throw new Error(
                result.message ||
                "No se pudo guardar el WiFi"
            );
        }

        showWifiMessage(
            "wifiSaveMessage",
            result.message,
            true
        );

    } catch (error) {
        console.error(
            "Error guardando WiFi:",
            error
        );

        showWifiMessage(
            "wifiSaveMessage",
            error.message,
            false
        );

        if (saveButton) {
            saveButton.disabled = false;

            saveButton.innerHTML = `
                <i class="bi bi-check-circle"></i>
                Guardar y conectar
            `;
        }
    }
}

async function resetWifiSettings() {
    const confirmed = window.confirm(
        "¿Seguro que querés borrar la configuración WiFi?\n\n" +
        "HydroControl se reiniciará y creará la red " +
        "HydroControl-Setup."
    );

    if (!confirmed) {
        return;
    }

    const resetButton =
        document.getElementById("resetWifiButton");

    if (resetButton) {
        resetButton.disabled = true;

        resetButton.innerHTML = `
            <span
                class="spinner-border spinner-border-sm me-2">
            </span>
            Borrando...
        `;
    }

    try {
        const response = await fetch("/api/wifi/reset", {
            method: "POST"
        });

        const result = await response.json();

        if (!response.ok) {
            throw new Error(
                result.message ||
                "No se pudieron borrar las credenciales"
            );
        }

        showWifiMessage(
            "wifiResetMessage",
            result.message,
            true
        );

    } catch (error) {
        console.error(
            "Error borrando credenciales WiFi:",
            error
        );

        showWifiMessage(
            "wifiResetMessage",
            error.message,
            false
        );

        if (resetButton) {
            resetButton.disabled = false;

            resetButton.innerHTML = `
                <i class="bi bi-trash"></i>
                Borrar credenciales WiFi
            `;
        }
    }
}

function toggleWifiPasswordVisibility() {
    const passwordInput =
        document.getElementById("wifiPassword");

    const passwordIcon =
        document.getElementById("wifiPasswordIcon");

    if (!passwordInput) {
        return;
    }

    const isHidden =
        passwordInput.type === "password";

    passwordInput.type =
        isHidden ? "text" : "password";

    if (passwordIcon) {
        passwordIcon.className = isHidden
            ? "bi bi-eye-slash"
            : "bi bi-eye";
    }
}

function updateWifiPage() {
    loadWifiStatus();

    document
        .getElementById("scanWifiButton")
        ?.addEventListener(
            "click",
            scanWifiNetworks
        );

    document
        .getElementById("saveWifiButton")
        ?.addEventListener(
            "click",
            saveWifiSettings
        );

    document
        .getElementById("resetWifiButton")
        ?.addEventListener(
            "click",
            resetWifiSettings
        );

    document
        .getElementById("toggleWifiPasswordButton")
        ?.addEventListener(
            "click",
            toggleWifiPasswordVisibility
        );
}