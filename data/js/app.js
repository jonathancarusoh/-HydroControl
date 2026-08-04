const sidebar =
    document.getElementById("sidebar");

const sidebarOverlay =
    document.getElementById("sidebarOverlay");

const mobileMenuButton =
    document.getElementById("mobileMenuButton");

const sidebarCloseButton =
    document.getElementById("sidebarCloseButton");

function openMobileMenu() {
    sidebar?.classList.add("open");
    sidebarOverlay?.classList.add("visible");
    document.body.classList.add("menu-open");
}

function closeMobileMenu() {
    sidebar?.classList.remove("open");
    sidebarOverlay?.classList.remove("visible");
    document.body.classList.remove("menu-open");
}

function setActiveMenuItem(page) {
    document
        .querySelectorAll(".sidebar li")
        .forEach(item => {
            item.classList.toggle(
                "active",
                item.dataset.page === page
            );
        });
}

async function loadPage(page) {
    const pageContainer =
        document.getElementById("page");

    try {
        const response = await fetch(
            "pages/" + page + ".html",
            {
                cache: "no-store"
            }
        );

        if (!response.ok) {
            throw new Error(
                `No se pudo cargar la página: ${page}`
            );
        }

        const html = await response.text();

        pageContainer.innerHTML = html;

        setActiveMenuItem(page);
        closeMobileMenu();

        window.scrollTo({
            top: 0,
            behavior: "instant"
        });

        switch (page) {
            case "dashboard":
                updateDashboard();
                break;

            case "ph":
                updatePhPage();
                break;

            case "profiles":
                updateProfilesPage();
                break;

            case "system":
                updateSystemPage();
                break;

            case "wifi":
                updateWifiPage();
                break;
        }

    } catch (error) {
        console.error(
            "Error cargando página:",
            error
        );

        pageContainer.innerHTML = `
            <div class="alert alert-danger">
                No se pudo cargar esta sección.
            </div>
        `;
    }
}

document
    .querySelectorAll(".sidebar li")
    .forEach(item => {

        item.addEventListener("click", () => {
            loadPage(item.dataset.page);
        });

    });

mobileMenuButton?.addEventListener(
    "click",
    openMobileMenu
);

sidebarCloseButton?.addEventListener(
    "click",
    closeMobileMenu
);

sidebarOverlay?.addEventListener(
    "click",
    closeMobileMenu
);

document.addEventListener(
    "keydown",
    event => {

        if (event.key === "Escape") {
            closeMobileMenu();
        }

    }
);

window.addEventListener(
    "resize",
    () => {

        if (window.innerWidth > 991) {
            closeMobileMenu();
        }

    }
);

loadPage("dashboard");