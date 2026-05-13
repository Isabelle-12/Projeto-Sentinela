const API_URL = "/api/data";

async function fetchSensorData() {
    try {
        const response = await fetch(API_URL);
        
        if (!response.ok) throw new Error("Erro na conexão");

        const data = await response.json();

        if (typeof updateGasUI === "function") {
            updateGasUI(data.gas);
        }
        
        if (typeof updatePresencaUI === "function") {
            updatePresencaUI(data.dist);
        }

        if (typeof updateServoUI === "function") {
            updateServoUI(data.alarm);
        }
        if (typeof updateEmergencyOverlay === "function") {
            updateEmergencyOverlay(data.alarm);
        }

        if (typeof updateChamaUI === "function") {
            updateChamaUI(data.flame);
        }
        if (typeof updateRFIDUI === "function" && data.rfid) {
            updateRFIDUI(data.rfid);
        }

        const statusText = document.getElementById('wifi-status-text');
        if (statusText) {
            statusText.innerText = "ONLINE";
            statusText.style.color = "var(--success)";
        }
        

    } catch (error) {
        console.error("Erro na API:", error);
        
        const statusText = document.getElementById('wifi-status-text');
        if (statusText) {
            statusText.innerText = "OFFLINE";
            statusText.style.color = "var(--danger)";
        }
    }
}

fetchSensorData();
setInterval(fetchSensorData, 1000);