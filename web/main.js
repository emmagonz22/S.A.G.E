document.addEventListener("alpine:init", () => {
    
    Alpine.data("themeHandler", () => ({
        theme: localStorage.getItem('theme') || 
                (window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light'),
       
        init() {
            console.log("Theme initialize");
            document.documentElement.setAttribute('data-theme', this.theme);
        },

        toggleTheme() {
            this.theme = this.theme === 'dark' ? 'light' : 'dark';
            document.documentElement.setAttribute('data-theme', this.theme);
            localStorage.setItem('theme', this.theme);
            console.log("Theme toggled:", this.theme); // Debugging
        }
    }));
});