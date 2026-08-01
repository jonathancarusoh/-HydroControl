const hydro = {
    ph: 5.82,
    ec: 1.45,
    waterTemp: 18.5,
    humidity: 61,
    online: true
};

function simulateData() {

    hydro.ph += (Math.random() - 0.5) * 0.02;

    hydro.ec += (Math.random() - 0.5) * 0.01;

    hydro.waterTemp += (Math.random() - 0.5) * 0.05;

    hydro.humidity += (Math.random() - 0.5) * 0.3;

}