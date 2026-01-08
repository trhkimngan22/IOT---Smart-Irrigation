/**
 * dashboard.js
 */
new Vue({
    el: '#app',
    data: {
        settings: {
            soilLimit: 30, 
            humLimit: 40,
            tempLimit: 35
        },
        historyData: [],
        waterUsageMonth: 0,
        soilMoisture: 0,
        myChart: null
    },
    mounted() {
        console.log("Vue App Ready!");
        uibuilder.start({ 'serverPath': '/home' });
        this.initChart();
        uibuilder.send({ topic: "get_settings" });
        // // Request history data
        // uibuilder.send({ topic: "get_history" });

        uibuilder.onChange('msg', (msg) => {
            if (!msg.payload) return;
            // if (msg.topic === "current_settings") {
            //     console.log("Đã khôi phục cài đặt:", msg.payload);
            //     this.settings.soilLimit = msg.payload.soilLimit;
            //     this.settings.humLimit = msg.payload.humLimit;
            //     this.settings.tempLimit = msg.payload.tempLimit;
            // }
            // if (msg.topic === "settings_saved") {
            //     alert("Đã lưu cài đặt thành công!");
            // }
            if (msg.topic === "history_table") {
                console.log("Received history table:", msg.payload.table);
                this.historyData = msg.payload.table;
            }
            if (msg.payload.table && !msg.topic) this.historyData = msg.payload.table;
            if (msg.payload.soil !== undefined) this.soilMoisture = msg.payload.soil;
            if (msg.payload.waterUsageMonth !== undefined) this.waterUsageMonth = msg.payload.waterUsageMonth;
            if (msg.payload.chart && this.myChart) this.updateChart(msg.payload.chart);
        });
    },
    methods: {
        saveSettings() {
            uibuilder.send({
                topic: "cmd_settings",
                payload: {
                    soilLimit: this.settings.soilLimit,
                    humLimit: this.settings.humLimit,
                    tempLimit: this.settings.tempLimit
                }
            });
        },
        initChart() {
            var ctx = document.getElementById('myChart');
            if (!ctx) return;
            this.myChart = new Chart(ctx.getContext('2d'), {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [
                        { label: 'Temp', borderColor: '#3498db', yAxisID: 'y', data: [] },
                        { label: 'Hum', borderColor: '#e74c3c', yAxisID: 'y1', data: [] }
                    ]
                },
                options: {
                    responsive: true, maintainAspectRatio: false,
                    scales: {
                        y: { position: 'left', beginAtZero: false },
                        y1: { position: 'right', min: 0, max: 100 }
                    }
                }
            });
        },
        updateChart(chartData) {
            this.myChart.data.labels = chartData.labels;
            this.myChart.data.datasets[0].data = chartData.temp;
            this.myChart.data.datasets[1].data = chartData.hum;
            this.myChart.update();
        }
    }
});