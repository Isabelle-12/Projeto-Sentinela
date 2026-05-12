#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <LittleFS.h>
#include <sqlite3.h>
#include <esp_task_wdt.h>

//Configuracoes de rede e banco de dados 
const char* WIFI_SSID = "nome_rede"; // aquela propria do esp32
const char* WIFI_PASS = "senha_rede";


const char* db_path = "/littlefs/sentinela.db";
sqlite3 *db_sentinela;

#define PIN_MQ2 34
#define PIN_SERVO 13
#define PIN_RELAY 14      
#define PIN_TRIG 12      
#define PIN_ECHO 27
#define PIN_BUZZER 5
#define GAS_THRESHOLD 250 // Reduzido para maior sensibilidade com isqueiro

struct SensorData {
    int gasLevel;
    float distance;
    unsigned long timestamp;
};

struct SystemMetrics {
    uint32_t taskExecTime[5]; 
    uint32_t freeHeap;
    volatile int gasLevel;    // Volatile para atualização em tempo real entre tasks
    volatile float distance;
    bool alarmActive;
} metrics;

WebServer server(80);
Servo gasValve;
QueueHandle_t sensorQueue = NULL;
unsigned long lastGasTime = 0;
const uint32_t MIN_TIME_ACTIVE = 15000; // Aumentado para 15 segundos

TaskHandle_t hSensors, hSecurity, hWeb, hSQLite, hPerf;

// --- Funções Auxiliares SQLite ---
int db_open(const char *filename, sqlite3 **db) {
    int rc = sqlite3_open(filename, db);
    if (rc) Serial.printf("Erro banco: %s\n", sqlite3_errmsg(*db));
    return rc;
}

int db_exec(sqlite3 *db, const char *sql) {
    char *zErrMsg = 0;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &zErrMsg);
    if (rc != SQLITE_OK) {
        Serial.printf("Erro SQL: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    return rc;
}

// --- TASKS ---

void taskSensors(void *p) {
    for (;;) {
        uint32_t start = millis();
        int rawGas = analogRead(PIN_MQ2);
        
        // FILTRO: Se o alarme estiver ON e a leitura cair bruscamente (< 15),
        // ignoramos essa leitura específica para evitar o falso "ambiente seguro".
        if (!(metrics.alarmActive && rawGas < 15)) {
            metrics.gasLevel = rawGas;
        } else {
            // Opcional: Serial.println("DEBUG: Leitura de gás ignorada por queda de tensão.");
        }

        // Leitura Distância
        digitalWrite(PIN_TRIG, LOW); delayMicroseconds(5);
        digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
        digitalWrite(PIN_TRIG, LOW);
        long duration = pulseIn(PIN_ECHO, HIGH, 25000);
        metrics.distance = (duration == 0) ? 999.0 : (duration * 0.034 / 2);

        // Envia para a fila para gravação no SQLite
        SensorData currentRead;
        currentRead.gasLevel = metrics.gasLevel;
        currentRead.distance = metrics.distance;
        if (sensorQueue != NULL) xQueueOverwrite(sensorQueue, &currentRead);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void taskSecurity(void *p) {
    int confirmaLeitura = 0; // Contador para evitar disparos por picos rápidos
    unsigned long fimAlarmeTime = 0; // Para ignorar leituras logo após desligar

    for (;;) {
        int currentGas = metrics.gasLevel;
        unsigned long agora = millis();

        // Só tenta ler se não estiver no tempo de espera (2s) após o último desligamento
        if (agora - fimAlarmeTime > 2000) {
            
            // LÓGICA DE ATIVAÇÃO COM CONFIRMAÇÃO
            if (currentGas > GAS_THRESHOLD && currentGas > 10) {
                confirmaLeitura++;
                if (confirmaLeitura >= 2) { // Precisa de 2 leituras altas seguidas
                    if (!metrics.alarmActive) {
                        db_exec(db_sentinela, "INSERT INTO alertas (tipo, valor) VALUES ('GAS', 1);");
                        Serial.println("!!! ALERTA: ACIONANDO SISTEMAS DE SEGURANÇA !!!");
                        
                        metrics.alarmActive = true;
                        digitalWrite(PIN_BUZZER, HIGH);
                        vTaskDelay(pdMS_TO_TICKS(500)); 
                        digitalWrite(PIN_RELAY, HIGH);
                        gasValve.write(90);
                    }
                    lastGasTime = agora;
                }
            } else {
                confirmaLeitura = 0; // Reseta se a leitura baixar
            }
        }

        // LÓGICA DE DESATIVAÇÃO
        if (metrics.alarmActive && (agora - lastGasTime > MIN_TIME_ACTIVE)) {
            Serial.println(">>> Ambiente seguro. Desativando hardware.");
            digitalWrite(PIN_BUZZER, LOW);
            digitalWrite(PIN_RELAY, LOW);
            gasValve.write(0);
            metrics.alarmActive = false;
            fimAlarmeTime = millis(); // Inicia o tempo de "descanso" de 2s
        }

        vTaskDelay(pdMS_TO_TICKS(150)); // Aumentado levemente para estabilidade
    }
}


void taskSQLite(void *p) {
    SensorData toSave;
    for (;;) {
        // Espera dado na fila
        if (sensorQueue != NULL && xQueueReceive(sensorQueue, &toSave, portMAX_DELAY)) {
            char query[128];
            snprintf(query, sizeof(query), 
                     "INSERT INTO sensores (nivel_gas, distancia) VALUES (%d, %.2f);", 
                     toSave.gasLevel, toSave.distance);
            db_exec(db_sentinela, query);
        }
        vTaskDelay(pdMS_TO_TICKS(15000)); // Grava log a cada 15s
    }
}

void taskWeb(void *p) {
    server.on("/api/data", []() {
        String json = "{\"gas\":" + String(metrics.gasLevel) + ",\"dist\":" + String(metrics.distance) + ",\"alarm\":" + (metrics.alarmActive ? "true" : "false") + "}";
        server.send(200, "application/json", json);
    });
    server.begin();
    for (;;) {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void taskPerf(void *p) {
    for (;;) {
        metrics.freeHeap = ESP.getFreeHeap();
        Serial.println("\n=== SENTINELA PERF MONITOR ===");
        Serial.printf("METRICAS: Gas=%d | Dist=%.2f | Alarme=%s\n", metrics.gasLevel, metrics.distance, metrics.alarmActive ? "ON" : "OFF");
        Serial.printf("MEMORIA: Heap Livre: %u bytes\n", metrics.freeHeap);
        Serial.println("===============================");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void setup() {
    Serial.begin(115200);
    delay(500); // Estabilização elétrica

    // 1. Inicialização da Fila
    sensorQueue = xQueueCreate(1, sizeof(SensorData)); 
    
    // 2. Sistema de Arquivos e Banco de Dados
    if (!LittleFS.begin(true)) Serial.println("Erro LittleFS");
    
    if (db_open(db_path, &db_sentinela) == SQLITE_OK) {
        db_exec(db_sentinela, "CREATE TABLE IF NOT EXISTS sensores (id INTEGER PRIMARY KEY, nivel_gas INTEGER, distancia REAL, data_hora DATETIME DEFAULT (datetime('now','localtime')));");
        db_exec(db_sentinela, "CREATE TABLE IF NOT EXISTS alertas (id INTEGER PRIMARY KEY, tipo TEXT, valor REAL, data_hora DATETIME DEFAULT (datetime('now','localtime')));");
    }

    // 3. Configuração de Pinos e Periféricos
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_RELAY, OUTPUT);
    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);
    
    gasValve.setPeriodHertz(50);
    gasValve.attach(PIN_SERVO, 500, 2400);
    gasValve.write(0);

    // 4. Conectividade
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Conectando WiFi...");

    // 5. Nova Configuração do Watchdog (Corrigindo o erro "Task Not Found")
    // Primeiro, desinicializamos qualquer resquício para evitar conflito
    esp_task_wdt_deinit(); 

    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 15000, // 15 segundos
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = false // Mudamos para false para evitar reboots enquanto testamos os sensores
    };
    
    esp_task_wdt_init(&twdt_config); 
    esp_task_wdt_add(NULL); // Registra a task atual (setup/loop) no Watchdog

    // 6. Escalonamento de Tarefas (Tasks)
    // Core 0: Tarefas críticas de tempo real (sensores e segurança)
    xTaskCreatePinnedToCore(taskSensors,  "Sensors",  4096, NULL, 5, &hSensors,  0);
    xTaskCreatePinnedToCore(taskSecurity, "Security", 4096, NULL, 4, &hSecurity, 0);
    
    // Core 1: Tarefas de monitoramento, comunicação e persistência
    xTaskCreatePinnedToCore(taskWeb,      "Web",      8192, NULL, 3, &hWeb,      1);
    xTaskCreatePinnedToCore(taskPerf,     "Perf",     4096, NULL, 2, &hPerf,     1);
    xTaskCreatePinnedToCore(taskSQLite,   "SQLite",   8192, NULL, 1, &hSQLite,   1);

    Serial.println("\nSistema Sentinela Inicializado!");
}

void loop() {
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(1000));
}
