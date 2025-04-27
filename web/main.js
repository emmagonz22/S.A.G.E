document.addEventListener("alpine:init", () => {
    Alpine.data("themeHandler", themeHandler);
});

// Theme handler function
function themeHandler() {
    return {
        theme: 'light',
        init() {
            this.theme = localStorage.getItem('theme') || 'light';
            document.documentElement.setAttribute('data-theme', this.theme);
        },
        toggleTheme() {
            this.theme = this.theme === 'light' ? 'dark' : 'light';
            document.documentElement.setAttribute('data-theme', this.theme);
            localStorage.setItem('theme', this.theme);
        }
    };
}

// Device Name Set Up
function setDeviceName() {
    const deviceName = document.getElementById("DeviceName").value;

    fetch('/set_name', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({ name: deviceName })
    })
    .then(response => {
        if (response.ok) {
            console.log("Device name set successfully.");
        } else {
            console.error("Failed to set device name.");
        }
    })
    .catch(error => console.error("Error:", error));
}

// Summarize Logs
function getQueryParam(param) {
    const params = new URLSearchParams(window.location.search);
    return params.get(param);
}

async function loadLogSummary() {
    const fileId = getQueryParam('id') || 'Test_s_log.csv';

    try {
        const response = await fetch(`/api/log_summary?id=${fileId}`);
        const data = await response.json();

        const logCards = document.querySelectorAll('.log-card');
        const labels = ["pH", "Fertility", "Enviroment Moisture", "Enviroment Temperature", "Soil Moisture", "Soil Temperature"];
        const keys = ["pH", "Fertility", "Moisture", "AirTemp", "Humidity", "SoilTemp"];

        logCards.forEach((card, index) => {
            const label = labels[index] || `Unknown ${index}`;
            const key = keys[index];
            const value = data[key] !== undefined ? data[key] : "#";
            card.innerHTML = `<strong>${label}</strong><br>${value}`;
        });

    } catch (err) {
        console.error("Failed to fetch log summary", err);
    }
}

document.addEventListener("DOMContentLoaded", loadLogSummary);

