/**
 * index.js
 * Logic điều khiển cho Plant Dashboard
 */

new Vue({
    el: '#app',

    // Khai báo các biến sẽ hiển thị trên giao diện
    data: {
        historyData: [], // Dữ liệu cho bảng lịch sử
        waterLevel: 0,   // Biến Mực nước (thay cho waterUsage)
        soilMoisture: 0, // Độ ẩm đất
        myChart: null    // Biến lưu đối tượng biểu đồ
    },

    mounted() {
        console.log("Vue App đã khởi động!");

        // 1. Khởi động kết nối uibuilder với Node-RED
        uibuilder.start();

        // 2. Cấu hình và tạo Biểu đồ (Chart.js)
        var ctx = document.getElementById('myChart');

        if (ctx) {
            this.myChart = new Chart(ctx.getContext('2d'), {
                type: 'line', // Loại biểu đồ đường
                data: {
                    labels: [], // Nhãn thời gian (sẽ được cập nhật từ Node-RED)
                    datasets: [
                        {
                            label: 'Temperature (°C)',
                            borderColor: '#3498db', // Màu xanh dương
                            backgroundColor: 'rgba(52, 152, 219, 0.1)', // Màu nền mờ
                            borderWidth: 2,
                            yAxisID: 'y', // Trục Y bên trái
                            tension: 0.4, // Làm mềm đường kẻ (cong nhẹ)
                            pointRadius: 3,
                            data: [] // Dữ liệu nhiệt độ
                        },
                        {
                            label: 'Humidity (%)',
                            borderColor: '#e74c3c', // Màu đỏ
                            backgroundColor: 'rgba(231, 76, 60, 0.1)',
                            borderWidth: 2,
                            yAxisID: 'y1', // Trục Y bên phải
                            tension: 0.4,
                            pointRadius: 3,
                            data: [] // Dữ liệu độ ẩm
                        }
                    ]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false, // Để biểu đồ tự co giãn theo khung
                    interaction: {
                        mode: 'index',
                        intersect: false,
                    },
                    scales: {
                        y: {
                            type: 'linear',
                            display: true,
                            position: 'left',
                            title: { display: true, text: 'Temperature (°C)' },
                            beginAtZero: true
                        },
                        y1: {
                            type: 'linear',
                            display: true,
                            position: 'right',
                            grid: {
                                drawOnChartArea: false, // Ẩn lưới ngang của trục bên phải cho đỡ rối
                            },
                            title: { display: true, text: 'Humidity (%)' },
                            beginAtZero: true,
                            max: 100 // Độ ẩm tối đa 100%
                        }
                    }
                }
            });
        } else {
            console.error("Lỗi: Không tìm thấy thẻ <canvas id='myChart'> trong HTML");
        }

        // 3. Lắng nghe dữ liệu gửi xuống từ Node-RED
        uibuilder.onChange('msg', (msg) => {
            // Kiểm tra xem có dữ liệu payload không
            if (!msg.payload) return;

            console.log("Dữ liệu mới nhận:", msg.payload);

            // --- CẬP NHẬT CÁC BIẾN VUE ---

            // 1. Cập nhật Bảng lịch sử
            this.historyData = msg.payload.table || [];

            // 2. Cập nhật Độ ẩm đất
            this.soilMoisture = msg.payload.soil || 0;

            // 3. Cập nhật Mực nước (Gán vào biến waterLevel)
            // msg.payload.water lúc này là số % đã tính trong Node Function
            this.waterLevel = msg.payload.water || 0;

            // --- CẬP NHẬT BIỂU ĐỒ ---
            if (this.myChart && msg.payload.chart) {
                // Cập nhật nhãn thời gian (Trục X)
                this.myChart.data.labels = msg.payload.chart.labels;

                // Cập nhật dữ liệu Nhiệt độ (Dataset 0)
                this.myChart.data.datasets[0].data = msg.payload.chart.temp;

                // Cập nhật dữ liệu Độ ẩm (Dataset 1)
                this.myChart.data.datasets[1].data = msg.payload.chart.hum;

                // Vẽ lại biểu đồ
                this.myChart.update();
            }
        });
    }
});