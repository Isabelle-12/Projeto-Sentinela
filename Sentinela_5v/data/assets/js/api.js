// assets/js/api.js - Foco: Gás e Presença
const API_URL = "/api/data"; 

async function fetchSensorData() {
    try {
        const response = await fetch(API_URL);
        
        if (!response.ok) throw new Error("Erro na conexão");

        const data = await response.json();
        
        // --- ATUALIZAÇÃO DA UI (FOCO ATUAL) ---
        // Verifica se as funções existem no ui.js antes de chamar
        if (typeof updateGasUI === "function") {
            updateGasUI(data.gasPPM);
        }
        
        if (typeof updatePresencaUI === "function") {
            updatePresencaUI(data.distancia);
        }

        // --- STATUS DA CONEXÃO ---
        const statusText = document.getElementById('wifi-status-text');
        if (statusText) {
            statusText.innerText = "ONLINE";
            statusText.style.color = "var(--success)";
        }

    } catch (error) {
        console.error("Erro na API:", error);
        
        // Feedback visual de desconexão
        const statusText = document.getElementById('wifi-status-text');
        if (statusText) {
            statusText.innerText = "OFFLINE";
            statusText.style.color = "var(--danger)";
        }
    }
}

// Inicia a busca de dados e repete a cada 1 segundo (1000ms)
setInterval(fetchSensorData, 1000);