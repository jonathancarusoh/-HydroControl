function showMessage(elementId, message, success) {
    const element = document.getElementById(elementId);

    if (!element) {
        return;
    }

    element.textContent = message;

    element.classList.remove(
        "d-none",
        "alert-success",
        "alert-danger"
    );

    element.classList.add(
        success ? "alert-success" : "alert-danger"
    );
}

async function loadPhConfig() {
    try {
        const response = await fetch("/api/config", {
            cache: "no-store"
        });

        if (!response.ok) {
            throw new Error(`Error HTTP: ${response.status}`);
        }

        const data = await response.json();

        document.getElementById("targetPh").value =
            Number(data.targetPh).toFixed(2);

        document.getElementById("phTolerance").value =
            Number(data.tolerance).toFixed(2);

        document.getElementById("doseDuration").value =
            data.doseSeconds;

        document.getElementById("doseInterval").value =
            data.intervalMinutes;

        document.getElementById("maxDoses").value =
            data.maxDoses;

        document.getElementById("autoMode").checked =
            data.automaticMode;

    } catch (error) {
        console.error("Error cargando configuración:", error);

        showMessage(
            "configMessage",
            "No se pudo cargar la configuración.",
            false
        );
    }
}

async function savePhConfig() {
    const targetPh =
        document.getElementById("targetPh").value;

    const tolerance =
        document.getElementById("phTolerance").value;

    const doseSeconds = Number(
        document.getElementById("doseDuration").value
    );

    const intervalMinutes =
        document.getElementById("doseInterval").value;

    const maxDoses =
        document.getElementById("maxDoses").value;

    const automaticMode =
        document.getElementById("autoMode").checked;

    const data = new URLSearchParams({
        targetPh,
        tolerance,
        doseSeconds: doseSeconds.toString(),
        intervalMinutes,
        maxDoses,
        automaticMode: automaticMode ? "true" : "false"
    });

    try {
        const response = await fetch("/api/config", {
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
                result.message || "No se pudo guardar"
            );
        }

        return result;

    } catch (error) {
        console.error("Error guardando configuración:", error);
        throw error;
    }
}

async function saveTargetPh() {
    try {
        await savePhConfig();

        showMessage(
            "targetMessage",
            "Objetivo de pH guardado",
            true
        );

    } catch (error) {
        showMessage(
            "targetMessage",
            error.message,
            false
        );
    }
}

async function saveDoseSettings() {
    try {
        await savePhConfig();

        showMessage(
            "configMessage",
            "Configuración de dosificación guardada",
            true
        );

    } catch (error) {
        showMessage(
            "configMessage",
            error.message,
            false
        );
    }
}

function updatePhPage() {
    const currentPh =
        document.getElementById("currentPh");

    if (currentPh) {
        currentPh.innerText =
            Number(hydro.ph).toFixed(2);
    }

    loadPhConfig();

    document
        .getElementById("saveDoseConfigButton")
        ?.addEventListener(
            "click",
            saveDoseSettings
        );

    document
        .getElementById("saveTargetButton")
        ?.addEventListener(
            "click",
            saveTargetPh
        );
}