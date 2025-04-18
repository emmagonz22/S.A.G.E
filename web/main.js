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
