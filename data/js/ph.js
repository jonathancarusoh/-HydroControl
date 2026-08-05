let phManualStatusTimer = null;
let phReadingTimer = null;
let phLastManualActive = false;
let phManualRequestInProgress = false;

function showPhMessage(elementId, message, success) {
    const element = document.getElementById(elementId);

    if (!element) {
        return;
    }

    element.textContent = message;

    element.classList.remove(
        "d-none",
        "is-success",
        "is-error",
        "is-info"
    );

    element.classList.add(
        success ? "is-success" : "is-error"
    );
}

function clearPhMessage(elementId) {
    document
        .getElementById(elementId)
        ?.classList.add("d-none");
}

async function readPhApiResponse(response) {
    const text = await response.text();
    let data;

    try {
        data = text ? JSON.parse(text) : {};
    } catch {
        throw new Error(
            "El ESP32 devolvió una respuesta inválida. Revisá que el firmware y la interfaz estén actualizados."
        );
    }

    if (!response.ok) {
        throw new Error(
            data.message ||
            `Error HTTP: ${response.status}`
        );
    }

    return data;
}

function getPhElement(id) {
    return document.getElementById(id);
}

function getPhNumber(id, fallback = 0) {
    const input = getPhElement(id);
    const value = Number(input?.value);

    return Number.isFinite(value)
        ? value
        : fallback;
}

function formatPhSeconds(value, decimals = 1) {
    const number = Number(value) || 0;

    return number.toLocaleString("es-AR", {
        minimumFractionDigits: decimals,
        maximumFractionDigits: decimals
    });
}

function formatPhRemainingTime(milliseconds) {
    const totalSeconds = Math.max(
        0,
        Number(milliseconds) || 0
    ) / 1000;

    if (totalSeconds >= 60) {
        const minutes = Math.floor(totalSeconds / 60);
        const seconds = Math.ceil(totalSeconds % 60);

        return `${minutes} min ${seconds} s`;
    }

    return `${formatPhSeconds(totalSeconds, 1)} s`;
}

function updatePhReadingDisplay() {
    const currentValue = Number(hydro.ph);
    const target = getPhNumber("targetPh", 5.8);
    const tolerance = getPhNumber("phTolerance", 0.1);

    const currentElement = getPhElement("currentPh");
    const stateElement = getPhElement("phState");
    const updatedElement = getPhElement("phUpdatedAt");

    if (currentElement) {
        currentElement.textContent = Number.isFinite(currentValue)
            ? currentValue.toFixed(2)
            : "--";
    }

    if (stateElement) {
        stateElement.classList.remove(
            "is-stable",
            "is-high",
            "is-low",
            "is-neutral"
        );

        if (!Number.isFinite(currentValue)) {
            stateElement.textContent = "SIN DATOS";
            stateElement.classList.add("is-neutral");
        } else if (currentValue > target + tolerance) {
            stateElement.textContent = "pH ALTO";
            stateElement.classList.add("is-high");
        } else if (currentValue < target - tolerance) {
            stateElement.textContent = "pH BAJO";
            stateElement.classList.add("is-low");
        } else {
            stateElement.textContent = "ESTABLE";
            stateElement.classList.add("is-stable");
        }
    }

    if (updatedElement) {
        updatedElement.textContent =
            `Actualizado ${new Date().toLocaleTimeString("es-AR")}`;
    }
}

function updatePhRangePreview() {
    const target = getPhNumber("targetPh", 5.8);
    const tolerance = getPhNumber("phTolerance", 0.1);
    const preview = getPhElement("targetRangePreview");

    if (preview) {
        preview.textContent =
            `${(target - tolerance).toFixed(2)} – ` +
            `${(target + tolerance).toFixed(2)}`;
    }

    updatePhReadingDisplay();
}

function updateAutomaticModeAppearance() {
    const automaticMode = Boolean(
        getPhElement("autoMode")?.checked
    );

    const card = document.querySelector(
        ".ph-control-auto-summary"
    );

    const state = getPhElement("autoModeState");
    const description = getPhElement("autoModeDescription");

    card?.classList.toggle(
        "is-enabled",
        automaticMode
    );

    if (state) {
        state.textContent = automaticMode
            ? "Regulación activada"
            : "Control manual";
    }

    if (description) {
        description.textContent = automaticMode
            ? "HydroControl podrá corregir el pH respetando los límites configurados."
            : "La regulación automática está pausada. Solo se ejecutarán órdenes manuales.";
    }
}

function updateAutomaticSummaries() {
    const duration = getPhNumber("doseDuration", 1);
    const interval = getPhNumber("doseInterval", 4);
    const maxDoses = getPhNumber("maxDoses", 3);

    const durationElement = getPhElement("autoDoseSummary");
    const intervalElement = getPhElement("autoIntervalSummary");
    const maxElement = getPhElement("autoLimitSummary");

    if (durationElement) {
        durationElement.textContent =
            `${formatPhSeconds(duration, 1)} s`;
    }

    if (intervalElement) {
        intervalElement.textContent =
            `${interval} min`;
    }

    if (maxElement) {
        maxElement.textContent =
            `${maxDoses} dosis`;
    }
}

function clampManualDoseCount() {
    const countInput = getPhElement("manualDoseCount");
    const maxInput = getPhElement("manualMaxDoses");

    if (!countInput || !maxInput) {
        return 1;
    }

    const configuredMax = Math.min(
        10,
        Math.max(1, Math.round(Number(maxInput.value) || 1))
    );

    maxInput.value = configuredMax;
    countInput.max = configuredMax;

    const count = Math.min(
        configuredMax,
        Math.max(1, Math.round(Number(countInput.value) || 1))
    );

    countInput.value = count;

    return count;
}

function updateManualSequenceEstimate() {
    const count = clampManualDoseCount();
    const duration = Math.min(
        30,
        Math.max(
            0.1,
            getPhNumber("manualDoseDuration", 1)
        )
    );

    const total = count * duration;
    const summary = getPhElement("manualSequenceEstimate");

    if (summary) {
        summary.textContent =
            `${count} ${count === 1 ? "dosis" : "dosis"} · ` +
            `${formatPhSeconds(total, 1)} s total`;
    }

    const minusCount = getPhElement("doseMinusCount");
    const plusCount = getPhElement("dosePlusCount");

    if (!phLastManualActive) {
        if (minusCount) minusCount.textContent = count;
        if (plusCount) plusCount.textContent = count;
    }
}

function collectPhConfig() {
    return new URLSearchParams({
        targetPh: getPhNumber("targetPh", 5.8).toString(),
        tolerance: getPhNumber("phTolerance", 0.1).toString(),
        doseSeconds: getPhNumber("doseDuration", 1).toString(),
        intervalMinutes: getPhNumber("doseInterval", 4).toString(),
        maxDoses: getPhNumber("maxDoses", 3).toString(),
        automaticMode: getPhElement("autoMode")?.checked
            ? "true"
            : "false",
        manualDoseSeconds:
            getPhNumber("manualDoseDuration", 1).toString(),
        manualMaxDoses:
            getPhNumber("manualMaxDoses", 3).toString()
    });
}

async function loadPhConfig() {
    const response = await fetch("/api/config", {
        cache: "no-store"
    });

    const data = await readPhApiResponse(response);

    getPhElement("targetPh").value =
        Number(data.targetPh).toFixed(2);

    getPhElement("phTolerance").value =
        Number(data.tolerance).toFixed(2);

    getPhElement("doseDuration").value =
        Number(data.doseSeconds);

    getPhElement("doseInterval").value =
        Number(data.intervalMinutes);

    getPhElement("maxDoses").value =
        Number(data.maxDoses);

    getPhElement("autoMode").checked =
        Boolean(data.automaticMode);

    getPhElement("manualDoseDuration").value =
        Number(data.manualDoseSeconds || 1);

    getPhElement("manualMaxDoses").value =
        Number(data.manualMaxDoses || 3);

    updatePhRangePreview();
    updateAutomaticModeAppearance();
    updateAutomaticSummaries();
    updateManualSequenceEstimate();

    return data;
}

async function savePhConfig() {
    const response = await fetch("/api/config", {
        method: "POST",
        headers: {
            "Content-Type":
                "application/x-www-form-urlencoded"
        },
        body: collectPhConfig().toString()
    });

    return readPhApiResponse(response);
}

async function saveTargetPh() {
    clearPhMessage("targetMessage");

    try {
        await savePhConfig();
        updatePhRangePreview();

        showPhMessage(
            "targetMessage",
            "Objetivo de pH guardado.",
            true
        );
    } catch (error) {
        showPhMessage(
            "targetMessage",
            error.message,
            false
        );
    }
}

async function saveManualSettings() {
    clearPhMessage("manualConfigMessage");

    try {
        await savePhConfig();
        updateManualSequenceEstimate();

        showPhMessage(
            "manualConfigMessage",
            "Límites manuales guardados.",
            true
        );
    } catch (error) {
        showPhMessage(
            "manualConfigMessage",
            error.message,
            false
        );
    }
}

async function saveAutomaticSettings() {
    clearPhMessage("configMessage");

    try {
        await savePhConfig();
        updateAutomaticModeAppearance();
        updateAutomaticSummaries();
        updatePhRangePreview();

        showPhMessage(
            "configMessage",
            "Configuración automática guardada.",
            true
        );
    } catch (error) {
        showPhMessage(
            "configMessage",
            error.message,
            false
        );
    }
}

async function saveAutomaticModeChange(previousValue) {
    updateAutomaticModeAppearance();

    try {
        await savePhConfig();

        showPhMessage(
            "configMessage",
            getPhElement("autoMode").checked
                ? "Modo automático activado."
                : "Modo automático pausado.",
            true
        );
    } catch (error) {
        getPhElement("autoMode").checked = previousValue;
        updateAutomaticModeAppearance();

        showPhMessage(
            "configMessage",
            error.message,
            false
        );
    }
}

function setManualControlsLocked(locked) {
    [
        "manualDoseDecrease",
        "manualDoseIncrease",
        "manualDoseCount",
        "manualDoseDuration",
        "manualMaxDoses",
        "saveManualConfigButton",
        "doseMinusButton",
        "dosePlusButton"
    ].forEach(id => {
        const element = getPhElement(id);

        if (element) {
            element.disabled = locked;
        }
    });

    const automaticSwitch = getPhElement("autoMode");

    if (automaticSwitch) {
        automaticSwitch.disabled = locked;
    }
}

function renderManualDoseStatus(status) {
    if (!getPhElement("manualSequencePanel")) {
        return;
    }

    const wasActive = phLastManualActive;
    const active = Boolean(status.active);
    const direction = status.direction || "none";
    const remaining = Number(status.remainingDoses) || 0;
    const current = Number(status.currentDoseNumber) || 0;
    const total = Number(status.totalDoses) || 0;
    const completed = Number(status.completedDoses) || 0;
    const progress = Math.min(
        100,
        Math.max(0, Number(status.progressPercent) || 0)
    );

    const runtimePanel = getPhElement("manualSequencePanel");
    const badge = getPhElement("manualStatusBadge");
    const title = getPhElement("manualSequenceTitle");
    const description = getPhElement("manualSequenceDescription");
    const progressBar = getPhElement("manualProgressBar");
    const currentElement = getPhElement("manualCurrentDose");
    const remainingElement = getPhElement("manualRemainingCount");
    const timeElement = getPhElement("manualTimeRemaining");
    const cancelButton = getPhElement("cancelManualDoseButton");
    const minusCount = getPhElement("doseMinusCount");
    const plusCount = getPhElement("dosePlusCount");
    const minusStrong = document.querySelector(
        "#doseMinusButton .ph-manual-dose-copy strong"
    );
    const plusStrong = document.querySelector(
        "#dosePlusButton .ph-manual-dose-copy strong"
    );

    phLastManualActive = active;
    setManualControlsLocked(active || phManualRequestInProgress);

    runtimePanel.classList.toggle("is-idle", !active);
    runtimePanel.classList.toggle("is-active", active);
    runtimePanel.classList.toggle(
        "is-minus",
        active && direction === "minus"
    );
    runtimePanel.classList.toggle(
        "is-plus",
        active && direction === "plus"
    );

    badge.classList.remove(
        "is-ready",
        "is-minus",
        "is-plus"
    );

    if (active) {
        const label = direction === "minus"
            ? "Dosificando pH-"
            : "Dosificando pH+";

        badge.textContent = label;
        badge.classList.add(
            direction === "minus"
                ? "is-minus"
                : "is-plus"
        );

        title.textContent = label;
        description.textContent =
            `Dosis ${current} de ${total} · ` +
            `${formatPhSeconds(
                Number(status.doseDurationMs) / 1000,
                1
            )} s por dosis`;

        progressBar.style.width = `${progress}%`;
        currentElement.textContent = `${current} / ${total}`;
        remainingElement.textContent = remaining;
        timeElement.textContent = formatPhRemainingTime(
            status.sequenceRemainingMs
        );

        cancelButton.classList.remove("d-none");

        if (direction === "minus") {
            minusCount.textContent = remaining;
            plusCount.textContent = "—";
            minusStrong.textContent = "Dosificando pH-";
            plusStrong.textContent = "pH+ bloqueado";
        } else {
            plusCount.textContent = remaining;
            minusCount.textContent = "—";
            plusStrong.textContent = "Dosificando pH+";
            minusStrong.textContent = "pH- bloqueado";
        }
    } else {
        if (wasActive) {
            const countInput = getPhElement("manualDoseCount");

            if (countInput) {
                countInput.value = 1;
            }
        }

        const selected = clampManualDoseCount();

        badge.textContent = "Listo";
        badge.classList.add("is-ready");
        title.textContent = "Sin secuencia activa";
        description.textContent =
            "Elegí la cantidad y presioná pH- o pH+.";
        progressBar.style.width = "0%";
        currentElement.textContent = "—";
        remainingElement.textContent = "—";
        timeElement.textContent = "—";
        cancelButton.classList.add("d-none");
        minusCount.textContent = selected;
        plusCount.textContent = selected;
        minusStrong.textContent = "Dosificar pH-";
        plusStrong.textContent = "Dosificar pH+";
    }

    if (
        status.automaticMode === false &&
        getPhElement("autoMode")
    ) {
        getPhElement("autoMode").checked = false;
        updateAutomaticModeAppearance();
    }

    if (active || phManualRequestInProgress) {
        scheduleManualStatusPoll(250);
    } else {
        scheduleManualStatusPoll(1400);
    }

    if (wasActive && !active) {
        updateManualSequenceEstimate();
    }
}

async function fetchManualDoseStatus() {
    if (!getPhElement("manualSequencePanel")) {
        clearTimeout(phManualStatusTimer);
        phManualStatusTimer = null;
        return;
    }

    try {
        const response = await fetch(
            "/api/ph/manual/status",
            { cache: "no-store" }
        );

        const status = await readPhApiResponse(response);
        renderManualDoseStatus(status);
    } catch (error) {
        showPhMessage(
            "manualMessage",
            error.message,
            false
        );

        scheduleManualStatusPoll(2500);
    }
}

function scheduleManualStatusPoll(delay) {
    clearTimeout(phManualStatusTimer);

    phManualStatusTimer = setTimeout(
        fetchManualDoseStatus,
        delay
    );
}

async function startManualDose(direction) {
    if (phManualRequestInProgress || phLastManualActive) {
        return;
    }

    const doses = clampManualDoseCount();

    phManualRequestInProgress = true;
    setManualControlsLocked(true);
    clearPhMessage("manualMessage");

    try {
        await savePhConfig();

        const response = await fetch(
            "/api/ph/manual/start",
            {
                method: "POST",
                headers: {
                    "Content-Type":
                        "application/x-www-form-urlencoded"
                },
                body: new URLSearchParams({
                    direction,
                    doses: doses.toString()
                }).toString()
            }
        );

        const status = await readPhApiResponse(response);

        getPhElement("autoMode").checked = false;
        updateAutomaticModeAppearance();

        showPhMessage(
            "manualMessage",
            `${status.directionLabel}: secuencia iniciada con ${doses} ` +
            `${doses === 1 ? "dosis" : "dosis"}.`,
            true
        );

        renderManualDoseStatus(status);
    } catch (error) {
        showPhMessage(
            "manualMessage",
            error.message,
            false
        );
    } finally {
        phManualRequestInProgress = false;

        if (!phLastManualActive) {
            setManualControlsLocked(false);
        }
    }
}

async function cancelManualDose() {
    const button = getPhElement("cancelManualDoseButton");

    if (button) {
        button.disabled = true;
    }

    try {
        const response = await fetch(
            "/api/ph/manual/cancel",
            { method: "POST" }
        );

        const result = await readPhApiResponse(response);

        showPhMessage(
            "manualMessage",
            result.message || "Secuencia cancelada.",
            true
        );

        await fetchManualDoseStatus();
    } catch (error) {
        showPhMessage(
            "manualMessage",
            error.message,
            false
        );
    } finally {
        if (button) {
            button.disabled = false;
        }
    }
}

async function refreshPhReading() {
    if (!getPhElement("currentPh")) {
        clearTimeout(phReadingTimer);
        phReadingTimer = null;
        return;
    }

    try {
        await getStatus();
        updatePhReadingDisplay();
    } catch (error) {
        console.error("Error actualizando pH:", error);
    }

    phReadingTimer = setTimeout(
        refreshPhReading,
        2500
    );
}

function bindPhPageEvents() {
    getPhElement("saveTargetButton")
        ?.addEventListener("click", saveTargetPh);

    getPhElement("saveManualConfigButton")
        ?.addEventListener("click", saveManualSettings);

    getPhElement("saveDoseConfigButton")
        ?.addEventListener("click", saveAutomaticSettings);

    getPhElement("doseMinusButton")
        ?.addEventListener(
            "click",
            () => startManualDose("minus")
        );

    getPhElement("dosePlusButton")
        ?.addEventListener(
            "click",
            () => startManualDose("plus")
        );

    getPhElement("cancelManualDoseButton")
        ?.addEventListener("click", cancelManualDose);

    getPhElement("manualDoseDecrease")
        ?.addEventListener("click", () => {
            const input = getPhElement("manualDoseCount");
            input.value = Number(input.value) - 1;
            updateManualSequenceEstimate();
        });

    getPhElement("manualDoseIncrease")
        ?.addEventListener("click", () => {
            const input = getPhElement("manualDoseCount");
            input.value = Number(input.value) + 1;
            updateManualSequenceEstimate();
        });

    [
        "manualDoseCount",
        "manualDoseDuration",
        "manualMaxDoses"
    ].forEach(id => {
        getPhElement(id)?.addEventListener(
            "input",
            updateManualSequenceEstimate
        );
    });

    [
        "targetPh",
        "phTolerance"
    ].forEach(id => {
        getPhElement(id)?.addEventListener(
            "input",
            updatePhRangePreview
        );
    });

    [
        "doseDuration",
        "doseInterval",
        "maxDoses"
    ].forEach(id => {
        getPhElement(id)?.addEventListener(
            "input",
            updateAutomaticSummaries
        );
    });

    getPhElement("autoMode")
        ?.addEventListener("change", event => {
            const previousValue = !event.target.checked;
            saveAutomaticModeChange(previousValue);
        });
}

async function updatePhPage() {
    clearTimeout(phManualStatusTimer);
    clearTimeout(phReadingTimer);

    phLastManualActive = false;
    phManualRequestInProgress = false;

    bindPhPageEvents();

    try {
        await Promise.all([
            getStatus(),
            loadPhConfig()
        ]);

        updatePhReadingDisplay();
        await fetchManualDoseStatus();
    } catch (error) {
        console.error("Error cargando la página pH:", error);

        showPhMessage(
            "configMessage",
            "No se pudo cargar la configuración de pH.",
            false
        );
    }

    phReadingTimer = setTimeout(
        refreshPhReading,
        2500
    );
}
