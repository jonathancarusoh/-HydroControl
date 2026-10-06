const hydro = {
    online: false,
    ph: null,
    ec: 0,
    waterTemp: 0,
    humidity: 0,
    wifiRssi: 0
};

async function getStatus() {
    const response = await fetch("/api/status", {
        cache: "no-store"
    });

    if (!response.ok) {
        throw new Error(`Error HTTP: ${response.status}`);
    }

    const data = await response.json();

    Object.assign(hydro, data);

    return hydro;
}