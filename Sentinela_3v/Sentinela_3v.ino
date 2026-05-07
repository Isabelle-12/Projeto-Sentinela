#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include "SPIFFS.h"

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

uint32_t lastGasTime = 0; 
const uint32_t MIN_TIME_ACTIVE = 10000; // 10 segundos em milissegundos

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
void taskPerformanceMonitor(void *pvParameters);
void taskConnectivityEnergy(void *pvParameters);
bool handleFileRead(String path); 
String getContentType(String filename);
void taskWebServer(void *pvParameters);

void setup() {
    Serial.begin(115200);
    
   
    // Inicialização do SPIFFS
    if(!SPIFFS.begin(true)){ // Alterado para SPIFFS
        Serial.println("Erro ao montar o SPIFFS");
    } else {
        Serial.println("SPIFFS montado com sucesso!");
    }

    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_RELAY, OUTPUT);
    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);

    gasValve.setPeriodHertz(50);
    gasValve.attach(PIN_SERVO, 500, 2400);
    gasValve.write(0);
    
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP("Sentinela_ESP32", "12345678");

    // Criação das Tasks permanece igual
    xTaskCreatePinnedToCore(taskSensorReading, "Sensors", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(taskSecurityLogic, "Security", 4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(taskWebServer, "Web", 8192, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(taskPerformanceMonitor, "Perf", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(taskConnectivityEnergy, "Energy", 4096, NULL, 1, NULL, 0);
}

// Implementacao das Task


// --- TASK DO SERVIDOR WEB ---
void taskWebServer(void *pvParameters) {
    server.on("/api/data", []() {
        String json = "{";
        json += "\"gasPPM\":" + String(metrics.gasLevel) + ",";
        json += "\"distancia\":" + String(metrics.distance) + ",";
        json += "\"alarme\":" + String(metrics.alarmActive ? "true" : "false");
        json += "}";
        server.send(200, "application/json", json);
    });

    server.onNotFound([]() {
        if (!handleFileRead(server.uri())) {
            server.send(404, "text/plain", "Arquivo não encontrado no SPIFFS");
        }
    });

    server.begin();
    for (;;) {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// --- FUNÇÕES DE SUPORTE AO LITTLEFS ---

String getContentType(String filename) {
    if (filename.endsWith(".html")) return "text/html";
    if (filename.endsWith(".css"))  return "text/css";
    if (filename.endsWith(".js"))   return "application/javascript";
    if (filename.endsWith(".png"))  return "image/png";
    if (filename.endsWith(".svg"))  return "image/svg+xml";
    if (filename.endsWith(".ico"))  return "image/x-icon";
    return "text/plain";
}

bool handleFileRead(String path) {
    if (path.endsWith("/")) path += "index.html";
    if (!path.startsWith("/")) path = "/" + path;

    // Alterado de LittleFS para SPIFFS
    if (SPIFFS.exists(path)) {
        File file = SPIFFS.open(path, "r");
        server.streamFile(file, getContentType(path));
        file.close();
        return true;
    }
    
    Serial.print("Arquivo nao encontrado no SPIFFS: ");
    Serial.println(path);
    return false;
}



// 1. Task de Leitura de Sensores 
void taskSensorReading(void *pvParameters) {
    for (;;) {
        uint32_t start = millis();
        // Leitura Gás (MQ2)
        metrics.gasLevel = analogRead(PIN_MQ2);
    

        // Leitura Ultrassônico (HC-SR04)
        digitalWrite(PIN_TRIG, LOW);
        delayMicroseconds(2);
        digitalWrite(PIN_TRIG, HIGH);
        delayMicroseconds(10);
        digitalWrite(PIN_TRIG, LOW);
        long duration = pulseIn(PIN_ECHO, HIGH);
        metrics.distance = duration * 0.034 / 2;
        
        metrics.taskExecTime[0] = millis() - start;
        vTaskDelay(pdMS_TO_TICKS(100)); // Frequencia de 10Hz
    }
}

// 2. Task de Logica de Seguranca e Atuadores 
void taskSecurityLogic(void *pvParameters) {
    bool lastState = false; // Para evitar spam de logs repetidos

    for (;;) {
        uint32_t start = millis();
        uint32_t currentTime = millis();

        if (metrics.gasLevel > 200) {
            metrics.alarmActive = true;
            lastGasTime = currentTime; 
        } 
        
        if (metrics.alarmActive) {
            if ((metrics.gasLevel <= 200 || metrics.gasLevel >= 4000) && (currentTime - lastGasTime > MIN_TIME_ACTIVE)) {
                metrics.alarmActive = false;
            }
        }//remover isso

        if (metrics.alarmActive) {
            digitalWrite(PIN_BUZZER, HIGH);
            //digitalWrite(PIN_RELAY, HIGH); 
            gasValve.write(90); 
            
            if (lastState == false) {
                Serial.println(">>> ATUADOR: Servo Motor FECHANDO Válvula (90°)");
                lastState = true;
            }
        } 
        else {
            digitalWrite(PIN_BUZZER, LOW);
            //digitalWrite(PIN_RELAY, LOW);
            gasValve.write(0); 

            if (lastState == true) {
                Serial.println(">>> ATUADOR: Servo Motor ABRINDO Válvula (0°)");
                lastState = false;
            }
        }

        metrics.taskExecTime[1] = millis() - start;
        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
}


// 4. Task de Monitoramento de Performance
void taskPerformanceMonitor(void *pvParameters) {
    for (;;) {
        uint32_t start = millis();
        metrics.freeHeap = ESP.getFreeHeap();

        Serial.println("\n--- STATUS DO SISTEMA ---");
        Serial.print("GAS: "); Serial.print(metrics.gasLevel);
        Serial.print(" | DISTANCIA: "); Serial.print(metrics.distance); Serial.println(" cm");
        
        // Log específico do Servo
        Serial.print("SERVO: "); 
        Serial.println(metrics.alarmActive ? "[90° - BLOQUEADO]" : "[0° - FLUXO LIVRE]");
        
        Serial.print("ALERTA: "); Serial.println(metrics.alarmActive ? "!!! ATIVO !!!" : "DESATIVADO");
        
        if (metrics.alarmActive && metrics.gasLevel <= 200) {
            long tempoRestante = (MIN_TIME_ACTIVE - (millis() - lastGasTime)) / 1000;
            Serial.print("AGUARDANDO ESTABILIZAÇÃO: "); Serial.print(tempoRestante); Serial.println("s");
        }
        Serial.println("-------------------------");

        metrics.taskExecTime[3] = millis() - start;
        vTaskDelay(pdMS_TO_TICKS(1000)); 
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
