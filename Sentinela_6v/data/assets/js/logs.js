const LOGS_URL = "/api/logs";
let todosOsLogs = []; // Armazena todos os logs recebidos

async function fetchLogs() {
    try {
        const response = await fetch(LOGS_URL);

        if (!response.ok) throw new Error("Erro na conexão");

        const data = await response.json();

        const statusText = document.getElementById('wifi-status-text');
        if (statusText) {
            statusText.innerText = "ONLINE";
            statusText.style.color = "var(--success)";
        }

        // Só adiciona logs novos que ainda não estão na tela
        data.logs.forEach(log => {
            if (!todosOsLogs.find(l => l.id === log.id)) {
                todosOsLogs.push(log);
                renderizarLog(log);
            }
        });

    } catch (error) {
        console.error("Erro ao buscar logs:", error);

        const statusText = document.getElementById('wifi-status-text');
        if (statusText) {
            statusText.innerText = "OFFLINE";
            statusText.style.color = "var(--danger)";
        }
    }
}

function renderizarLog(log) {
   const logConsole = document.getElementById('log-console');

    const entry = document.createElement('div');
    entry.classList.add('log-entry', `log-${log.type}`);
    entry.dataset.type = log.type;

    entry.innerHTML = `
        <span class="log-time">[${log.time}]</span>
        <span class="log-msg">${log.msg}</span>
    `;

   logConsole.appendChild(entry);

    // Auto-scroll para o final
    logConsole.scrollTop = logConsole.scrollHeight;
}

function inicializarFiltros() {
    const filterBtns = document.querySelectorAll('.filter-btn');

    filterBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            // Atualiza botão ativo
            filterBtns.forEach(b => b.classList.remove('active'));
            btn.classList.add('active');

            const tipo = btn.dataset.type;
            const entries = document.querySelectorAll('.log-entry');

            entries.forEach(entry => {
                if (tipo === 'all' || entry.dataset.type === tipo) {
                    entry.style.display = 'flex';
                } else {
                    entry.style.display = 'none';
                }
            });
        });
    });
}
function exportarLogs() {
    let conteudo = "=== LOGS DO SISTEMA SENTINELA ===\n";
    conteudo += `Exportado em: ${new Date().toLocaleString('pt-BR')}\n`;
    conteudo += "=================================\n\n";

    const entries = document.querySelectorAll('.log-entry');
    
    entries.forEach(entry => {
        const time = entry.querySelector('.log-time')?.innerText || '';
        const msg  = entry.querySelector('.log-msg')?.innerText  || '';
        const type = entry.dataset.type.toUpperCase();
        conteudo += `${time} [${type}] ${msg}\n`;
    });

    // Cria o arquivo e faz o download
    const blob = new Blob([conteudo], { type: 'text/plain' });
    const url  = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href     = url;
    link.download = `sentinela_logs_${new Date().toLocaleDateString('pt-BR').replace(/\//g, '-')}.txt`;
    link.click();

    URL.revokeObjectURL(url);
}

function limparLogsLocal() {
    const logConsole = document.getElementById('log-console');
    logConsole.innerHTML = '';
    todosOsLogs = [];
}

// Chama ao carregar a página
inicializarFiltros();
fetchLogs();
setInterval(fetchLogs, 3000);