async function updateEcPage() {
    const targetElement = document.getElementById("ecConfiguredTarget");
    const messageElement = document.getElementById("ecPageMessage");

    try {
        const response = await fetch("/api/config", { cache: "no-store" });
        const text = await response.text();
        let data;

        try {
            data = JSON.parse(text);
        } catch {
            throw new Error("El ESP32 devolvió una respuesta inválida.");
        }

        if (!response.ok) {
            throw new Error(data.message || "No se pudo leer la configuración de EC.");
        }

        if (targetElement) {
            const target = Number(data.targetEc);
            targetElement.textContent = Number.isFinite(target)
                ? target.toFixed(2)
                : "--";
        }
    } catch (error) {
        console.error("Error cargando la página EC:", error);

        if (messageElement) {
            messageElement.textContent = error.message;
            messageElement.classList.remove("d-none", "alert-success");
            messageElement.classList.add("alert-danger");
        }
    }
}
