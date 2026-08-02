async function updateDashboard() {
    const phElement = document.getElementById("phValue");

    // No estamos en el Dashboard
    if (!phElement) {
        return;
    }

    try {
        const data = await getStatus();

        document.getElementById("phValue").innerText =
            data.ph.toFixed(2);

        document.getElementById("ecValue").innerText =
            data.ec.toFixed(2);

        document.getElementById("waterTemp").innerText =
            `${data.waterTemp.toFixed(1)} °C`;

        document.getElementById("humidity").innerText =
            `${data.humidity.toFixed(0)} %`;

    } catch (error) {
        console.error("No se pudo consultar el ESP32:", error);

        document.getElementById("phValue").innerText = "--";
        document.getElementById("ecValue").innerText = "--";
        document.getElementById("waterTemp").innerText = "--";
        document.getElementById("humidity").innerText = "--";
    }
}

// Actualización automática cada dos segundos
setInterval(updateDashboard, 2000);