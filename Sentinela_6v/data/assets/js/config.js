async function enviarComando(cmd) {
    try {
        const response = await fetch(`/api/command/${cmd}`, { method: 'POST' });
        
        if (!response.ok) throw new Error("Erro ao enviar comando");
        
        console.log(`Comando enviado: ${cmd}`);
    } catch (error) {
        console.error("Erro ao enviar comando:", error);
    }
}

async function salvarConfig() {
    const gasLimit  = document.getElementById('set-gas').value;
    const distLimit = document.getElementById('set-dist').value;

    try {
        const response = await fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                gasThreshold:  parseInt(gasLimit),
                distThreshold: parseInt(distLimit)
            })
        });

        if (!response.ok) throw new Error("Erro ao salvar");

        alert("Parâmetros salvos com sucesso!");
    } catch (error) {
        console.error("Erro ao salvar configurações:", error);
        alert("Erro ao salvar. Verifique a conexão.");
    }
}

async function modoAprender() {
    try {
        const response = await fetch('/api/rfid/learn', { method: 'POST' });
        
        if (!response.ok) throw new Error("Erro ao ativar modo aprendizado");

        alert("Modo aprendizado ativado!\nAproximie a nova tag do leitor RFID.\nO sistema registrará automaticamente.");
    } catch (error) {
        console.error("Erro ao ativar modo aprendizado:", error);
        alert("Erro ao ativar. Verifique a conexão.");
    }
}

async function salvarTags() {
    const rows = document.querySelectorAll('#rfid-list tr');
    const tags = [];

    rows.forEach(row => {
        const uid  = row.querySelector('.uid-tag')?.innerText.trim();
        const name = row.querySelector('.input-inline')?.value.trim();
        
        if (uid && name) {
            tags.push({ uid, name });
        }
    });

    try {
        const response = await fetch('/api/rfid/tags', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ tags })
        });

        if (!response.ok) throw new Error("Erro ao salvar tags");

        alert("Tags salvas com sucesso!");
    } catch (error) {
        console.error("Erro ao salvar tags:", error);
        alert("Erro ao salvar. Verifique a conexão.");
    }
}

function removerTag(btn) {
    const row = btn.closest('tr');
    row.remove();
}