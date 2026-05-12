// Função para atualizar o Sensor de Gás
function updateGasUI(ppm) {
    const gasVal = document.getElementById('gas-val');
    const gasBar = document.getElementById('gas-bar');
    const gasStatus = document.getElementById('gas-status');
    const alarmBanner = document.getElementById('alarm-banner');

    // Atualiza o valor numérico
    gasVal.innerText = ppm;

    // Calcula a porcentagem da barra (Base: 1000 PPM)
    let percentage = (ppm / 1000) * 100;
    if (percentage > 100) percentage = 100;
    gasBar.style.width = percentage + "%";

    // Lógica de Alerta de Segurança
    if (ppm > 400) { 
        gasBar.style.backgroundColor = "var(--danger)";
        gasStatus.innerText = "PERIGO: VAZAMENTO DETECTADO!";
        gasStatus.style.color = "var(--danger)";
        alarmBanner.classList.remove('hidden'); // Exibe o banner de alerta
    } else if (ppm > 200) {
        gasBar.style.backgroundColor = "var(--warn)";
        gasStatus.innerText = "ALERTA: NÍVEL ELEVADO";
        gasStatus.style.color = "var(--warn)";
        alarmBanner.classList.add('hidden');
    } else {
        gasBar.style.backgroundColor = "var(--success)";
        gasStatus.innerText = "AMBIENTE SEGURO";
        gasStatus.style.color = "var(--success)";
        alarmBanner.classList.add('hidden');
    }
}

// Função para atualizar o Sensor de Presença (Ultrassônico)
function updatePresencaUI(distancia) {
    const distVal = document.getElementById('dist-val');
    const presenceText = document.getElementById('presence-text');
    const presenceIndicator = document.getElementById('presence-indicator');

    // Atualiza a distância medida
    distVal.innerText = Math.round(distancia);

    // Lógica de Ocupação (Limite de 100cm)
    // O "distancia > 2" filtra erros comuns de leitura (0 ou 1)
    if (distancia > 2 && distancia < 100) {
        presenceIndicator.classList.add('presence-active');
        presenceText.innerText = "SALA OCUPADA";
        presenceText.style.color = "var(--accent-blue)";
    } else {
        presenceIndicator.classList.remove('presence-active');
        presenceText.innerText = "SALA VAZIA";
        presenceText.style.color = "var(--text-dim)";
    }
}