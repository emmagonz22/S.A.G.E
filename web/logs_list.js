document.addEventListener("alpine:init", () => {
    Alpine.data("logsHandler", logsHandler);
});

// Logs handler function
function logsHandler() {
    return {
        logs: [],
        loading: true,
        
        init() {
            // Initialize by loading mock data 
            setTimeout(() => {
                this.logs = this.getMockLogData();
                this.loading = false;
                console.log(this.logs);
            }, 300);
        },
        
        // getMockLogData() {
        //     // Mock data for testing
        //     return [
        //         {
        //             id: "log123",
        //             fileName: "sensor_data_20230425.csv",
        //             date: "2023-04-25T14:30:45",
        //             formattedDate: "25/04/2023 | 2:30:45 PM"
        //         },
        //         {
        //             id: "log456",
        //             fileName: "sensor_data_20230426.csv",
        //             date: "2023-04-26T09:15:20",
        //             formattedDate: "26/04/2023 | 9:15:20 AM"
        //         },
        //         {
        //             id: "log789",
        //             fileName: "sensor_data_20230427.csv",
        //             date: "2023-04-27T18:45:10",
        //             formattedDate: "27/04/2023 | 6:45:10 PM"
        //         }
        //     ];
        // },
        
        // fetchLogData() {
        //     return new Promise((resolve) => {
        //         setTimeout(() => {
        //             resolve(this.getMockLogData());
        //         }, 300);
        //     });
        // },

        fetchLogData() {
            return fetch("/logs/list")
                .then(response => response.json())
                .then(data => {
                    // Format timestamps if needed, or just return raw
                    return data.map(item => ({
                        ...item,
                        formattedDate: new Date().toLocaleString() // Placeholder
                    }));
                });
        },
        
        init() {
            this.fetchLogData().then((data) => {
                this.logs = data;
                this.loading = false;
            });
        },
        


        goToLog(logId) {
            window.location.href = `log.html?id=${logId}`;
        },

    };
}

// For server implementation, you would need an endpoint that returns JSON like:
// [
//   {
//     "id": "unique-id-1",
//     "fileName": "data_20230425.csv",
//     "date": "2023-04-25T14:30:45",
//     "formattedDate": "25/04/2023 | 2:30:45 PM"
//   },
//   ...
// ]

// This endpoint would scan your data folder for CSV files, extract date information
// from filenames or file metadata, and return it in a structured format.