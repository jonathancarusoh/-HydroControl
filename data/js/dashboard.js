function updateDashboard() {

    document.getElementById("phValue").innerText =
        hydro.ph.toFixed(2);

    document.getElementById("ecValue").innerText =
        hydro.ec.toFixed(2);

    document.getElementById("waterTemp").innerText =
        hydro.waterTemp.toFixed(1) + " °C";

    document.getElementById("humidity").innerText =
        hydro.humidity.toFixed(0) + " %";

}

setInterval(() => {

    simulateData();

    if (document.getElementById("phValue")) {

        updateDashboard();

    }

}, 1000);