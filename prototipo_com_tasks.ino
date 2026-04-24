#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// estrutura de performance e monitoramento
struct SystemMetrics {
    float cpuUsage;
    uint32_t freeHeap;
    uint32_t taskExecTime[5]; // Tempo de execucao das 5 tasks
    int gasLevel;
    float distance;
    bool alarmActive;
};

SystemMetrics metrics;
WebServer server(80);
Servo gasValve;

// definicao dos pinos
// relevem os nomes esqueci os nomer certos dos componentes :)
// PINOS FICTICIOS TROCAR PARA OS QUE A GNT REALMENTE USA!!
#define PIN_MQ2 34 
#define PIN_SERVO 13
#define PIN_RELAY 14     // Atuador 2 "valvula"
#define PIN_TRIG 12      // Sensor 2 ultrassonico
#define PIN_ECHO 27
#define PIN_BUZZER 5

// PROTOTIPO das tasks (sao uma "base", provavelmente tem que ajustar coisa nelas)
void taskSensorReading(void *pvParameters);
void taskSecurityLogic(void *pvParameters);
void taskWebServer(void *pvParameters);
void taskPerformanceMonitor(void *pvParameters);
void taskConnectivityEnergy(void *pvParameters);

void setup() {
    Serial.begin(115200);
    
    // Inicialização de Hardware
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_RELAY, OUTPUT);
    gasValve.attach(PIN_SERVO);
    
    // Configuracao Wi-Fi (Modo Access Point Inicial)
    WiFi.softAP("Sentinela_ESP32", "12345678");

    // Criacao das Tasks (distribuodas nos Cores 0 e 1 - evita sobrecarregar e comeca a trabalhar concorrencia)
    xTaskCreatePinnedToCore(taskSensorReading, "Sensors", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(taskSecurityLogic, "Security", 4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(taskWebServer, "Web", 8192, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(taskPerformanceMonitor, "Perf", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(taskConnectivityEnergy, "Energy", 4096, NULL, 1, NULL, 0);
}

// Implementacao das Task

// 1. Task de Leitura de Sensores 
void taskSensorReading(void *pvParameters) {
    for (;;) {
        uint32_t start = millis();
        metrics.gasLevel = analogRead(PIN_MQ2);
        // Logica simplificada para o HC-SR04
        // metrics.distance = lerUltrassonico(); 
        
        metrics.taskExecTime[0] = millis() - start;
        vTaskDelay(pdMS_TO_TICKS(100)); // Frequencia de 10Hz
    }
}

// 2. Task de Logica de Seguranca e Atuadores 
void taskSecurityLogic(void *pvParameters) {
    for (;;) {
        uint32_t start = millis();
        if (metrics.gasLevel > 2000) {
            metrics.alarmActive = true;
            digitalWrite(PIN_BUZZER, HIGH);
            digitalWrite(PIN_RELAY, HIGH); // Liga ventilador/exaustor
            gasValve.write(90);            // Fecha valvula
        } else {
            metrics.alarmActive = false;
            digitalWrite(PIN_BUZZER, LOW);
            digitalWrite(PIN_RELAY, LOW);
            gasValve.write(0);
        }
        metrics.taskExecTime[1] = millis() - start;
        vTaskDelay(pdMS_TO_TICKS(50)); // Resposta rapida (20Hz)
    }
}

// 3. Task do Servidor Web
void taskWebServer(void *pvParameters) {
    server.on("/", []() {
        server.send(200, "text/plain", "Interface Web - Sentinela");
    });
    server.begin();
    for (;;) {
        uint32_t start = millis();
        server.handleClient();
        metrics.taskExecTime[2] = millis() - start;
        vTaskDelay(pdMS_TO_TICKS(5)); // Evita watchdog reset
    }
}

// 4. Task de Monitoramento de Performance
void taskPerformanceMonitor(void *pvParameters) {
    for (;;) {
        uint32_t start = millis();
        metrics.freeHeap = ESP.getFreeHeap();
        // Aqui você calcularia o uso de CPU por task se necessário
        
        // Log de sistema (Serial servindo de debug por enquanto) [cite: 61]
        if(metrics.alarmActive) Serial.println("[ALERTA] Vazamento detectado!");
        
        metrics.taskExecTime[3] = millis() - start;
        vTaskDelay(pdMS_TO_TICKS(1000)); // Atualiza a cada 1s
    }
}

// 5. Task de Conectividade e Energia
void taskConnectivityEnergy(void *pvParameters) {
    for (;;) {
        uint32_t start = millis();
        // Verifica conexao e gerencia modos de economia se bateria estiver baixa
        // vTaskDelay(pdMS_TO_TICKS(2000));
        
        metrics.taskExecTime[4] = millis() - start;
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void loop() {
    // O loop fica vazio no FreeRTOS/ LittleFS ou pode ser usado para um Watchdog de baixa prioridade.
    vTaskDelete(NULL); 
}
