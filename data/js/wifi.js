const wifiPageState = {
    status: null,
    selectedNetwork: null,
    useSavedCredentials: false,
    forceNewPassword: false
};

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

async function readJsonResponse(response) {
    const responseText = await response.text();

    try {
        return JSON.parse(responseText);
    } catch (error) {
        console.error("Respuesta no JSON del ESP32:", responseText);

        throw new Error(
            response.status === 404
                ? "La función solicitada todavía no está cargada en el firmware del ESP32."
                : "El ESP32 devolvió una respuesta inválida."
        );
    }
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

function updateDisconnectCard(data) {
    const card = document.getElementById("wifiDisconnectCard");
    const description = document.getElementById("wifiDisconnectDescription");

    if (!card) {
        return;
    }

    const canDisconnect =
        data.connected &&
        !data.localMode &&
        !data.setupMode;

    card.classList.toggle("d-none", !canDisconnect);

    if (canDisconnect && description) {
        description.textContent =
            `HydroControl está conectado a "${data.ssid}". ` +
            "Podés pasar al modo local sin borrar la contraseña guardada.";
    }
}

async function loadWifiStatus() {
    try {
        const response = await fetch("/api/wifi/status", {
            cache: "no-store"
        });

        if (!response.ok) {
            throw new Error(`Error HTTP: ${response.status}`);
        }

        const data = await readJsonResponse(response);
        wifiPageState.status = data;

        const statusElement =
            document.getElementById("wifiConnectionStatus");
        const ssidElement =
            document.getElementById("wifiCurrentSsid");
        const ipElement =
            document.getElementById("wifiCurrentIp");
        const signalElement =
            document.getElementById("wifiSignal");

        if (statusElement) {
            statusElement.classList.remove(
                "text-success",
                "text-warning",
                "text-info",
                "text-danger"
            );

            if (data.localMode) {
                statusElement.textContent = "Modo local";
                statusElement.classList.add("text-info");
            } else if (data.connected) {
                statusElement.textContent = "Conectado";
                statusElement.classList.add("text-success");
            } else if (data.setupMode) {
                statusElement.textContent = "Modo configuración";
                statusElement.classList.add("text-warning");
            } else {
                statusElement.textContent = "Desconectado";
                statusElement.classList.add("text-danger");
            }
        }

        if (ssidElement) {
            ssidElement.textContent = data.ssid || "Sin red activa";
        }

        if (ipElement) {
            ipElement.textContent = data.ip || "---";
        }

        if (signalElement) {
            signalElement.textContent = data.localMode
                ? "Conexión directa al ESP32"
                : data.connected
                    ? getWifiSignalLabel(Number(data.rssi))
                    : "---";
        }

        updateDisconnectCard(data);
        return data;

    } catch (error) {
        console.error("Error cargando el estado WiFi:", error);

        const statusElement =
            document.getElementById("wifiConnectionStatus");

        if (statusElement) {
            statusElement.textContent = "Error de conexión";
            statusElement.classList.add("text-danger");
        }

        return null;
    }
}

function resetSelectedNetwork() {
    wifiPageState.selectedNetwork = null;
    wifiPageState.useSavedCredentials = false;
    wifiPageState.forceNewPassword = false;

    const ssidInput = document.getElementById("wifiSsid");
    const passwordInput = document.getElementById("wifiPassword");
    const passwordGroup = document.getElementById("wifiPasswordGroup");
    const hint = document.getElementById("wifiCredentialHint");
    const changePasswordButton =
        document.getElementById("changeSavedPasswordButton");
    const deletePasswordButton =
        document.getElementById("deleteSavedPasswordButton");
    const saveButton = document.getElementById("saveWifiButton");

    if (ssidInput) {
        ssidInput.value = "";
    }

    if (passwordInput) {
        passwordInput.value = "";
        passwordInput.required = false;
    }

    passwordGroup?.classList.remove("d-none");
    changePasswordButton?.classList.add("d-none");
    deletePasswordButton?.classList.add("d-none");

    if (hint) {
        hint.className = "wifi-credential-hint";
        hint.textContent = "Seleccioná una red para continuar.";
    }

    if (saveButton) {
        saveButton.disabled = true;
        saveButton.innerHTML = `
            <i class="bi bi-check-circle"></i>
            Conectar
        `;
    }
}

function configureSelectedNetwork(network) {
    wifiPageState.selectedNetwork = network;
    wifiPageState.forceNewPassword = false;

    const status = wifiPageState.status || {};
    const savedSsid = String(status.savedSsid || "").trim();
    const isSavedNetwork =
        Boolean(status.hasSavedCredentials) &&
        network.ssid === savedSsid;
    const hasSavedPassword =
        isSavedNetwork &&
        Boolean(status.hasSavedPassword);

    wifiPageState.useSavedCredentials = hasSavedPassword;

    const ssidInput = document.getElementById("wifiSsid");
    const passwordInput = document.getElementById("wifiPassword");
    const passwordGroup = document.getElementById("wifiPasswordGroup");
    const hint = document.getElementById("wifiCredentialHint");
    const changePasswordButton =
        document.getElementById("changeSavedPasswordButton");
    const deletePasswordButton =
        document.getElementById("deleteSavedPasswordButton");
    const saveButton = document.getElementById("saveWifiButton");

    if (ssidInput) {
        ssidInput.value = network.ssid;
    }

    if (passwordInput) {
        passwordInput.value = "";
        passwordInput.required = network.secure && !hasSavedPassword;
    }

    if (hasSavedPassword) {
        passwordGroup?.classList.add("d-none");
        changePasswordButton?.classList.remove("d-none");
        deletePasswordButton?.classList.remove("d-none");

        if (hint) {
            hint.className = "wifi-credential-hint saved";
            hint.innerHTML = `
                <i class="bi bi-shield-check"></i>
                La contraseña de <strong>${escapeHtml(network.ssid)}</strong>
                ya está guardada en HydroControl.
            `;
        }

        if (saveButton) {
            saveButton.disabled = false;
            saveButton.innerHTML = `
                <i class="bi bi-lightning-charge"></i>
                Conectar con contraseña guardada
            `;
        }

        return;
    }

    changePasswordButton?.classList.add("d-none");
    deletePasswordButton?.classList.add("d-none");

    if (network.secure) {
        passwordGroup?.classList.remove("d-none");

        if (hint) {
            hint.className = "wifi-credential-hint";
            hint.textContent = isSavedNetwork
                ? "El nombre de esta red sigue guardado, pero la contraseña fue eliminada. Ingresá una nueva contraseña para conectar."
                : "Ingresá la contraseña de la red seleccionada.";
        }

        passwordInput?.focus();
    } else {
        passwordGroup?.classList.add("d-none");

        if (hint) {
            hint.className = "wifi-credential-hint open";
            hint.innerHTML = `
                <i class="bi bi-unlock"></i>
                Esta red es abierta y no necesita contraseña.
            `;
        }
    }

    if (saveButton) {
        saveButton.disabled = false;
        saveButton.innerHTML = `
            <i class="bi bi-check-circle"></i>
            Guardar y conectar
        `;
    }
}

function escapeHtml(value) {
    const element = document.createElement("div");
    element.textContent = String(value || "");
    return element.innerHTML;
}

function createWifiNetworkButton(network) {
    const button = document.createElement("button");
    const status = wifiPageState.status || {};
    const savedSsid = String(status.savedSsid || "").trim();
    const isSavedNetwork =
        Boolean(status.hasSavedCredentials) &&
        network.ssid === savedSsid;

    button.type = "button";
    button.className = "wifi-network-item";

    const main = document.createElement("div");
    main.className = "wifi-network-main";

    const signalIcon = document.createElement("span");
    signalIcon.className = "wifi-network-signal";
    signalIcon.innerHTML = '<i class="bi bi-wifi"></i>';

    const info = document.createElement("div");
    info.className = "wifi-network-info";

    const nameRow = document.createElement("div");
    nameRow.className = "wifi-network-name-row";

    const name = document.createElement("span");
    name.className = "wifi-network-name";
    name.textContent = network.ssid;
    nameRow.appendChild(name);

    if (isSavedNetwork) {
        const savedBadge = document.createElement("span");
        const hasSavedPassword = Boolean(status.hasSavedPassword);

        savedBadge.className = "wifi-saved-badge";
        savedBadge.innerHTML = hasSavedPassword
            ? `
                <i class="bi bi-shield-check"></i>
                Guardada
            `
            : `
                <i class="bi bi-bookmark-check"></i>
                Red conocida
            `;

        nameRow.appendChild(savedBadge);
    }

    const details = document.createElement("small");
    details.className = "wifi-network-details";
    details.textContent =
        `${getWifiSignalLabel(network.rssi)} · ` +
        `${network.secure ? "Protegida" : "Abierta"}`;

    info.appendChild(nameRow);
    info.appendChild(details);

    const securityIcon = document.createElement("span");
    securityIcon.className = "wifi-network-security";
    securityIcon.innerHTML = network.secure
        ? '<i class="bi bi-lock-fill"></i>'
        : '<i class="bi bi-unlock"></i>';

    main.appendChild(signalIcon);
    main.appendChild(info);
    button.appendChild(main);
    button.appendChild(securityIcon);

    button.addEventListener("click", () => {
        document
            .querySelectorAll(".wifi-network-item")
            .forEach(item => item.classList.remove("selected"));

        button.classList.add("selected");
        configureSelectedNetwork(network);

        document.getElementById("wifiConnectCard")?.scrollIntoView({
            behavior: "smooth",
            block: "start"
        });
    });

    return button;
}

async function scanWifiNetworks() {
    const scanButton = document.getElementById("scanWifiButton");
    const networkList = document.getElementById("wifiNetworkList");
    const scanMessage = document.getElementById("wifiScanMessage");

    if (!networkList) {
        return;
    }

    resetSelectedNetwork();

    if (scanButton) {
        scanButton.disabled = true;
        scanButton.innerHTML = `
            <span class="spinner-border spinner-border-sm me-2"></span>
            Buscando...
        `;
    }

    if (scanMessage) {
        scanMessage.textContent = "Buscando redes WiFi cercanas...";
        scanMessage.classList.remove("d-none", "alert-danger");
        scanMessage.classList.add("alert-info");
    }

    networkList.innerHTML = `
        <div class="wifi-empty-state">
            <span class="spinner-border spinner-border-sm"></span>
            <span>Analizando redes cercanas...</span>
        </div>
    `;

    try {
        const response = await fetch("/api/wifi/scan", {
            cache: "no-store"
        });

        if (!response.ok) {
            throw new Error(`Error HTTP: ${response.status}`);
        }

        const networks = await readJsonResponse(response);

        if (!Array.isArray(networks)) {
            throw new Error("Respuesta WiFi inválida");
        }

        const seenSsids = new Set();
        const uniqueNetworks = networks
            .sort((a, b) => Number(b.rssi) - Number(a.rssi))
            .map(network => ({
                ssid: String(network.ssid || "").trim(),
                rssi: Number(network.rssi),
                secure: Boolean(network.secure)
            }))
            .filter(network => {
                if (!network.ssid || seenSsids.has(network.ssid)) {
                    return false;
                }

                seenSsids.add(network.ssid);
                return true;
            });

        networkList.innerHTML = "";

        if (uniqueNetworks.length === 0) {
            networkList.innerHTML = `
                <div class="wifi-empty-state">
                    <i class="bi bi-wifi-off"></i>
                    <span>No se encontraron redes WiFi.</span>
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
                `Se encontraron ${uniqueNetworks.length} redes. ` +
                "Seleccioná una para continuar.";
            scanMessage.classList.remove("alert-danger");
            scanMessage.classList.add("alert-info");
        }

    } catch (error) {
        console.error("Error buscando redes WiFi:", error);

        networkList.innerHTML = `
            <div class="wifi-empty-state error">
                <i class="bi bi-exclamation-triangle"></i>
                <span>No se pudieron buscar las redes WiFi.</span>
            </div>
        `;

        if (scanMessage) {
            scanMessage.textContent = "No se pudo completar la búsqueda.";
            scanMessage.classList.remove("alert-info");
            scanMessage.classList.add("alert-danger");
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

async function connectUsingSavedCredentials(ssid) {
    const data = new URLSearchParams({ ssid });

    const response = await fetch("/api/wifi/connect-saved", {
        method: "POST",
        headers: {
            "Content-Type": "application/x-www-form-urlencoded"
        },
        body: data.toString()
    });

    const result = await readJsonResponse(response);

    if (!response.ok) {
        throw new Error(
            result.message ||
            "No se pudo conectar usando la contraseña guardada."
        );
    }

    return result;
}

async function saveNewWifiCredentials(ssid, password) {
    const data = new URLSearchParams({ ssid, password });

    const response = await fetch("/api/wifi/save", {
        method: "POST",
        headers: {
            "Content-Type": "application/x-www-form-urlencoded"
        },
        body: data.toString()
    });

    const result = await readJsonResponse(response);

    if (!response.ok) {
        throw new Error(
            result.message ||
            "No se pudo guardar la configuración WiFi."
        );
    }

    return result;
}

async function saveWifiSettings() {
    const network = wifiPageState.selectedNetwork;
    const passwordInput = document.getElementById("wifiPassword");
    const saveButton = document.getElementById("saveWifiButton");

    if (!network) {
        showWifiMessage(
            "wifiSaveMessage",
            "Primero seleccioná una red de la lista.",
            false
        );
        return;
    }

    const password = passwordInput?.value || "";

    if (
        network.secure &&
        !wifiPageState.useSavedCredentials &&
        !password
    ) {
        showWifiMessage(
            "wifiSaveMessage",
            "Ingresá la contraseña de la red seleccionada.",
            false
        );
        passwordInput?.focus();
        return;
    }

    if (saveButton) {
        saveButton.disabled = true;
        saveButton.innerHTML = `
            <span class="spinner-border spinner-border-sm me-2"></span>
            Conectando...
        `;
    }

    try {
        const result = wifiPageState.useSavedCredentials
            ? await connectUsingSavedCredentials(network.ssid)
            : await saveNewWifiCredentials(network.ssid, password);

        showWifiMessage(
            "wifiSaveMessage",
            result.alreadyConnected
                ? result.message
                : `${result.message} Después conectate a "${network.ssid}" ` +
                  "y abrí hydrocontrol.local.",
            true
        );

        if (result.alreadyConnected && saveButton) {
            saveButton.disabled = false;
            saveButton.innerHTML = `
                <i class="bi bi-check-circle"></i>
                Ya conectado
            `;
        }

    } catch (error) {
        console.error("Error conectando el WiFi:", error);

        showWifiMessage(
            "wifiSaveMessage",
            error.message,
            false
        );

        if (saveButton) {
            saveButton.disabled = false;
            saveButton.innerHTML = wifiPageState.useSavedCredentials
                ? `
                    <i class="bi bi-lightning-charge"></i>
                    Conectar con contraseña guardada
                `
                : `
                    <i class="bi bi-check-circle"></i>
                    Guardar y conectar
                `;
        }
    }
}

function useNewPasswordForSavedNetwork() {
    const network = wifiPageState.selectedNetwork;

    if (!network) {
        return;
    }

    wifiPageState.useSavedCredentials = false;
    wifiPageState.forceNewPassword = true;

    const passwordGroup = document.getElementById("wifiPasswordGroup");
    const passwordInput = document.getElementById("wifiPassword");
    const hint = document.getElementById("wifiCredentialHint");
    const changePasswordButton =
        document.getElementById("changeSavedPasswordButton");
    const deletePasswordButton =
        document.getElementById("deleteSavedPasswordButton");
    const saveButton = document.getElementById("saveWifiButton");

    passwordGroup?.classList.remove("d-none");
    changePasswordButton?.classList.add("d-none");
    deletePasswordButton?.classList.add("d-none");

    if (passwordInput) {
        passwordInput.value = "";
        passwordInput.required = true;
        passwordInput.focus();
    }

    if (hint) {
        hint.className = "wifi-credential-hint";
        hint.textContent =
            "Ingresá la nueva contraseña. La anterior será reemplazada solamente si la conexión funciona.";
    }

    if (saveButton) {
        saveButton.innerHTML = `
            <i class="bi bi-check-circle"></i>
            Guardar nueva contraseña y conectar
        `;
    }
}

async function deleteSavedWifiPassword() {
    const network = wifiPageState.selectedNetwork;

    if (!network) {
        return;
    }

    const confirmed = window.confirm(
        `¿Querés eliminar la contraseña guardada de "${network.ssid}"?

` +
        "El nombre de la red seguirá guardado. Para volver a conectarte, " +
        "vas a tener que escribir una contraseña nueva."
    );

    if (!confirmed) {
        return;
    }

    const button = document.getElementById("deleteSavedPasswordButton");

    if (button) {
        button.disabled = true;
        button.innerHTML = `
            <span class="spinner-border spinner-border-sm me-2"></span>
            Eliminando...
        `;
    }

    try {
        const response = await fetch("/api/wifi/delete-password", {
            method: "POST"
        });
        const result = await readJsonResponse(response);

        if (!response.ok) {
            throw new Error(
                result.message ||
                "No se pudo eliminar la contraseña guardada."
            );
        }

        if (wifiPageState.status) {
            wifiPageState.status.hasSavedPassword = false;
        }

        wifiPageState.useSavedCredentials = false;
        wifiPageState.forceNewPassword = true;

        const passwordGroup = document.getElementById("wifiPasswordGroup");
        const passwordInput = document.getElementById("wifiPassword");
        const hint = document.getElementById("wifiCredentialHint");
        const changePasswordButton =
            document.getElementById("changeSavedPasswordButton");
        const saveButton = document.getElementById("saveWifiButton");

        button?.classList.add("d-none");
        changePasswordButton?.classList.add("d-none");
        passwordGroup?.classList.remove("d-none");

        if (passwordInput) {
            passwordInput.value = "";
            passwordInput.required = network.secure;
            passwordInput.focus();
        }

        if (hint) {
            hint.className = "wifi-credential-hint";
            hint.textContent =
                "La contraseña fue eliminada. Ingresá una nueva para volver a conectar esta red.";
        }

        if (saveButton) {
            saveButton.disabled = false;
            saveButton.innerHTML = `
                <i class="bi bi-check-circle"></i>
                Guardar nueva contraseña y conectar
            `;
        }

        document
            .querySelectorAll(".wifi-saved-badge")
            .forEach(badge => {
                badge.innerHTML = `
                    <i class="bi bi-bookmark-check"></i>
                    Red conocida
                `;
            });

        showWifiMessage(
            "wifiSaveMessage",
            result.message,
            true
        );

    } catch (error) {
        console.error("Error eliminando la contraseña WiFi:", error);

        showWifiMessage(
            "wifiSaveMessage",
            error.message,
            false
        );

        if (button) {
            button.disabled = false;
            button.innerHTML = `
                <i class="bi bi-key"></i>
                Eliminar contraseña guardada
            `;
        }
    }
}

async function disconnectWifiFromRouter() {
    const confirmed = window.confirm(
        "¿Querés desconectar HydroControl del router?\n\n" +
        "La red y la contraseña seguirán guardadas. " +
        "El ESP32 se reiniciará en modo local."
    );

    if (!confirmed) {
        return;
    }

    const button = document.getElementById("disconnectWifiButton");

    if (button) {
        button.disabled = true;
        button.innerHTML = `
            <span class="spinner-border spinner-border-sm me-2"></span>
            Desconectando...
        `;
    }

    try {
        const response = await fetch("/api/wifi/disconnect", {
            method: "POST"
        });
        const result = await readJsonResponse(response);

        if (!response.ok) {
            throw new Error(
                result.message ||
                "No se pudo desconectar la red."
            );
        }

        showWifiMessage(
            "wifiConnectionActionMessage",
            result.message +
                " En unos segundos conectate a la red HydroControl " +
                "y abrí 192.168.4.1.",
            true
        );

    } catch (error) {
        console.error("Error desconectando el WiFi:", error);

        showWifiMessage(
            "wifiConnectionActionMessage",
            error.message,
            false
        );

        if (button) {
            button.disabled = false;
            button.innerHTML = `
                <i class="bi bi-wifi-off"></i>
                Desconectar del router
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

    const resetButton = document.getElementById("resetWifiButton");

    if (resetButton) {
        resetButton.disabled = true;
        resetButton.innerHTML = `
            <span class="spinner-border spinner-border-sm me-2"></span>
            Borrando...
        `;
    }

    try {
        const response = await fetch("/api/wifi/reset", {
            method: "POST"
        });
        const result = await readJsonResponse(response);

        if (!response.ok) {
            throw new Error(
                result.message ||
                "No se pudieron borrar las credenciales."
            );
        }

        showWifiMessage(
            "wifiResetMessage",
            result.message,
            true
        );

    } catch (error) {
        console.error("Error borrando credenciales WiFi:", error);

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
    const passwordInput = document.getElementById("wifiPassword");
    const passwordIcon = document.getElementById("wifiPasswordIcon");

    if (!passwordInput) {
        return;
    }

    const isHidden = passwordInput.type === "password";
    passwordInput.type = isHidden ? "text" : "password";

    if (passwordIcon) {
        passwordIcon.className = isHidden
            ? "bi bi-eye-slash"
            : "bi bi-eye";
    }
}

async function updateWifiPage() {
    document
        .getElementById("scanWifiButton")
        ?.addEventListener("click", scanWifiNetworks);

    document
        .getElementById("saveWifiButton")
        ?.addEventListener("click", saveWifiSettings);

    document
        .getElementById("changeSavedPasswordButton")
        ?.addEventListener("click", useNewPasswordForSavedNetwork);

    document
        .getElementById("deleteSavedPasswordButton")
        ?.addEventListener("click", deleteSavedWifiPassword);

    document
        .getElementById("disconnectWifiButton")
        ?.addEventListener("click", disconnectWifiFromRouter);

    document
        .getElementById("resetWifiButton")
        ?.addEventListener("click", resetWifiSettings);

    document
        .getElementById("toggleWifiPasswordButton")
        ?.addEventListener("click", toggleWifiPasswordVisibility);

    resetSelectedNetwork();

    const status = await loadWifiStatus();

    // En modo local o de configuración el buscador se abre solo,
    // porque es justamente el camino para volver a una red cercana.
    if (status?.localMode || status?.setupMode) {
        scanWifiNetworks();
    }
}
