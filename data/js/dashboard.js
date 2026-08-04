const dashboardTimeState = {
    baseEpoch: null,
    receivedAtMs: 0
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
    if (!clockData?.configured || !Number.isFinite(clockData.epoch)) {
        dashboardTimeState.baseEpoch = null;
        dashboardTimeState.receivedAtMs = 0;
        dashboardTickClock();
        return;
    }

    dashboardTimeState.baseEpoch = Number(clockData.epoch);
    dashboardTimeState.receivedAtMs = Date.now();
    dashboardTickClock();
}

function dashboardRenderLight(lightData) {
    const stateElement = document.getElementById("dashboardLightState");
    const nextElement = document.getElementById("dashboardLightNext");

    if (!stateElement || !nextElement) {
        return;
    }

    const stateCode = lightData?.stateCode || "unknown";
    const icon = stateCode === "on"
        ? "bi-lightbulb-fill"
        : "bi-lightbulb";

    stateElement.dataset.state = stateCode;
    stateElement.innerHTML = `
        <i class="bi ${icon}"></i>
        ${lightData?.stateLabel || "Sin información"}
    `;

    nextElement.textContent = lightData?.nextChangeLabel || "---";
}

async function updateDashboard() {
    const phElement = document.getElementById("phValue");

    // No estamos en el Dashboard.
    if (!phElement) {
        return;
    }

    try {
        const data = await getStatus();

        document.getElementById("phValue").innerText =
            Number(data.ph).toFixed(2);

        document.getElementById("ecValue").innerText =
            Number(data.ec).toFixed(2);

        document.getElementById("waterTemp").innerText =
            `${Number(data.waterTemp).toFixed(1)} °C`;

        document.getElementById("humidity").innerText =
            `${Number(data.humidity).toFixed(0)} %`;

        dashboardRenderClock(data.clock);
        dashboardRenderLight(data.light);

    } catch (error) {
        console.error("No se pudo consultar el ESP32:", error);

        document.getElementById("phValue").innerText = "--";
        document.getElementById("ecValue").innerText = "--";
        document.getElementById("waterTemp").innerText = "--";
        document.getElementById("humidity").innerText = "--";

        dashboardRenderClock(null);
        dashboardRenderLight(null);
    }
}

// El reloj se dibuja cada segundo sin aumentar las consultas al ESP32.
setInterval(dashboardTickClock, 1000);

// Las lecturas generales se consultan cada dos segundos.
setInterval(updateDashboard, 2000);
