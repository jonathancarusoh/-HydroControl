const eventsPageState = {
    events: [],
    loading: false,
    lastRefreshMs: 0
};

const eventCategoryMetadata = {
    system: {
        label: "Sistema",
        icon: "bi-cpu"
    },
    wifi: {
        label: "WiFi",
        icon: "bi-wifi"
    },
    profile: {
        label: "Perfil",
        icon: "bi-bookmarks"
    },
    ph: {
        label: "pH",
        icon: "bi-droplet-half"
    },
    ec: {
        label: "EC",
        icon: "bi-lightning-charge"
    },
    clock: {
        label: "Reloj",
        icon: "bi-clock"
    },
    light: {
        label: "Luz",
        icon: "bi-lightbulb"
    },
    dosage: {
        label: "Dosificación",
        icon: "bi-eyedropper"
    }
};

async function eventsReadJson(response) {
    const responseText = await response.text();

    try {
        return JSON.parse(responseText);
    } catch (error) {
        console.error("Respuesta no JSON del ESP32:", responseText);
        throw new Error("El ESP32 devolvió una respuesta inválida.");
    }
}


async function eventsFetchCompatible(endpoints, options = {}) {
    let lastResponse = null;

    for (const endpoint of endpoints) {
        const response = await fetch(endpoint, options);
        lastResponse = response;

        if (response.status !== 404) {
            return response;
        }
    }

    const data = lastResponse
        ? await eventsReadJson(lastResponse)
        : {};

    throw new Error(
        data.message ||
        "La interfaz nueva está cargada, pero el firmware del ESP32 no contiene la API de eventos. Hacé Build + Upload del firmware y después recargá la página."
    );
}

function eventsShowMessage(message, success) {
    const element = document.getElementById("eventsMessage");

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

function eventsFormatBytes(bytes) {
    const value = Number(bytes);

    if (!Number.isFinite(value) || value <= 0) {
        return "0 KB";
    }

    if (value < 1024) {
        return `${value} B`;
    }

    return `${(value / 1024).toFixed(1)} KB`;
}

function eventsFormatUptime(seconds) {
    let remaining = Math.max(0, Number(seconds) || 0);
    const days = Math.floor(remaining / 86400);
    remaining %= 86400;
    const hours = Math.floor(remaining / 3600);
    remaining %= 3600;
    const minutes = Math.floor(remaining / 60);
    const secs = Math.floor(remaining % 60);

    const time = [hours, minutes, secs]
        .map(value => String(value).padStart(2, "0"))
        .join(":");

    return days > 0
        ? `Encendido +${days} d ${time}`
        : `Encendido +${time}`;
}

function eventsFormatTimestamp(event) {
    const epoch = Number(event.epoch);

    if (Number.isFinite(epoch) && epoch > 0) {
        return new Intl.DateTimeFormat("es-AR", {
            day: "2-digit",
            month: "2-digit",
            year: "numeric",
            hour: "2-digit",
            minute: "2-digit",
            second: "2-digit",
            hour12: false
        }).format(new Date(epoch * 1000));
    }

    return eventsFormatUptime(event.uptimeSeconds);
}

function eventsCreateTimelineItem(event) {
    const metadata =
        eventCategoryMetadata[event.category] ||
        eventCategoryMetadata.system;

    const article = document.createElement("article");
    article.className = "event-item";
    article.dataset.category = event.category || "system";

    const marker = document.createElement("span");
    marker.className = `event-marker ${event.category || "system"}`;
    marker.innerHTML = `<i class="bi ${metadata.icon}"></i>`;

    const content = document.createElement("div");
    content.className = "event-content";

    const header = document.createElement("div");
    header.className = "event-header";

    const titleBlock = document.createElement("div");

    const category = document.createElement("span");
    category.className = "event-category";
    category.textContent = metadata.label;

    const title = document.createElement("h5");
    title.textContent = event.title || "Evento del sistema";

    titleBlock.appendChild(category);
    titleBlock.appendChild(title);

    const timestamp = document.createElement("time");
    timestamp.textContent = eventsFormatTimestamp(event);

    header.appendChild(titleBlock);
    header.appendChild(timestamp);

    content.appendChild(header);

    if (event.detail) {
        const detail = document.createElement("p");
        detail.textContent = event.detail;
        content.appendChild(detail);
    }

    article.appendChild(marker);
    article.appendChild(content);

    return article;
}

function eventsRenderTimeline() {
    const timeline = document.getElementById("eventsTimeline");
    const emptyState = document.getElementById("eventsEmptyState");
    const filter =
        document.getElementById("eventsCategoryFilter")?.value || "all";

    if (!timeline || !emptyState) {
        return;
    }

    timeline.innerHTML = "";

    const visibleEvents = eventsPageState.events.filter(event => {
        return filter === "all" || event.category === filter;
    });

    visibleEvents.forEach(event => {
        timeline.appendChild(eventsCreateTimelineItem(event));
    });

    emptyState.classList.toggle(
        "d-none",
        visibleEvents.length > 0
    );
}

function eventsRenderSummary(data) {
    const countElement = document.getElementById("eventsCount");
    const storageElement = document.getElementById("eventsStorage");
    const clockStateElement = document.getElementById("eventsClockState");

    if (countElement) {
        countElement.textContent = String(data.count || 0);
    }

    if (storageElement) {
        storageElement.textContent = eventsFormatBytes(data.fileBytes);
    }

    if (clockStateElement) {
        clockStateElement.textContent = data.clockConfigured
            ? "Fecha y hora configuradas"
            : "Por tiempo encendido";
    }
}

async function eventsLoad(showLoading = false) {
    if (eventsPageState.loading) {
        return;
    }

    eventsPageState.loading = true;

    const loadingElement = document.getElementById("eventsLoading");
    const refreshButton = document.getElementById("refreshEventsButton");

    if (showLoading) {
        loadingElement?.classList.remove("d-none");
    }

    if (refreshButton) {
        refreshButton.disabled = true;
    }

    try {
        const response = await eventsFetchCompatible(
            ["/api/events?limit=80", "/api/events/list?limit=80"],
            { cache: "no-store" }
        );

        const data = await eventsReadJson(response);

        if (!response.ok || data.success === false) {
            throw new Error(
                data.message || "No se pudieron cargar los eventos."
            );
        }

        eventsPageState.events = Array.isArray(data.events)
            ? data.events
            : [];
        eventsPageState.lastRefreshMs = Date.now();

        eventsRenderSummary(data);
        eventsRenderTimeline();
    } catch (error) {
        console.error("Error cargando eventos:", error);
        eventsShowMessage(error.message, false);
    } finally {
        eventsPageState.loading = false;
        loadingElement?.classList.add("d-none");

        if (refreshButton) {
            refreshButton.disabled = false;
        }
    }
}

async function eventsClearLog() {
    const confirmed = window.confirm(
        "¿Seguro que querés limpiar el registro de eventos?\n\n" +
        "Esta acción no modifica perfiles ni configuraciones."
    );

    if (!confirmed) {
        return;
    }

    const button = document.getElementById("clearEventsButton");

    if (button) {
        button.disabled = true;
        button.innerHTML = `
            <span class="spinner-border spinner-border-sm me-2"></span>
            Limpiando...
        `;
    }

    try {
        const response = await fetch("/api/events/clear", {
            method: "POST"
        });

        const result = await eventsReadJson(response);

        if (response.status === 404) {
            throw new Error(
                "El firmware del ESP32 no contiene la función para limpiar eventos. Hacé Build + Upload del firmware."
            );
        }

        if (!response.ok || !result.success) {
            throw new Error(
                result.message || "No se pudo limpiar el registro."
            );
        }

        eventsShowMessage(result.message, true);
        await eventsLoad(false);
    } catch (error) {
        console.error("Error limpiando eventos:", error);
        eventsShowMessage(error.message, false);
    } finally {
        if (button) {
            button.disabled = false;
            button.innerHTML = `
                <i class="bi bi-trash3"></i>
                Limpiar registro
            `;
        }
    }
}

function updateEventsPage() {
    eventsLoad(true);

    document
        .getElementById("refreshEventsButton")
        ?.addEventListener("click", () => eventsLoad(true));

    document
        .getElementById("clearEventsButton")
        ?.addEventListener("click", eventsClearLog);

    document
        .getElementById("eventsCategoryFilter")
        ?.addEventListener("change", eventsRenderTimeline);
}

setInterval(() => {
    if (!document.getElementById("eventsPageRoot")) {
        return;
    }

    if (Date.now() - eventsPageState.lastRefreshMs >= 10000) {
        eventsLoad(false);
    }
}, 1000);
