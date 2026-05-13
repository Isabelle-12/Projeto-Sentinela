const PERF_URL = "/api/performance";
const MAX_POINTS = 30;

const ctx = document.getElementById('perfChart').getContext('2d');
const perfChart = new Chart(ctx, {
    type: 'line',
    data: {
        labels: [],
        datasets: [{
            label: 'Heap Livre (bytes)',
            data: [],
            borderColor: '#58a6ff',
            backgroundColor: 'rgba(88, 166, 255, 0.1)',
            borderWidth: 2,
            pointRadius: 3,
            pointBackgroundColor: '#58a6ff',
            tension: 0.4,
            fill: true
        }]
    },
    options: {
        responsive: true,
        maintainAspectRatio: false,
        animation: { duration: 300 },
        scales: {
            x: {
                ticks: { color: '#8b949e', maxTicksLimit: 8 },
                grid:  { color: '#21262d' }
            },
            y: {
                ticks: { color: '#8b949e' },
                grid:  { color: '#21262d' },
                beginAtZero: false
            }
        },
        plugins: {
            legend: { labels: { color: '#c9d1d9' } }
        }
    }
});

async function fetchPerfData() {
    try {
        const response = await fetch(PERF_URL);

        if (!response.ok) throw new Error("Erro na conexão");

        const data = await response.json();

        const heapVal = document.getElementById('heap-val');
        const tSensor = document.getElementById('t-sensor');
        const tLogic  = document.getElementById('t-logic');
        const tWeb    = document.getElementById('t-web');
        const statusText = document.getElementById('wifi-status-text');

        if (heapVal) heapVal.innerText = data.heap.toLocaleString('pt-BR');
        if (tSensor) tSensor.innerText = data.tasks[0] + " ms";
        if (tLogic)  tLogic.innerText  = data.tasks[1] + " ms";
        if (tWeb)    tWeb.innerText    = data.tasks[3] + " ms";

        if (statusText) {
            statusText.innerText = "ONLINE";
            statusText.style.color = "var(--success)";
        }

        const agora = new Date().toLocaleTimeString('pt-BR');
        perfChart.data.labels.push(agora);
        perfChart.data.datasets[0].data.push(data.heap);

        if (perfChart.data.labels.length > MAX_POINTS) {
            perfChart.data.labels.shift();
            perfChart.data.datasets[0].data.shift();
        }

        perfChart.update();

    } catch (error) {
        console.error("Erro na API:", error);

        const statusText = document.getElementById('wifi-status-text');
        if (statusText) {
            statusText.innerText = "OFFLINE";
            statusText.style.color = "var(--danger)";
        }
    }
}

fetchPerfData();
setInterval(fetchPerfData, 2000);