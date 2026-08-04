const clockPageState = {
    data: null,
    baseEpoch: null,
    receivedAtMs: 0,
    lastServerRefreshMs: 0,
    requestInProgress: false,
    clockDirty: false,
    lightDirty: false
};

function clockShowMessage(message, success) {
    const element = document.getElementById("clockPageMessage");

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
        success === null
            ? "alert-info"
            : success
                ? "alert-success"
                : "alert-danger"
    );
}

async function clockReadJson(response) {
    const responseText = await response.text();

    try {
        return JSON.parse(responseText);
    } catch (error) {
        console.error("Respuesta no JSON del ESP32:", responseText);
        throw new Error("El ESP32 devolvió una respuesta inválida.");
    }
}


async function clockFetchCompatible(endpoints, options = {}) {
    let lastResponse = null;

    for (const endpoint of endpoints) {
        const response = await fetch(endpoint, options);
        lastResponse = response;

        if (response.status !== 404) {
            return response;
        }
    }

    const data = lastResponse
        ? await clockReadJson(lastResponse)
        : {};

    throw new Error(
        data.message ||
        "La interfaz nueva está cargada, pero el firmware del ESP32 no contiene la API de reloj. Hacé Build + Upload del firmware y después recargá la página."
    );
}

function clockPad(value) {
    return String(value).padStart(2, "0");
}

function clockFormatDateInput(date) {
    return [
        date.getFullYear(),
        clockPad(date.getMonth() + 1),
        clockPad(date.getDate())
    ].join("-");
}

function clockFormatTimeInput(date) {
    return [
        clockPad(date.getHours()),
        clockPad(date.getMinutes()),
        clockPad(date.getSeconds())
    ].join(":");
}

function clockFormatLongDate(date) {
    return new Intl.DateTimeFormat("es-AR", {
        weekday: "long",
        day: "numeric",
        month: "long",
        year: "numeric"
    }).format(date);
}

function clockGetEstimatedDate() {
    if (!Number.isFinite(clockPageState.baseEpoch)) {
        return null;
    }

    const elapsedSeconds =
        (Date.now() - clockPageState.receivedAtMs) / 1000;

    return new Date(
        (clockPageState.baseEpoch + elapsedSeconds) * 1000
    );
}

function clockRenderLiveTime() {
    const timeElement = document.getElementById("clockLiveTime");
    const dateElement = document.getElementById("clockLiveDate");

    if (!timeElement || !dateElement) {
        return;
    }

    const date = clockGetEstimatedDate();

    if (!date) {
        timeElement.textContent = "--:--:--";
        dateElement.textContent = "Hora todavía no configurada";
        return;
    }

    timeElement.textContent = date.toLocaleTimeString("es-AR", {
        hour: "2-digit",
        minute: "2-digit",
        second: "2-digit",
        hour12: false
    });

    const formattedDate = clockFormatLongDate(date);
    dateElement.textContent =
        formattedDate.charAt(0).toUpperCase() +
        formattedDate.slice(1);
}

function clockMinutesFromTime(value) {
    const parts = String(value || "").split(":").map(Number);

    if (
        parts.length < 2 ||
        !Number.isFinite(parts[0]) ||
        !Number.isFinite(parts[1])
    ) {
        return null;
    }

    return parts[0] * 60 + parts[1];
}

function clockCalculatePhotoperiod(onValue, offValue) {
    const onMinutes = clockMinutesFromTime(onValue);
    const offMinutes = clockMinutesFromTime(offValue);

    if (onMinutes === null || offMinutes === null) {
        return null;
    }

    let duration = offMinutes - onMinutes;

    if (duration <= 0) {
        duration += 24 * 60;
    }

    return {
        totalMinutes: duration,
        hours: Math.floor(duration / 60),
        minutes: duration % 60,
        onMinutes,
        offMinutes
    };
}

function clockRenderDayTrack() {
    const onInput = document.getElementById("lightOnInput");
    const offInput = document.getElementById("lightOffInput");
    const primary = document.getElementById("lightDaySegmentPrimary");
    const secondary = document.getElementById("lightDaySegmentSecondary");
    const photoperiodElement = document.getElementById(
        "lightPhotoperiodValue"
    );

    if (
        !onInput ||
        !offInput ||
        !primary ||
        !secondary ||
        !photoperiodElement
    ) {
        return;
    }

    const photoperiod = clockCalculatePhotoperiod(
        onInput.value,
        offInput.value
    );

    primary.style.width = "0%";
    primary.style.left = "0%";
    secondary.style.width = "0%";
    secondary.style.left = "0%";

    if (!photoperiod) {
        photoperiodElement.textContent = "-- h -- min";
        return;
    }

    photoperiodElement.textContent =
        `${photoperiod.hours} h ${photoperiod.minutes} min`;

    const onPercent = photoperiod.onMinutes / 1440 * 100;
    const offPercent = photoperiod.offMinutes / 1440 * 100;

    if (photoperiod.onMinutes < photoperiod.offMinutes) {
        primary.style.left = `${onPercent}%`;
        primary.style.width = `${offPercent - onPercent}%`;
    } else {
        primary.style.left = `${onPercent}%`;
        primary.style.width = `${100 - onPercent}%`;
        secondary.style.left = "0%";
        secondary.style.width = `${offPercent}%`;
    }
}

function clockRenderServerData(data) {
    clockPageState.data = data;
    clockPageState.lastServerRefreshMs = Date.now();

    if (data.clock?.configured && Number.isFinite(data.clock.epoch)) {
        clockPageState.baseEpoch = Number(data.clock.epoch);
        clockPageState.receivedAtMs = Date.now();
    } else {
        clockPageState.baseEpoch = null;
        clockPageState.receivedAtMs = 0;
    }

    const statusBadge = document.getElementById("clockStatusBadge");

    if (statusBadge) {
        statusBadge.classList.remove("ready", "pending", "restored");

        if (!data.clock?.configured) {
            statusBadge.textContent = "Requiere configuración";
            statusBadge.classList.add("pending");
        } else if (data.clock?.restoredAfterRestart) {
            statusBadge.textContent = "Restaurado tras reinicio";
            statusBadge.classList.add("restored");
        } else {
            statusBadge.textContent = "Hora configurada";
            statusBadge.classList.add("ready");
        }
    }

    const dateInput = document.getElementById("clockDateInput");
    const timeInput = document.getElementById("clockTimeInput");

    if (
        !clockPageState.clockDirty &&
        data.clock?.configured &&
        Number.isFinite(data.clock.epoch)
    ) {
        const date = new Date(Number(data.clock.epoch) * 1000);

        if (dateInput) {
            dateInput.value = clockFormatDateInput(date);
        }

        if (timeInput) {
            timeInput.value = clockFormatTimeInput(date);
        }
    }

    const enabledInput = document.getElementById("lightScheduleEnabled");
    const lightOnInput = document.getElementById("lightOnInput");
    const lightOffInput = document.getElementById("lightOffInput");

    if (!clockPageState.lightDirty) {
        if (enabledInput) {
            enabledInput.checked = Boolean(data.light?.enabled);
        }

        if (lightOnInput) {
            lightOnInput.value = data.light?.on || "06:00";
        }

        if (lightOffInput) {
            lightOffInput.value = data.light?.off || "18:00";
        }
    }

    const stateElement = document.getElementById("lightProgrammedState");
    const nextElement = document.getElementById("lightNextChange");

    if (stateElement) {
        stateElement.textContent =
            data.light?.stateLabel || "Sin información";
        stateElement.dataset.state = data.light?.stateCode || "unknown";
    }

    if (nextElement) {
        nextElement.textContent =
            data.light?.nextChangeLabel || "---";
    }

    clockUpdateLightControls();
    clockRenderLiveTime();
    clockRenderDayTrack();
}

function clockUpdateLightControls() {
    const enabled = Boolean(
        document.getElementById("lightScheduleEnabled")?.checked
    );

    document
        .getElementById("lightTimeControls")
        ?.classList.toggle("disabled", !enabled);
}

async function clockLoadStatus(showError = true) {
    if (clockPageState.requestInProgress) {
        return;
    }

    clockPageState.requestInProgress = true;

    try {
        const response = await clockFetchCompatible(
            ["/api/clock/status", "/api/clock"],
            { cache: "no-store" }
        );

        const data = await clockReadJson(response);

        if (!response.ok || data.success === false) {
            throw new Error(
                data.message || "No se pudo cargar el reloj."
            );
        }

        clockRenderServerData(data);
    } catch (error) {
        console.error("Error cargando reloj y luz:", error);

        if (showError) {
            clockShowMessage(error.message, false);
        }
    } finally {
        clockPageState.requestInProgress = false;
    }
}

function clockCopyDeviceTimeToForm() {
    const now = new Date();
    const dateInput = document.getElementById("clockDateInput");
    const timeInput = document.getElementById("clockTimeInput");

    if (dateInput) {
        dateInput.value = clockFormatDateInput(now);
    }

    if (timeInput) {
        timeInput.value = clockFormatTimeInput(now);
    }

    clockPageState.clockDirty = true;

    clockShowMessage(
        "Copié la fecha y hora de este dispositivo. Revisalas y guardá.",
        null
    );
}

async function clockSaveManualTime() {
    const dateValue = document.getElementById("clockDateInput")?.value;
    const timeValue = document.getElementById("clockTimeInput")?.value;
    const saveButton = document.getElementById("saveClockButton");

    if (!dateValue || !timeValue) {
        clockShowMessage("Completá la fecha y la hora.", false);
        return;
    }

    const localDate = new Date(`${dateValue}T${timeValue}`);

    if (!Number.isFinite(localDate.getTime())) {
        clockShowMessage("La fecha u hora no es válida.", false);
        return;
    }

    const epoch = Math.floor(localDate.getTime() / 1000);
    const body = new URLSearchParams({
        epoch: String(epoch)
    });

    if (saveButton) {
        saveButton.disabled = true;
        saveButton.innerHTML = `
            <span class="spinner-border spinner-border-sm me-2"></span>
            Guardando...
        `;
    }

    try {
        const response = await fetch("/api/clock/set", {
            method: "POST",
            headers: {
                "Content-Type": "application/x-www-form-urlencoded"
            },
            body: body.toString()
        });

        const result = await clockReadJson(response);

        if (response.status === 404) {
            throw new Error(
                "El firmware del ESP32 no contiene la función para guardar la hora. Hacé Build + Upload del firmware."
            );
        }

        if (!response.ok || !result.success) {
            throw new Error(
                result.message || "No se pudo guardar la hora."
            );
        }

        clockPageState.clockDirty = false;
        clockShowMessage(result.message, true);
        await clockLoadStatus(false);
    } catch (error) {
        console.error("Error guardando reloj:", error);
        clockShowMessage(error.message, false);
    } finally {
        if (saveButton) {
            saveButton.disabled = false;
            saveButton.innerHTML = `
                <i class="bi bi-check2-circle"></i>
                Guardar hora en el ESP32
            `;
        }
    }
}

async function clockSaveLightSchedule() {
    const enabled = Boolean(
        document.getElementById("lightScheduleEnabled")?.checked
    );
    const lightOn = document.getElementById("lightOnInput")?.value;
    const lightOff = document.getElementById("lightOffInput")?.value;
    const saveButton = document.getElementById("saveLightScheduleButton");

    if (!lightOn || !lightOff) {
        clockShowMessage(
            "Completá los horarios de encendido y apagado.",
            false
        );
        return;
    }

    if (lightOn === lightOff) {
        clockShowMessage(
            "El horario de encendido y apagado no puede ser el mismo.",
            false
        );
        return;
    }

    const body = new URLSearchParams({
        enabled: enabled ? "true" : "false",
        lightOn,
        lightOff
    });

    if (saveButton) {
        saveButton.disabled = true;
        saveButton.innerHTML = `
            <span class="spinner-border spinner-border-sm me-2"></span>
            Guardando...
        `;
    }

    try {
        const response = await fetch("/api/light/schedule", {
            method: "POST",
            headers: {
                "Content-Type": "application/x-www-form-urlencoded"
            },
            body: body.toString()
        });

        const result = await clockReadJson(response);

        if (response.status === 404) {
            throw new Error(
                "El firmware del ESP32 no contiene la programación de luz. Hacé Build + Upload del firmware."
            );
        }

        if (!response.ok || !result.success) {
            throw new Error(
                result.message || "No se pudo guardar el horario."
            );
        }

        clockPageState.lightDirty = false;
        clockShowMessage(result.message, true);
        await clockLoadStatus(false);
    } catch (error) {
        console.error("Error guardando horario de luz:", error);
        clockShowMessage(error.message, false);
    } finally {
        if (saveButton) {
            saveButton.disabled = false;
            saveButton.innerHTML = `
                <i class="bi bi-calendar-check"></i>
                Guardar programación de luz
            `;
        }
    }
}

function updateClockPage() {
    clockLoadStatus();

    document
        .getElementById("refreshClockButton")
        ?.addEventListener("click", () => clockLoadStatus());

    document
        .getElementById("useDeviceTimeButton")
        ?.addEventListener("click", clockCopyDeviceTimeToForm);

    document
        .getElementById("saveClockButton")
        ?.addEventListener("click", clockSaveManualTime);

    document
        .getElementById("saveLightScheduleButton")
        ?.addEventListener("click", clockSaveLightSchedule);

    ["clockDateInput", "clockTimeInput"].forEach(id => {
        document
            .getElementById(id)
            ?.addEventListener("input", () => {
                clockPageState.clockDirty = true;
            });
    });

    document
        .getElementById("lightScheduleEnabled")
        ?.addEventListener("change", () => {
            clockPageState.lightDirty = true;
            clockUpdateLightControls();
        });

    ["lightOnInput", "lightOffInput"].forEach(id => {
        document
            .getElementById(id)
            ?.addEventListener("input", () => {
                clockPageState.lightDirty = true;
                clockRenderDayTrack();
            });
    });
}

setInterval(() => {
    if (!document.getElementById("clockPageRoot")) {
        return;
    }

    clockRenderLiveTime();

    if (
        Date.now() - clockPageState.lastServerRefreshMs >= 5000
    ) {
        clockLoadStatus(false);
    }
}, 1000);
