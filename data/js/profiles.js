const profilesPageState = {
    profiles: [],
    activeId: -1,
    maxProfiles: 10,
    currentConfig: null
};

async function readProfilesJson(response) {
    const responseText = await response.text();

    try {
        return JSON.parse(responseText);
    } catch (error) {
        console.error("Respuesta no JSON del ESP32:", responseText);
        throw new Error("El ESP32 devolvió una respuesta inválida.");
    }
}

function showProfilesMessage(elementId, message, success) {
    const element = document.getElementById(elementId);

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
        success ? "alert-success" : "alert-danger"
    );
}

function hideProfilesMessage(elementId) {
    document.getElementById(elementId)?.classList.add("d-none");
}

function formatProfileNumber(value, decimals = 2) {
    const number = Number(value);

    return Number.isFinite(number)
        ? number.toFixed(decimals)
        : "--";
}

function calculatePhotoperiod(lightOn, lightOff) {
    if (!lightOn || !lightOff) {
        return null;
    }

    const [onHour, onMinute] = lightOn.split(":").map(Number);
    const [offHour, offMinute] = lightOff.split(":").map(Number);

    if (
        !Number.isFinite(onHour) ||
        !Number.isFinite(onMinute) ||
        !Number.isFinite(offHour) ||
        !Number.isFinite(offMinute)
    ) {
        return null;
    }

    const onTotal = onHour * 60 + onMinute;
    const offTotal = offHour * 60 + offMinute;
    let duration = offTotal - onTotal;

    if (duration <= 0) {
        duration += 24 * 60;
    }

    return {
        hours: Math.floor(duration / 60),
        minutes: duration % 60
    };
}

function updateProfilePhotoperiod() {
    const lightOn = document.getElementById("profileLightOn")?.value;
    const lightOff = document.getElementById("profileLightOff")?.value;
    const output = document.getElementById("profilePhotoperiod");

    if (!output) {
        return;
    }

    const photoperiod = calculatePhotoperiod(lightOn, lightOff);

    output.textContent = photoperiod
        ? `Fotoperíodo: ${photoperiod.hours} h ${photoperiod.minutes} min`
        : "Fotoperíodo: --";
}

function getCurrentProfileDefaults() {
    const config = profilesPageState.currentConfig || {};

    return {
        id: -1,
        name: "",
        targetPh: Number(config.targetPh ?? 5.8),
        tolerance: Number(config.tolerance ?? 0.1),
        doseSeconds: Number(config.doseSeconds ?? 0.5),
        intervalMinutes: Number(config.intervalMinutes ?? 4),
        maxDoses: Number(config.maxDoses ?? 3),
        automaticMode: Boolean(config.automaticMode ?? true),
        targetEc: Number(config.targetEc ?? 1.4),
        lightOn: String(config.lightOn ?? "06:00"),
        lightOff: String(config.lightOff ?? "18:00")
    };
}

function fillProfileForm(profile, editing = false) {
    const profileData = profile || getCurrentProfileDefaults();

    document.getElementById("profileId").value =
        editing ? String(profileData.id) : "-1";

    document.getElementById("profileName").value =
        editing ? profileData.name : "";

    document.getElementById("profileTargetPh").value =
        formatProfileNumber(profileData.targetPh, 2);

    document.getElementById("profilePhTolerance").value =
        formatProfileNumber(profileData.tolerance, 2);

    document.getElementById("profileDoseSeconds").value =
        Number(profileData.doseSeconds).toString();

    document.getElementById("profileDoseInterval").value =
        Number(profileData.intervalMinutes).toString();

    document.getElementById("profileMaxDoses").value =
        Number(profileData.maxDoses).toString();

    document.getElementById("profileAutomaticMode").checked =
        Boolean(profileData.automaticMode);

    document.getElementById("profileTargetEc").value =
        formatProfileNumber(profileData.targetEc, 2);

    document.getElementById("profileLightOn").value =
        profileData.lightOn;

    document.getElementById("profileLightOff").value =
        profileData.lightOff;

    const mode = document.getElementById("profileFormMode");
    const title = document.getElementById("profileFormTitle");
    const saveButton = document.getElementById("saveProfileButton");
    const cancelButton = document.getElementById("cancelProfileEditButton");

    if (mode) {
        mode.textContent = editing ? "EDITANDO PERFIL" : "NUEVO PERFIL";
    }

    if (title) {
        title.textContent = editing
            ? `Editar ${profileData.name}`
            : "Crear configuración";
    }

    if (saveButton) {
        saveButton.innerHTML = editing
            ? '<i class="bi bi-check2"></i> Guardar cambios'
            : '<i class="bi bi-floppy"></i> Guardar perfil';
    }

    cancelButton?.classList.toggle("d-none", !editing);
    hideProfilesMessage("profileFormMessage");
    updateProfilePhotoperiod();
}

function scrollToProfileEditor() {
    document.getElementById("profileEditorCard")?.scrollIntoView({
        behavior: "smooth",
        block: "start"
    });
}

function renderActiveProfile() {
    const activeProfile = profilesPageState.profiles.find(
        profile => profile.id === profilesPageState.activeId
    );

    const nameElement = document.getElementById("activeProfileName");
    const descriptionElement = document.getElementById(
        "activeProfileDescription"
    );
    const countElement = document.getElementById("profilesCountBadge");

    if (countElement) {
        countElement.textContent =
            `${profilesPageState.profiles.length} / ${profilesPageState.maxProfiles}`;
    }

    if (!nameElement || !descriptionElement) {
        return;
    }

    if (!activeProfile) {
        nameElement.textContent = "Ningún perfil aplicado";
        descriptionElement.textContent =
            "La configuración actual fue ajustada manualmente.";
        return;
    }

    nameElement.textContent = activeProfile.name;
    descriptionElement.textContent =
        `pH ${formatProfileNumber(activeProfile.targetPh)} · ` +
        `EC ${formatProfileNumber(activeProfile.targetEc)} mS/cm · ` +
        `Luz ${activeProfile.lightOn}–${activeProfile.lightOff}`;
}

function createProfileMetric(icon, label, value) {
    const metric = document.createElement("div");
    metric.className = "profile-metric";

    metric.innerHTML = `
        <span class="profile-metric-icon">
            <i class="bi ${icon}"></i>
        </span>
        <div>
            <small>${label}</small>
            <strong>${value}</strong>
        </div>
    `;

    return metric;
}

function createProfileCard(profile) {
    const card = document.createElement("article");
    card.className = "profile-card";

    if (profile.active) {
        card.classList.add("active");
    }

    const header = document.createElement("div");
    header.className = "profile-card-header";

    const titleBlock = document.createElement("div");
    titleBlock.className = "profile-card-title-block";

    const title = document.createElement("h5");
    title.textContent = profile.name;

    const subtitle = document.createElement("span");
    subtitle.textContent = profile.automaticMode
        ? "pH automático habilitado"
        : "pH automático deshabilitado";

    titleBlock.appendChild(title);
    titleBlock.appendChild(subtitle);
    header.appendChild(titleBlock);

    if (profile.active) {
        const badge = document.createElement("span");
        badge.className = "profile-active-badge";
        badge.innerHTML = '<i class="bi bi-check2-circle"></i> Activo';
        header.appendChild(badge);
    }

    const metrics = document.createElement("div");
    metrics.className = "profile-metrics";

    metrics.appendChild(
        createProfileMetric(
            "bi-droplet-half",
            "pH objetivo",
            formatProfileNumber(profile.targetPh)
        )
    );

    metrics.appendChild(
        createProfileMetric(
            "bi-lightning-charge",
            "EC objetivo",
            `${formatProfileNumber(profile.targetEc)} mS/cm`
        )
    );

    metrics.appendChild(
        createProfileMetric(
            "bi-lightbulb",
            "Horario de luz",
            `${profile.lightOn}–${profile.lightOff}`
        )
    );

    metrics.appendChild(
        createProfileMetric(
            "bi-stopwatch",
            "Dosificación",
            `${Number(profile.doseSeconds)} s · cada ${profile.intervalMinutes} min`
        )
    );

    const footer = document.createElement("div");
    footer.className = "profile-card-actions";

    const applyButton = document.createElement("button");
    applyButton.type = "button";
    applyButton.className = profile.active
        ? "btn btn-success"
        : "btn btn-outline-success";
    applyButton.disabled = profile.active;
    applyButton.innerHTML = profile.active
        ? '<i class="bi bi-check2"></i> Aplicado'
        : '<i class="bi bi-play-fill"></i> Aplicar';
    applyButton.addEventListener("click", () => applyProfile(profile));

    const editButton = document.createElement("button");
    editButton.type = "button";
    editButton.className = "btn btn-outline-light";
    editButton.innerHTML = '<i class="bi bi-pencil"></i> Editar';
    editButton.addEventListener("click", () => {
        fillProfileForm(profile, true);
        scrollToProfileEditor();
        document.getElementById("profileName")?.focus();
    });

    const duplicateButton = document.createElement("button");
    duplicateButton.type = "button";
    duplicateButton.className = "btn btn-outline-secondary";
    duplicateButton.innerHTML = '<i class="bi bi-copy"></i> Duplicar';
    duplicateButton.addEventListener("click", () => duplicateProfile(profile));

    const deleteButton = document.createElement("button");
    deleteButton.type = "button";
    deleteButton.className = "btn btn-outline-danger";
    deleteButton.innerHTML = '<i class="bi bi-trash"></i>';
    deleteButton.title = "Eliminar perfil";
    deleteButton.setAttribute("aria-label", `Eliminar ${profile.name}`);
    deleteButton.addEventListener("click", () => deleteSavedProfile(profile));

    footer.appendChild(applyButton);
    footer.appendChild(editButton);
    footer.appendChild(duplicateButton);
    footer.appendChild(deleteButton);

    card.appendChild(header);
    card.appendChild(metrics);
    card.appendChild(footer);

    return card;
}

function renderProfilesList() {
    const list = document.getElementById("profilesList");

    if (!list) {
        return;
    }

    list.innerHTML = "";

    if (profilesPageState.profiles.length === 0) {
        list.innerHTML = `
            <div class="profiles-empty-state">
                <span class="profiles-empty-icon">
                    <i class="bi bi-bookmarks"></i>
                </span>
                <strong>Todavía no hay perfiles guardados</strong>
                <p>
                    Creá uno con la configuración actual o cargá valores nuevos.
                </p>
            </div>
        `;
        return;
    }

    profilesPageState.profiles.forEach(profile => {
        list.appendChild(createProfileCard(profile));
    });
}

async function loadProfilesData() {
    const [configResponse, profilesResponse] = await Promise.all([
        fetch("/api/config", { cache: "no-store" }),
        fetch("/api/profiles", { cache: "no-store" })
    ]);

    const config = await readProfilesJson(configResponse);
    const profilesResult = await readProfilesJson(profilesResponse);

    if (!configResponse.ok) {
        throw new Error(config.message || "No se pudo cargar la configuración actual.");
    }

    if (!profilesResponse.ok || !profilesResult.success) {
        throw new Error(profilesResult.message || "No se pudieron cargar los perfiles.");
    }

    profilesPageState.currentConfig = config;
    profilesPageState.profiles = Array.isArray(profilesResult.profiles)
        ? profilesResult.profiles
        : [];
    profilesPageState.activeId = Number(profilesResult.activeId);
    profilesPageState.maxProfiles = Number(profilesResult.maxProfiles || 10);

    renderActiveProfile();
    renderProfilesList();
}

function collectProfileFormData(nameOverride = null, idOverride = null) {
    return new URLSearchParams({
        id: idOverride ?? document.getElementById("profileId").value,
        name: nameOverride ?? document.getElementById("profileName").value.trim(),
        targetPh: document.getElementById("profileTargetPh").value,
        tolerance: document.getElementById("profilePhTolerance").value,
        doseSeconds: document.getElementById("profileDoseSeconds").value,
        intervalMinutes: document.getElementById("profileDoseInterval").value,
        maxDoses: document.getElementById("profileMaxDoses").value,
        automaticMode: document.getElementById("profileAutomaticMode").checked
            ? "true"
            : "false",
        targetEc: document.getElementById("profileTargetEc").value,
        lightOn: document.getElementById("profileLightOn").value,
        lightOff: document.getElementById("profileLightOff").value
    });
}

function validateProfileForm() {
    const form = document.getElementById("profileForm");

    if (!form?.checkValidity()) {
        form?.reportValidity();
        return false;
    }

    const name = document.getElementById("profileName").value.trim();

    if (!name) {
        showProfilesMessage(
            "profileFormMessage",
            "Escribí un nombre para el perfil.",
            false
        );
        return false;
    }

    return true;
}

async function saveProfileForm(event) {
    event.preventDefault();

    if (!validateProfileForm()) {
        return;
    }

    const saveButton = document.getElementById("saveProfileButton");
    const editing = Number(document.getElementById("profileId").value) >= 0;

    saveButton.disabled = true;
    saveButton.innerHTML = `
        <span class="spinner-border spinner-border-sm me-2"></span>
        Guardando...
    `;

    try {
        const response = await fetch("/api/profiles/save", {
            method: "POST",
            headers: {
                "Content-Type": "application/x-www-form-urlencoded"
            },
            body: collectProfileFormData().toString()
        });

        const result = await readProfilesJson(response);

        if (!response.ok || !result.success) {
            throw new Error(result.message || "No se pudo guardar el perfil.");
        }

        showProfilesMessage("profilesPageMessage", result.message, true);
        await loadProfilesData();
        fillProfileForm(getCurrentProfileDefaults(), false);

    } catch (error) {
        console.error("Error guardando perfil:", error);
        showProfilesMessage("profileFormMessage", error.message, false);

    } finally {
        const stillEditing =
            Number(document.getElementById("profileId")?.value) >= 0;

        saveButton.disabled = false;
        saveButton.innerHTML = stillEditing
            ? '<i class="bi bi-check2"></i> Guardar cambios'
            : '<i class="bi bi-floppy"></i> Guardar perfil';
    }
}

async function applyProfile(profile) {
    const confirmed = window.confirm(
        `¿Aplicar el perfil "${profile.name}"?\n\n` +
        "Se reemplazará la configuración activa de pH, EC y horario de luz."
    );

    if (!confirmed) {
        return;
    }

    try {
        const response = await fetch("/api/profiles/apply", {
            method: "POST",
            headers: {
                "Content-Type": "application/x-www-form-urlencoded"
            },
            body: new URLSearchParams({ id: profile.id }).toString()
        });

        const result = await readProfilesJson(response);

        if (!response.ok || !result.success) {
            throw new Error(result.message || "No se pudo aplicar el perfil.");
        }

        showProfilesMessage("profilesPageMessage", result.message, true);
        await loadProfilesData();
        fillProfileForm(getCurrentProfileDefaults(), false);

    } catch (error) {
        console.error("Error aplicando perfil:", error);
        showProfilesMessage("profilesPageMessage", error.message, false);
    }
}

async function duplicateProfile(profile) {
    if (profilesPageState.profiles.length >= profilesPageState.maxProfiles) {
        showProfilesMessage(
            "profilesPageMessage",
            `Ya alcanzaste el máximo de ${profilesPageState.maxProfiles} perfiles.`,
            false
        );
        return;
    }

    const duplicatedName = `${profile.name} copia`.slice(0, 32);
    const data = new URLSearchParams({
        id: "-1",
        name: duplicatedName,
        targetPh: profile.targetPh,
        tolerance: profile.tolerance,
        doseSeconds: profile.doseSeconds,
        intervalMinutes: profile.intervalMinutes,
        maxDoses: profile.maxDoses,
        automaticMode: profile.automaticMode ? "true" : "false",
        targetEc: profile.targetEc,
        lightOn: profile.lightOn,
        lightOff: profile.lightOff
    });

    try {
        const response = await fetch("/api/profiles/save", {
            method: "POST",
            headers: {
                "Content-Type": "application/x-www-form-urlencoded"
            },
            body: data.toString()
        });

        const result = await readProfilesJson(response);

        if (!response.ok || !result.success) {
            throw new Error(result.message || "No se pudo duplicar el perfil.");
        }

        showProfilesMessage(
            "profilesPageMessage",
            `Perfil duplicado como "${duplicatedName}".`,
            true
        );
        await loadProfilesData();

    } catch (error) {
        console.error("Error duplicando perfil:", error);
        showProfilesMessage("profilesPageMessage", error.message, false);
    }
}

async function deleteSavedProfile(profile) {
    const confirmed = window.confirm(
        `¿Eliminar definitivamente el perfil "${profile.name}"?`
    );

    if (!confirmed) {
        return;
    }

    try {
        const response = await fetch("/api/profiles/delete", {
            method: "POST",
            headers: {
                "Content-Type": "application/x-www-form-urlencoded"
            },
            body: new URLSearchParams({ id: profile.id }).toString()
        });

        const result = await readProfilesJson(response);

        if (!response.ok || !result.success) {
            throw new Error(result.message || "No se pudo eliminar el perfil.");
        }

        showProfilesMessage("profilesPageMessage", result.message, true);
        await loadProfilesData();
        fillProfileForm(getCurrentProfileDefaults(), false);

    } catch (error) {
        console.error("Error eliminando perfil:", error);
        showProfilesMessage("profilesPageMessage", error.message, false);
    }
}

async function updateProfilesPage() {
    try {
        await loadProfilesData();
        fillProfileForm(getCurrentProfileDefaults(), false);

        document
            .getElementById("profileForm")
            ?.addEventListener("submit", saveProfileForm);

        document
            .getElementById("newProfileButton")
            ?.addEventListener("click", () => {
                fillProfileForm(getCurrentProfileDefaults(), false);
                scrollToProfileEditor();
                document.getElementById("profileName")?.focus();
            });

        document
            .getElementById("cancelProfileEditButton")
            ?.addEventListener("click", () => {
                fillProfileForm(getCurrentProfileDefaults(), false);
            });

        document
            .getElementById("profileLightOn")
            ?.addEventListener("input", updateProfilePhotoperiod);

        document
            .getElementById("profileLightOff")
            ?.addEventListener("input", updateProfilePhotoperiod);

    } catch (error) {
        console.error("Error iniciando la página de perfiles:", error);
        showProfilesMessage("profilesPageMessage", error.message, false);

        const list = document.getElementById("profilesList");

        if (list) {
            list.innerHTML = `
                <div class="profiles-empty-state error">
                    <i class="bi bi-exclamation-triangle"></i>
                    No se pudieron cargar los perfiles.
                </div>
            `;
        }
    }
}
