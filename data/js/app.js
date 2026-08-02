async function loadPage(page) {
    const response = await fetch(
        "pages/" + page + ".html"
    );

    if (!response.ok) {
        throw new Error(
            `No se pudo cargar la página: ${page}`
        );
    }

    const html = await response.text();

    document.getElementById("page").innerHTML = html;

    switch (page) {
        case "dashboard":
            updateDashboard();
            break;

        case "ph":
            updatePhPage();
            break;

        case "wifi":
            updateWifiPage();
            break;
    }
}

document.querySelectorAll(".sidebar li").forEach(item => {

    item.addEventListener("click", () => {

        loadPage(item.dataset.page);

    });

});

loadPage("dashboard");