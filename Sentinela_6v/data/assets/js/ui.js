const alarmBanner = document.getElementById('alarm-banner');

function updateGasUI(ppm) {
    const gasVal = document.getElementById('gas-val');
    const gasBar = document.getElementById('gas-bar');
    const gasStatus = document.getElementById('gas-status');
    

    gasVal.innerText = ppm;

    let percentage = (ppm / 1000) * 100;
    if (percentage > 100) percentage = 100;
    gasBar.style.width = percentage + "%";

    if (ppm > 400) {
        gasBar.style.backgroundColor = "var(--danger)";
        gasStatus.innerText = "PERIGO: VAZAMENTO DETECTADO!";
        gasStatus.style.color = "var(--danger)";
        alarmBanner.classList.remove('hidden');
    } else if (ppm > 200) {
        gasBar.style.backgroundColor = "var(--warning)"; // corrigido
        gasStatus.innerText = "ALERTA: NÍVEL ELEVADO";
        gasStatus.style.color = "var(--warning)"; // corrigido
        alarmBanner.classList.add('hidden');
    } else {
        gasBar.style.backgroundColor = "var(--success)";
        gasStatus.innerText = "AMBIENTE SEGURO";
        gasStatus.style.color = "var(--success)";
        alarmBanner.classList.add('hidden');
    }
}

function updatePresencaUI(distancia) {
    const distVal = document.getElementById('dist-val');
    const presenceText = document.getElementById('presence-text');
    const presenceIndicator = document.getElementById('presence-indicator');

    distVal.innerText = Math.round(distancia);

    if (distancia > 2 && distancia < 100) {
        presenceIndicator.classList.add('detected');
        presenceText.innerText = "SALA OCUPADA";
        presenceText.style.color = "var(--accent-blue)";
    } else {
        presenceIndicator.classList.remove('detected');
        presenceText.innerText = "SALA VAZIA";
        presenceText.style.color = "var(--text-dim)";
    }
}

function updateServoUI(alarmActive) {
    const servoCircle = document.getElementById('servo-circle');
    const servoBadge  = document.getElementById('servo-badge');

    if (alarmActive) {
        servoCircle.classList.remove('circle-open');
        servoCircle.classList.add('circle-closed');
        servoBadge.classList.remove('badge-open');
        servoBadge.classList.add('badge-closed');
        servoBadge.innerText = "FECHADA";
    } else {
        servoCircle.classList.remove('circle-closed');
        servoCircle.classList.add('circle-open');
        servoBadge.classList.remove('badge-closed');
        servoBadge.classList.add('badge-open');
        servoBadge.innerText = "ABERTA";
    }
}

function updateEmergencyOverlay(alarmActive) {
    const overlay = document.getElementById('emergency-overlay');

    if (alarmActive) {
        overlay.classList.remove('hidden');
    } else {
        overlay.classList.add('hidden');
    }
}

function updateChamaUI(intensity) {
    const flameVal    = document.getElementById('flame-val');
    const flameBar    = document.getElementById('flame-bar');
    const flameStatus = document.getElementById('flame-status');
    const flameTrack  = document.getElementById('flame-bar').parentElement;

    flameVal.innerText = Math.round(intensity);

    let percentage = intensity;
    if (percentage > 100) percentage = 100;
    if (percentage < 0)   percentage = 0;
    flameBar.style.width = percentage + "%";

    if (intensity > 70) {
        flameTrack.classList.add('flame-critical');
        flameStatus.innerText = "PERIGO: CHAMA DETECTADA!";
        flameStatus.style.color = "var(--danger)";
        alarmBanner.classList.remove('hidden');
    } else if (intensity > 30) {
        flameTrack.classList.remove('flame-critical');
        flameStatus.innerText = "ALERTA: CALOR ELEVADO";
        flameStatus.style.color = "var(--warning)";
        alarmBanner.classList.add('hidden');
    } else {
        flameTrack.classList.remove('flame-critical');
        flameStatus.innerText = "AMBIENTE RESFRIADO";
        flameStatus.style.color = "var(--success)";
        alarmBanner.classList.add('hidden');
    }
}

function updateRFIDUI(rfidData) {
    const rfidContainer = document.getElementById('rfid-icon-container');
    const rfidIcon      = document.getElementById('rfid-icon');
    const userName      = document.getElementById('user-name');
    const cardUID       = document.getElementById('card-uid');
    const btnShutdown   = document.getElementById('btn-shutdown');

    if (rfidData.authorized) {
        // Cartão autorizado lido
        rfidContainer.classList.add('rfid-authorized');
        rfidIcon.style.color = "var(--success)";
        userName.innerText = rfidData.name || "USUÁRIO AUTORIZADO";
        cardUID.innerText  = "UID: " + (rfidData.uid || "---");

        // Habilita o botão de finalizar alerta
        btnShutdown.classList.remove('btn-disabled');
        btnShutdown.classList.add('btn-active-shutdown');
        btnShutdown.disabled = false;

        btnShutdown.onclick = () => {
            fetch('/api/shutdown', { method: 'POST' })
                .then(() => {
                    btnShutdown.classList.remove('btn-active-shutdown');
                    btnShutdown.classList.add('btn-disabled');
                    btnShutdown.disabled = true;
                    rfidContainer.classList.remove('rfid-authorized');
                    rfidIcon.style.color = "var(--text-dim)";
                    userName.innerText = "SISTEMA AGUARDANDO";
                    cardUID.innerText  = "APROXIME O CARTÃO MESTRE";
                })
                .catch(err => console.error("Erro ao finalizar alerta:", err));
        };

    } else {
        // Nenhum cartão ou cartão não autorizado
        rfidContainer.classList.remove('rfid-authorized');
        rfidIcon.style.color = "var(--text-dim)";
        userName.innerText = "SISTEMA AGUARDANDO";
        cardUID.innerText  = "APROXIME O CARTÃO MESTRE";
        btnShutdown.classList.remove('btn-active-shutdown');
        btnShutdown.classList.add('btn-disabled');
        btnShutdown.disabled = true;
    }
}

function updateDateTime() {
    const systemDate = document.getElementById('system-date');
    
    const agora = new Date();
    const opcoes = { 
        weekday: 'long', 
        day: '2-digit', 
        month: 'long', 
        year: 'numeric',
        hour: '2-digit', 
        minute: '2-digit', 
        second: '2-digit'
    };
    
    systemDate.innerText = agora.toLocaleDateString('pt-BR', opcoes);
}


// Atualiza imediatamente e depois a cada segundo
updateDateTime();
setInterval(updateDateTime, 1000);