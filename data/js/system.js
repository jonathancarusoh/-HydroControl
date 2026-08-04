let systemRefreshTimer = null;
let systemRequestInProgress = false;

function formatSystemBytes(bytes) {
    const value = Number(bytes);

    if (!Number.isFinite(value) || value < 0) {
        return "---";
    }

    if (value >= 1024 * 1024) {
        return `${(value / (1024 * 1024)).toFixed(2)} MB`;
    }

    if (value >= 1024) {
        return `${(value / 1024).toFixed(1)} KB`;
    }

    return `${value} B`;
}

function formatSystemUptime(totalSeconds) {
    let seconds = Math.max(
        0,
        Math.floor(Number(totalSeconds) || 0)
    );

    const days = Math.floor(seconds / 86400);
    seconds %= 86400;

    const hours = Math.floor(seconds / 3600);
    seconds %= 3600;

    const minutes = Math.floor(seconds / 60);
    seconds %= 60;

    const parts = [];

    if (days > 0) {
        parts.push(`${days} d`);
    }

    if (hours > 0 || days > 0) {
        parts.push(`${hours} h`);
    }

    if (minutes > 0 || hours > 0 || days > 0) {
        parts.push(`${minutes} min`);
    }

    parts.push(`${seconds} s`);

    return parts.join(" ");
}

function getSystemSignalLabel(rssi) {
    if (rssi === null || rssi === undefined) {
        return "No aplica";
    }

    const value = Number(rssi);

    if (!Number.isFinite(value)) {
        return "---";
    }

    if (value >= -50) {
        return `${value} dBm · Excelente`;
    }

    if (value >= -60) {
        return `${value} dBm · Buena`;
    }

    if (value >= -70) {
        return `${value} dBm · Regular`;
    }

    return `${value} dBm · Débil`;
}

function setSystemText(elementId, value) {
    const element = document.getElementById(elementId);

    if (element) {
        element.textContent = value;
    }
}

function showSystemMessage(message, type = "info") {
    const element = document.getElementById("systemMessage");

    if (!element) {
        return;
    }

    element.textContent = message;

    element.classList.remove(
        "d-none",
        "alert-info",
        "alert-success",
        "alert-danger",
        "alert-warning"
    );

    element.classList.add(`alert-${type}`);
}

function updateSystemModeVisual(mode) {
    const icon = document.getElementById("systemModeIcon");
    const badge = document.getElementById("systemWifiState");

    if (!icon || !badge) {
        return;
    }

    icon.classList.remove(
        "router",
        "local",
        "setup"
    );

    badge.classList.remove(
        "router",
        "local",
        "setup"
    );

    const modeCode = mode?.code || "router";

    icon.classList.add(modeCode);
    badge.classList.add(modeCode);

    const iconClass = {
        router: "bi bi-router",
        local: "bi bi-broadcast-pin",
        setup: "bi bi-tools"
    }[modeCode] || "bi bi-cpu";

    icon.innerHTML = `<i class="${iconClass}"></i>`;
}

function renderSystemStatus(data) {
    setSystemText(
        "systemModeLabel",
        data.mode?.label || "---"
    );

    setSystemText(
        "systemWifiState",
        data.wifi?.state || "---"
    );

    updateSystemModeVisual(data.mode);

    const uptime = formatSystemUptime(
        data.uptimeSeconds
    );

    setSystemText("systemUptime", uptime);

    const version =
        data.firmware?.version || "---";

    setSystemText(
        "systemFirmwareVersion",
        version
    );

    setSystemText(
        "systemFirmwareVersionDetail",
        version
    );

    setSystemText(
        "systemFreeHeap",
        formatSystemBytes(data.memory?.freeHeap)
    );

    setSystemText(
        "systemMinFreeHeap",
        formatSystemBytes(
            data.memory?.minimumFreeHeap
        )
    );

    setSystemText(
        "systemFlashTotal",
        formatSystemBytes(data.flash?.total)
    );

    const littleFsTotal = Number(
        data.littlefs?.total || 0
    );

    const littleFsUsed = Number(
        data.littlefs?.used || 0
    );

    const littleFsAvailable = Number(
        data.littlefs?.available || 0
    );

    const littleFsPercent =
        littleFsTotal > 0
            ? Math.min(
                100,
                Math.max(
                    0,
                    (littleFsUsed / littleFsTotal) * 100
                )
            )
            : 0;

    setSystemText(
        "systemLittleFsSummary",
        `${formatSystemBytes(littleFsUsed)} usados`
    );

    setSystemText(
        "systemLittleFsPercent",
        `${littleFsPercent.toFixed(1)}%`
    );

    setSystemText(
        "systemLittleFsUsed",
        formatSystemBytes(littleFsUsed)
    );

    setSystemText(
        "systemLittleFsAvailable",
        formatSystemBytes(littleFsAvailable)
    );

    setSystemText(
        "systemLittleFsTotal",
        formatSystemBytes(littleFsTotal)
    );

    const littleFsProgress =
        document.getElementById(
            "systemLittleFsProgress"
        );

    if (littleFsProgress) {
        littleFsProgress.style.width =
            `${littleFsPercent}%`;
    }

    setSystemText(
        "systemWifiStatus",
        data.wifi?.state || "---"
    );

    setSystemText(
        "systemWifiSsid",
        data.wifi?.ssid || "Sin red"
    );

    setSystemText(
        "systemWifiIp",
        data.wifi?.ip || "No asignada"
    );

    setSystemText(
        "systemWifiSignal",
        getSystemSignalLabel(data.wifi?.rssi)
    );

    setSystemText(
        "systemMdnsUrl",
        data.mdns?.url ||
            "http://hydrocontrol.local"
    );

    const mdnsBadge =
        document.getElementById("systemMdnsBadge");

    if (mdnsBadge) {
        const available =
            Boolean(data.mdns?.available);

        mdnsBadge.textContent = available
            ? "Disponible"
            : "No disponible en este modo";

        mdnsBadge.classList.toggle(
            "available",
            available
        );
    }

    setSystemText(
        "systemCompiledAt",
        data.firmware?.compiledAt || "---"
    );

    setSystemText(
        "systemResetReason",
        data.reset?.reason || "---"
    );
}

async function loadSystemStatus(showErrors = true) {
    if (systemRequestInProgress) {
        return;
    }

    systemRequestInProgress = true;

    const refreshButton =
        document.getElementById("refreshSystemButton");

    try {
        const response = await fetch(
            "/api/system/status",
            {
                cache: "no-store"
            }
        );

        const responseText = await response.text();

        let data;

        try {
            data = JSON.parse(responseText);
        } catch {
            throw new Error(
                "El ESP32 devolvió una respuesta inválida."
            );
        }

        if (!response.ok) {
            throw new Error(
                data.message ||
                "No se pudo consultar el sistema."
            );
        }

        renderSystemStatus(data);

    } catch (error) {
        console.error(
            "Error cargando el estado del sistema:",
            error
        );

        if (showErrors) {
            showSystemMessage(
                error.message,
                "danger"
            );
        }

    } finally {
        systemRequestInProgress = false;

        if (refreshButton) {
            refreshButton.disabled = false;
            refreshButton.innerHTML = `
                <i class="bi bi-arrow-clockwise"></i>
                Actualizar
            `;
        }
    }
}

async function manuallyRefreshSystem() {
    const refreshButton =
        document.getElementById("refreshSystemButton");

    if (refreshButton) {
        refreshButton.disabled = true;
        refreshButton.innerHTML = `
            <span
                class="spinner-border spinner-border-sm me-2">
            </span>
            Actualizando...
        `;
    }

    await loadSystemStatus(true);
}

async function restartHydroControl() {
    const confirmed = window.confirm(
        "¿Reiniciar el ESP32 ahora?\n\n" +
        "No se borrarán perfiles, configuraciones ni WiFi."
    );

    if (!confirmed) {
        return;
    }

    const button =
        document.getElementById("restartSystemButton");

    if (button) {
        button.disabled = true;
        button.innerHTML = `
            <span
                class="spinner-border spinner-border-sm me-2">
            </span>
            Reiniciando...
        `;
    }

    try {
        const response = await fetch(
            "/api/system/restart",
            {
                method: "POST"
            }
        );

        const responseText = await response.text();

        let result;

        try {
            result = JSON.parse(responseText);
        } catch {
            throw new Error(
                "El ESP32 devolvió una respuesta inválida."
            );
        }

        if (!response.ok || !result.success) {
            throw new Error(
                result.message ||
                "No se pudo reiniciar HydroControl."
            );
        }

        showSystemMessage(
            result.message,
            "warning"
        );

        if (systemRefreshTimer) {
            clearInterval(systemRefreshTimer);
            systemRefreshTimer = null;
        }

        window.setTimeout(() => {
            window.location.reload();
        }, 8000);

    } catch (error) {
        console.error(
            "Error reiniciando HydroControl:",
            error
        );

        showSystemMessage(
            error.message,
            "danger"
        );

        if (button) {
            button.disabled = false;
            button.innerHTML = `
                <i class="bi bi-power"></i>
                Reiniciar ESP32
            `;
        }
    }
}

function stopSystemRefresh() {
    if (systemRefreshTimer) {
        clearInterval(systemRefreshTimer);
        systemRefreshTimer = null;
    }
}

function updateSystemPage() {
    stopSystemRefresh();

    document
        .getElementById("refreshSystemButton")
        ?.addEventListener(
            "click",
            manuallyRefreshSystem
        );

    document
        .getElementById("restartSystemButton")
        ?.addEventListener(
            "click",
            restartHydroControl
        );

    loadSystemStatus(true);

    systemRefreshTimer = window.setInterval(
        () => {
            if (!document.getElementById("systemPageRoot")) {
                stopSystemRefresh();
                return;
            }

            loadSystemStatus(false);
        },
        5000
    );
}
