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