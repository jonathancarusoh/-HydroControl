async function loadPage(page) {

    const response = await fetch("pages/" + page + ".html");
    const html = await response.text();

    document.getElementById("page").innerHTML = html;

    switch (page) {

        case "dashboard":
            updateDashboard();
            break;

        case "ph":
            updatePhPage();
            break;
    }

}

document.querySelectorAll(".sidebar li").forEach(item => {

    item.addEventListener("click", () => {

        loadPage(item.dataset.page);

    });

});

loadPage("dashboard");