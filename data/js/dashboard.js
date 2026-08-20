const dashboardTimeState = {
    baseEpoch: null,
    receivedAtMs: 0
};

const dashboardConfigCache = {
    data: null,
    fetchedAtMs: 0
};

const dashboardLightControlState = {
    requestInProgress: false,
    lastLightData: null
};

function dashboardFormatLongDate(date) {
    const formatted = new Intl.DateTimeFormat("es-AR", {
        weekday: "long",
        day: "numeric",
        month: "long",
        year: "numeric"
    }).format(date);

    return formatted.charAt(0).toUpperCase() + formatted.slice(1);
}

function dashboardFormatNumber(value, decimals, suffix = "") {
    const numericValue = Number(value);

    if (!Number.isFinite(numericValue)) {
        return "--";
    }

    return `${numericValue.toFixed(decimals)}${suffix}`;
}

function dashboardRenderActiveProfile(profileData) {
    const badge = document.getElementById("dashboardProfileBadge");
    const label = document.getElementById("dashboardProfileLabel");

    if (!badge || !label) {
        return;
    }

    if (!profileData) {
        badge.dataset.state = "unknown";
        badge.querySelector("i")?.classList.remove(
            "bi-bookmark-check",
            "bi-bookmarks"
        );
        badge.querySelector("i")?.classList.add("bi-exclamation-circle");
        label.textContent = "Perfil sin información";
        badge.title = "No se pudo consultar el perfil activo";
        return;
    }

    const active = Boolean(profileData.active);
    const profileName = String(profileData.name || "").trim();
    const icon = badge.querySelector("i");

    badge.dataset.state = active ? "active" : "none";
    icon?.classList.remove(
        "bi-bookmark-check",
        "bi-bookmarks",
        "bi-exclamation-circle"
    );
    icon?.classList.add(active ? "bi-bookmark-check" : "bi-bookmarks");

    label.textContent = active && profileName
        ? `Perfil: ${profileName}`
        : "Sin perfil activo";

    badge.title = active && profileName
        ? `Perfil activo: ${profileName}. Abrir perfiles.`
        : "La configuración actual no pertenece a un perfil. Abrir perfiles.";
}

async function dashboardReadJson(response) {
    const text = await response.text();

    try {
        return text ? JSON.parse(text) : {};
    } catch {
        throw new Error(
            "El ESP32 devolvió una respuesta inválida. Revisá que el firmware y la interfaz estén actualizados."
        );
    }
}

function dashboardTickClock() {
    const timeElement = document.getElementById("dashboardClock");
    const dateElement = document.getElementById("dashboardClockDate");

    if (!timeElement || !dateElement) {
        return;
    }

    if (!Number.isFinite(dashboardTimeState.baseEpoch)) {
        timeElement.textContent = "--:--:--";
        dateElement.textContent = "Hora no configurada";
        return;
    }

    const elapsedSeconds =
        (Date.now() - dashboardTimeState.receivedAtMs) / 1000;

    const date = new Date(
        (dashboardTimeState.baseEpoch + elapsedSeconds) * 1000
    );

    timeElement.textContent = date.toLocaleTimeString("es-AR", {
        hour: "2-digit",
        minute: "2-digit",
        second: "2-digit",
        hour12: false
    });

    dateElement.textContent = dashboardFormatLongDate(date);
}

function dashboardRenderClock(clockData) {
    if (!clockData?.configured || !Number.isFinite(Number(clockData.epoch))) {
        dashboardTimeState.baseEpoch = null;
        dashboardTimeState.receivedAtMs = 0;
        dashboardTickClock();
        return;
    }

    dashboardTimeState.baseEpoch = Number(clockData.epoch);
    dashboardTimeState.receivedAtMs = Date.now();
    dashboardTickClock();
}

function dashboardShowLightMessage(message, success = null) {
    const element = document.getElementById("dashboardLightMessage");

    if (!element) {
        return;
    }

    if (!message) {
        element.hidden = true;
        element.textContent = "";
        element.classList.remove("is-success", "is-error", "is-info");
        return;
    }

    element.hidden = false;
    element.textContent = message;
    element.classList.remove("is-success", "is-error", "is-info");

    if (success === true) {
        element.classList.add("is-success");
    } else if (success === false) {
        element.classList.add("is-error");
    } else {
        element.classList.add("is-info");
    }
}

function dashboardRenderLight(lightData) {
    const panel = document.getElementById("dashboardLightPanel");
    const stateElement = document.getElementById("dashboardLightState");
    const nextElement = document.getElementById("dashboardLightNext");
    const modeElement = document.getElementById("dashboardLightMode");
    const automaticSwitch = document.getElementById(
        "dashboardAutomaticLightSwitch"
    );
    const automaticLabel = document.getElementById(
        "dashboardAutomaticLightLabel"
    );
    const manualSwitch = document.getElementById(
        "dashboardManualLightSwitch"
    );
    const manualLabel = document.getElementById(
        "dashboardManualLightLabel"
    );
    const hintElement = document.getElementById(
        "dashboardLightControlHint"
    );

    if (
        !panel ||
        !stateElement ||
        !nextElement ||
        !modeElement ||
        !automaticSwitch ||
        !automaticLabel ||
        !manualSwitch ||
        !manualLabel ||
        !hintElement
    ) {
        return;
    }

    dashboardLightControlState.lastLightData = lightData || null;

    const automaticEnabled = Boolean(
        lightData?.automaticEnabled ?? lightData?.enabled
    );

    const manualOn = Boolean(lightData?.manualOn);

    const stateCode = lightData?.stateCode || "unknown";

    const effectiveOn = lightData?.effectiveOn !== undefined
        ? Boolean(lightData.effectiveOn)
        : stateCode === "on" || stateCode === "manual-on";

    const hasLightData = Boolean(lightData);
    const noControlActive =
        hasLightData &&
        !automaticEnabled &&
        !manualOn;

    const mode = !hasLightData
        ? "unknown"
        : automaticEnabled
            ? "automatic"
            : manualOn
                ? "manual"
                : "inactive";

    panel.dataset.mode = mode;
    panel.classList.toggle("is-automatic", automaticEnabled);
    panel.classList.toggle(
        "is-manual-on",
        !automaticEnabled && manualOn
    );
    panel.classList.toggle("is-inactive", noControlActive);

    const icon = effectiveOn
        ? "bi-lightbulb-fill"
        : "bi-lightbulb";

    stateElement.dataset.state = noControlActive
        ? "inactive"
        : stateCode;

    stateElement.innerHTML = `
        <i class="bi ${icon}"></i>
        ${lightData?.stateLabel || "Sin información"}
    `;

    if (noControlActive) {
        nextElement.textContent =
            "Sin programación automática ni encendido manual";
    } else {
        nextElement.textContent =
            lightData?.nextChangeLabel || "---";
    }

    if (!hasLightData) {
        modeElement.innerHTML =
            '<i class="bi bi-circle-fill"></i> Sin información';
    } else if (automaticEnabled) {
        modeElement.innerHTML =
            '<i class="bi bi-circle-fill"></i> Programación automática activa';
    } else if (manualOn) {
        modeElement.innerHTML =
            '<i class="bi bi-hand-index-thumb"></i> Control manual activo';
    } else {
        modeElement.innerHTML =
            '<i class="bi bi-exclamation-circle"></i> Sin control activo';
    }

    automaticSwitch.checked = automaticEnabled;
    automaticSwitch.disabled =
        dashboardLightControlState.requestInProgress ||
        !hasLightData;

    automaticLabel.textContent = automaticEnabled
        ? "Activado"
        : "Desactivado";

    // El control manual refleja únicamente el estado manual guardado.
    // No debe encenderse visualmente porque la lámpara esté encendida
    // por el horario automático.
    manualSwitch.checked = manualOn;
    manualSwitch.disabled =
        dashboardLightControlState.requestInProgress ||
        !hasLightData;

    manualLabel.textContent = manualOn
        ? "Encendido"
        : "Apagado";

    if (!hasLightData) {
        hintElement.textContent =
            "No se pudo consultar el control de iluminación.";
    } else if (automaticEnabled) {
        hintElement.textContent =
            "La lámpara sigue el horario configurado. Al activar el modo manual, el automático se desactiva.";
    } else if (manualOn) {
        hintElement.textContent =
            "La lámpara está bajo control manual. Podés apagarla desde esta llave o volver a activar el horario.";
    } else {
        hintElement.textContent =
            "Ambos controles están apagados. Activá el horario automático o encendé la lámpara manualmente.";
    }
}

async function dashboardSetAutomaticLight(requestedEnabled) {
    if (dashboardLightControlState.requestInProgress) {
        return;
    }

    const automaticSwitch = document.getElementById(
        "dashboardAutomaticLightSwitch"
    );
    const manualSwitch = document.getElementById(
        "dashboardManualLightSwitch"
    );

    dashboardLightControlState.requestInProgress = true;

    if (automaticSwitch) {
        automaticSwitch.disabled = true;
    }

    if (manualSwitch) {
        manualSwitch.disabled = true;
    }

    dashboardShowLightMessage(
        requestedEnabled
            ? "Activando programación automática..."
            : "Desactivando programación automática...",
        null
    );

    try {
        const body = new URLSearchParams({
            state: requestedEnabled ? "true" : "false"
        });

        const response = await fetch("/api/light/automatic", {
            method: "POST",
            headers: {
                "Content-Type": "application/x-www-form-urlencoded"
            },
            body: body.toString()
        });

        const result = await dashboardReadJson(response);

        if (!response.ok || result.success === false) {
            throw new Error(
                result.message ||
                "No se pudo cambiar la programación automática."
            );
        }

        dashboardShowLightMessage(result.message, true);
        dashboardConfigCache.fetchedAtMs = 0;

        await updateDashboard();
    } catch (error) {
        console.error(
            "Error cambiando la programación automática:",
            error
        );

        dashboardShowLightMessage(error.message, false);
        await updateDashboard();
    } finally {
        dashboardLightControlState.requestInProgress = false;

        if (automaticSwitch) {
            automaticSwitch.disabled = false;
        }

        if (manualSwitch) {
            manualSwitch.disabled = false;
        }
    }
}

async function dashboardSetManualLight(requestedOn) {
    if (dashboardLightControlState.requestInProgress) {
        return;
    }

    const automaticSwitch = document.getElementById(
        "dashboardAutomaticLightSwitch"
    );
    const manualSwitch = document.getElementById(
        "dashboardManualLightSwitch"
    );

    dashboardLightControlState.requestInProgress = true;

    if (automaticSwitch) {
        automaticSwitch.disabled = true;
    }

    if (manualSwitch) {
        manualSwitch.disabled = true;
    }

    dashboardShowLightMessage(
        requestedOn
            ? "Encendiendo control manual..."
            : "Apagando control manual...",
        null
    );

    try {
        const body = new URLSearchParams({
            state: requestedOn ? "true" : "false"
        });

        const response = await fetch("/api/light/manual", {
            method: "POST",
            headers: {
                "Content-Type": "application/x-www-form-urlencoded"
            },
            body: body.toString()
        });

        const result = await dashboardReadJson(response);

        if (!response.ok || result.success === false) {
            throw new Error(
                result.message || "No se pudo cambiar el control manual."
            );
        }

        dashboardShowLightMessage(
            result.automaticDisabled
                ? `${result.message} La programación automática fue desactivada.`
                : result.message,
            true
        );

        dashboardConfigCache.fetchedAtMs = 0;
        await updateDashboard();
    } catch (error) {
        console.error("Error cambiando la luz manual:", error);
        dashboardShowLightMessage(error.message, false);
        await updateDashboard();
    } finally {
        dashboardLightControlState.requestInProgress = false;

        if (automaticSwitch) {
            automaticSwitch.disabled = false;
        }

        if (manualSwitch) {
            manualSwitch.disabled = false;
        }
    }
}

async function dashboardGetConfig(forceRefresh = false) {
    const cacheAgeMs = Date.now() - dashboardConfigCache.fetchedAtMs;

    if (
        !forceRefresh &&
        dashboardConfigCache.data &&
        cacheAgeMs < 10000
    ) {
        return dashboardConfigCache.data;
    }

    const response = await fetch("/api/config", {
        cache: "no-store"
    });

    if (!response.ok) {
        throw new Error(`Error HTTP: ${response.status}`);
    }

    const data = await response.json();

    dashboardConfigCache.data = data;
    dashboardConfigCache.fetchedAtMs = Date.now();

    return data;
}

function dashboardRenderPhStatus(currentPh, configData) {
    const statusElement = document.getElementById("phStatus");

    if (!statusElement) {
        return;
    }

    const target = Number(configData?.targetPh);
    const tolerance = Number(configData?.tolerance);

    if (
        !Number.isFinite(currentPh) ||
        !Number.isFinite(target) ||
        !Number.isFinite(tolerance)
    ) {
        statusElement.dataset.state = "unknown";
        statusElement.textContent = "SIN DATOS";
        return;
    }

    if (currentPh < target - tolerance) {
        statusElement.dataset.state = "low";
        statusElement.textContent = "PH BAJO";
        return;
    }

    if (currentPh > target + tolerance) {
        statusElement.dataset.state = "high";
        statusElement.textContent = "PH ALTO";
        return;
    }

    statusElement.dataset.state = "stable";
    statusElement.textContent = "ESTABLE";
}

function dashboardRenderAutomaticConfig(configData) {
    const modeLabel = document.getElementById("dashboardAutoModeLabel");
    const modeState = document.getElementById("dashboardAutoModeState");
    const details = document.getElementById("dashboardAutoDetails");
    const disabledMessage = document.getElementById("dashboardAutoDisabledMessage");
    const disabledText = document.getElementById("dashboardAutoDisabledText");

    if (!modeLabel || !modeState || !details || !disabledMessage || !disabledText) {
        return;
    }

    if (!configData) {
        modeLabel.textContent = "Sin información";
        modeState.dataset.enabled = "unknown";
        modeState.innerHTML = '<i class="bi bi-circle-fill"></i> Sin datos';
        details.classList.add("d-none");
        disabledMessage.classList.remove("d-none");
        disabledText.textContent =
            "No se pudo cargar la configuración automática de pH.";
        return;
    }

    const automaticMode = Boolean(configData.automaticMode);
    const target = Number(configData.targetPh);
    const tolerance = Number(configData.tolerance);
    const doseSeconds = Number(configData.doseSeconds);
    const intervalMinutes = Number(configData.intervalMinutes);
    const maxDailyDoses = Number(
        configData.maxDailyDoses ?? configData.maxDoses
    );

    modeLabel.textContent = automaticMode
        ? "Regulación automática"
        : "Control manual";

    modeState.dataset.enabled = automaticMode ? "true" : "false";
    modeState.innerHTML = automaticMode
        ? '<i class="bi bi-circle-fill"></i> Activo'
        : '<i class="bi bi-circle-fill"></i> Desactivado';

    details.classList.toggle("d-none", !automaticMode);
    disabledMessage.classList.toggle("d-none", automaticMode);
    disabledText.textContent =
        "El control automático está desactivado. El sistema permanece en modo manual.";

    if (!automaticMode) {
        return;
    }

    const minPh = target - tolerance;
    const maxPh = target + tolerance;

    document.getElementById("dashboardTargetPh").textContent =
        dashboardFormatNumber(target, 2);

    document.getElementById("dashboardPhRange").textContent =
        Number.isFinite(minPh) && Number.isFinite(maxPh)
            ? `${minPh.toFixed(2)} – ${maxPh.toFixed(2)}`
            : "--";

    document.getElementById("dashboardDoseDuration").textContent =
        dashboardFormatNumber(doseSeconds, 1, " s");

    document.getElementById("dashboardDoseInterval").textContent =
        Number.isFinite(intervalMinutes)
            ? `${intervalMinutes} min`
            : "--";

    document.getElementById("dashboardMaxDoses").textContent =
        Number.isFinite(maxDailyDoses)
            ? `${maxDailyDoses} dosis / 24 h`
            : "--";
}

function dashboardRenderSecondaryDetails(statusData, configData) {
    const currentEc = Number(statusData?.ec);
    const targetEc = Number(configData?.targetEc);
    const difference = currentEc - targetEc;

    const ecCurrentElement = document.getElementById("dashboardEcCurrent");
    const targetEcElement = document.getElementById("dashboardTargetEc");
    const ecDifferenceElement = document.getElementById("dashboardEcDifference");
    const waterCurrentElement = document.getElementById("dashboardWaterCurrent");
    const humidityCurrentElement = document.getElementById("dashboardHumidityCurrent");

    if (ecCurrentElement) {
        ecCurrentElement.textContent = dashboardFormatNumber(currentEc, 2, " mS/cm");
    }

    if (targetEcElement) {
        targetEcElement.textContent = dashboardFormatNumber(targetEc, 2, " mS/cm");
    }

    if (ecDifferenceElement) {
        ecDifferenceElement.textContent =
            Number.isFinite(difference)
                ? `${difference >= 0 ? "+" : ""}${difference.toFixed(2)} mS/cm`
                : "--";
    }

    if (waterCurrentElement) {
        waterCurrentElement.textContent =
            dashboardFormatNumber(statusData?.waterTemp, 1, " °C");
    }

    if (humidityCurrentElement) {
        humidityCurrentElement.textContent =
            dashboardFormatNumber(statusData?.humidity, 0, " %");
    }
}

function dashboardRenderLastUpdate() {
    const element = document.getElementById("dashboardLastUpdate");

    if (!element) {
        return;
    }

    element.textContent = new Date().toLocaleTimeString("es-AR", {
        hour: "2-digit",
        minute: "2-digit",
        second: "2-digit",
        hour12: false
    });
}

function dashboardSetCardExpanded(card, expanded) {
    const toggle = card.querySelector(".dashboard-sensor-toggle");
    const details = card.querySelector(".dashboard-sensor-details");
    const detailLabel = card.querySelector(".dashboard-view-details");

    card.classList.toggle("is-expanded", expanded);

    if (toggle) {
        toggle.setAttribute("aria-expanded", expanded ? "true" : "false");
    }

    if (details) {
        details.hidden = !expanded;
    }

    if (detailLabel) {
        detailLabel.textContent = expanded ? "Ocultar detalles" : "Ver detalles";
    }
}

function dashboardScrollExpandedCardIntoView(card) {
    window.setTimeout(() => {
        if (!card?.isConnected || !card.classList.contains("is-expanded")) {
            return;
        }

        card.scrollIntoView({
            behavior: "smooth",
            block: "start",
            inline: "nearest"
        });
    }, 90);
}

function dashboardToggleCard(card) {
    const shouldExpand = !card.classList.contains("is-expanded");

    document
        .querySelectorAll("[data-dashboard-card]")
        .forEach(otherCard => {
            dashboardSetCardExpanded(
                otherCard,
                otherCard === card && shouldExpand
            );
        });

    if (shouldExpand) {
        dashboardScrollExpandedCardIntoView(card);
    }
}

function dashboardBindActions() {
    document
        .querySelectorAll("[data-dashboard-card]")
        .forEach(card => {
            const toggle = card.querySelector(".dashboard-sensor-toggle");

            if (!toggle || toggle.dataset.bound === "true") {
                return;
            }

            toggle.dataset.bound = "true";
            toggle.addEventListener("click", () => dashboardToggleCard(card));
        });

    document
        .querySelectorAll("[data-dashboard-page]")
        .forEach(button => {
            if (button.dataset.bound === "true") {
                return;
            }

            button.dataset.bound = "true";
            button.addEventListener("click", event => {
                event.stopPropagation();
                loadPage(button.dataset.dashboardPage);
            });
        });

    const automaticSwitch = document.getElementById(
        "dashboardAutomaticLightSwitch"
    );

    if (
        automaticSwitch &&
        automaticSwitch.dataset.bound !== "true"
    ) {
        automaticSwitch.dataset.bound = "true";
        automaticSwitch.addEventListener("change", () => {
            dashboardSetAutomaticLight(automaticSwitch.checked);
        });
    }

    const manualSwitch = document.getElementById(
        "dashboardManualLightSwitch"
    );

    if (manualSwitch && manualSwitch.dataset.bound !== "true") {
        manualSwitch.dataset.bound = "true";
        manualSwitch.addEventListener("change", () => {
            dashboardSetManualLight(manualSwitch.checked);
        });
    }
}

async function updateDashboard() {
    const phElement = document.getElementById("phValue");

    if (!phElement) {
        return;
    }

    dashboardBindActions();

    try {
        const [statusData, configData] = await Promise.all([
            getStatus(),
            dashboardGetConfig()
        ]);

        const currentPh = Number(statusData.ph);
        const currentEc = Number(statusData.ec);
        const waterTemperature = Number(statusData.waterTemp);
        const currentHumidity = Number(statusData.humidity);

        document.getElementById("phValue").textContent =
            dashboardFormatNumber(currentPh, 2);

        document.getElementById("ecValue").textContent =
            dashboardFormatNumber(currentEc, 2);

        document.getElementById("waterTemp").textContent =
            dashboardFormatNumber(waterTemperature, 1);

        document.getElementById("humidity").textContent =
            dashboardFormatNumber(currentHumidity, 0);

        dashboardRenderClock(statusData.clock);
        dashboardRenderLight(statusData.light);
        dashboardRenderActiveProfile(statusData.activeProfile);
        dashboardRenderPhStatus(currentPh, configData);
        dashboardRenderAutomaticConfig(configData);
        dashboardRenderSecondaryDetails(statusData, configData);
        dashboardRenderLastUpdate();

    } catch (error) {
        console.error("No se pudo consultar el ESP32:", error);

        document.getElementById("phValue").textContent = "--";
        document.getElementById("ecValue").textContent = "--";
        document.getElementById("waterTemp").textContent = "--";
        document.getElementById("humidity").textContent = "--";

        dashboardRenderClock(null);
        dashboardRenderLight(null);
        dashboardRenderActiveProfile(null);
        dashboardRenderPhStatus(NaN, null);
        dashboardRenderAutomaticConfig(null);
        dashboardRenderSecondaryDetails(null, null);

        const updateElement = document.getElementById("dashboardLastUpdate");

        if (updateElement) {
            updateElement.textContent = "Sin conexión";
        }
    }
}

// El reloj se dibuja cada segundo sin aumentar las consultas al ESP32.
setInterval(dashboardTickClock, 1000);

// Las lecturas generales se consultan cada dos segundos.
setInterval(updateDashboard, 2000);
