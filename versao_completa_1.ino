#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <LittleFS.h>
#include <MySQL_Generic.h>
#include <esp_task_wdt.h>

//Configuracoes de rede e banco de dados 
const char* WIFI_SSID = "nome_rede"; // aquela propria do esp32
const char* WIFI_PASS = "senha_rede";

IPAddress server_addr(192, 168, 1, 100); // IP do servidor MySQL 
uint16_t server_port = 3306;
char db_user[] = "root";
char db_pass[] = "senha123"; // essa senha vc cria quando primeiro configura o MySQLWorkbench anotem pq vc sempre vai usar ela
char db_name[] = "sentinela_db";

//definicao pinos LEMBRANDO QUE OS PINOS PODEM MUDAR!!
#define PIN_MQ2 34
#define PIN_SERVO 13
#define PIN_RELAY 14     // Atuador 2 (Exaustor/Válvula 2)
#define PIN_TRIG 12      // Sensor Ultrassônico
#define PIN_ECHO 27
#define PIN_BUZZER 5
#define WDT_TIMEOUT 5    // Timeout do Watchdog em segundos

// estrutura de dados
struct SensorData {
    int gasLevel;
    float distance;
    bool alarm;
    unsigned long timestamp;
};

struct SystemMetrics {
    uint32_t taskExecTime[5]; 
    uint32_t freeHeap;
    int gasLevel;
    float distance;
    bool alarmActive;
} metrics;

//globais 
WebServer server(80);
Servo gasValve;
QueueHandle_t sensorQueue;
MySQL_Connection sql_conn((Client *)&WiFiClient());
unsigned long lastGasTime = 0;
const uint32_t MIN_TIME_ACTIVE = 10000;

// PROTOTIPO tasks
void taskSensors(void *p);
void taskSecurity(void *p);
void taskMySQL(void *p);
void taskWeb(void *p);
void taskPerf(void *p);

// funcoes suporte littleFS
String getContentType(String filename) {
    if (filename.endsWith(".html")) return "text/html";
    if (filename.endsWith(".css"))  return "text/css";
    if (filename.endsWith(".js"))   return "application/javascript";
    if (filename.endsWith(".png"))  return "image/png";
    return "text/plain";
}

bool handleFileRead(String path) {
    if (path.endsWith("/")) path += "index.html";
    if (LittleFS.exists(path)) {
        File file = LittleFS.open(path, "r");
        server.streamFile(file, getContentType(path));
        file.close();
        return true;
    }
    return false;
}

void setup() {
    Serial.begin(115200);

    // 1. inicializa sistema de arquivos (LittleFS)
    if (!LittleFS.begin(true)) {
        Serial.println("Erro ao montar LittleFS");
    }

    // 2. configura watchdog no Core principal
    esp_task_wdt_init(WDT_TIMEOUT, true);
    esp_task_wdt_add(NULL);

    // 3. Hardware
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_RELAY, OUTPUT);
    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);
    gasValve.setPeriodHertz(50);
    gasValve.attach(PIN_SERVO, 500, 2400);
    gasValve.write(0);

    // 4. comunicacao entre tasks (Queue)
    sensorQueue = xQueueCreate(10, sizeof(SensorData));

    // 5. conexao WiFi (Modo Station)
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Conectando WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConectado! IP: " + WiFi.localIP().toString());

    // 6. Escalonamento de Tasks (FreeRTOS)
    xTaskCreatePinnedToCore(taskSecurity, "Security", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(taskSensors,  "Sensors",  4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(taskWeb,      "Web",      8192, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(taskPerf,     "Perf",     4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(taskMySQL,    "MySQL",    8192, NULL, 1, NULL, 0);
}

// 1. task leitura de sensores (Core 1)
void taskSensors(void *p) {
    for (;;) {
        uint32_t start = millis();
        SensorData currentRead;

        currentRead.gasLevel = analogRead(PIN_MQ2);
        
        digitalWrite(PIN_TRIG, LOW); delayMicroseconds(2);
        digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
        digitalWrite(PIN_TRIG, LOW);
        currentRead.distance = pulseIn(PIN_ECHO, HIGH) * 0.034 / 2;
        currentRead.timestamp = millis();

        xQueueOverwrite(sensorQueue, &currentRead); 

        metrics.taskExecTime[0] = millis() - start;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// 2. Task logica de segurança e watchdog (Core 1)
void taskSecurity(void *p) {
    esp_task_wdt_add(NULL); // add task ao monitoramento do WDT
    SensorData data;

    for (;;) {
        uint32_t start = millis();
        esp_task_wdt_reset(); // alimenta o Watchdog

        if (xQueuePeek(sensorQueue, &data, portMAX_DELAY)) {
            metrics.gasLevel = data.gasLevel;
            metrics.distance = data.distance;

            if (data.gasLevel > 2000) {
                metrics.alarmActive = true;
                lastGasTime = millis();
                digitalWrite(PIN_BUZZER, HIGH);
                digitalWrite(PIN_RELAY, HIGH);
                gasValve.write(90);
            } else if (millis() - lastGasTime > MIN_TIME_ACTIVE) {
                metrics.alarmActive = false;
                digitalWrite(PIN_BUZZER, LOW);
                digitalWrite(PIN_RELAY, LOW);
                gasValve.write(0);
            }
        }
        metrics.taskExecTime[1] = millis() - start;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 3. task persistência MySQL (Core 0)
void taskMySQL(void *p) {
    SensorData toSave;
    for (;;) {
        if (xQueueReceive(sensorQueue, &toSave, portMAX_DELAY)) {
            uint32_t start = millis();
            if (WiFi.status() == WL_CONNECTED) {
                if (sql_conn.connect(server_addr, server_port, db_user, db_pass)) {
                    MySQL_Cursor *cur = new MySQL_Cursor(&sql_conn);
                    
                    // Salva na tabela de sensores
                    String query = "INSERT INTO " + String(db_name) + ".logs_sensores (nivel_gas, distancia, status_alarme) VALUES (" 
                                   + String(toSave.gasLevel) + "," + String(toSave.distance) + "," + String(metrics.alarmActive) + ")";
                    cur->execute(query.c_str());

                    // Salva na tabela de performance (opcional a cada ciclo)
                    String perfQuery = "INSERT INTO " + String(db_name) + ".logs_performance (heap_livre, tempo_task_sensores, tempo_task_seguranca) VALUES (" 
                                       + String(metrics.freeHeap) + "," + String(metrics.taskExecTime[0]) + "," + String(metrics.taskExecTime[1]) + ")";
                    cur->execute(perfQuery.c_str());

                    delete cur;
                    sql_conn.close();
                }
            }
            metrics.taskExecTime[2] = millis() - start;
        }
        vTaskDelay(pdMS_TO_TICKS(10000)); // Grava logs a cada 10 segundos
    }
}

// 4. task servidor web (Core 0)
void taskWeb(void *p) {
    server.on("/api/status", []() {
        String json = "{\"gas\":" + String(metrics.gasLevel) + ",\"alarm\":" + String(metrics.alarmActive) + "}";
        server.send(200, "application/json", json);
    });

    server.on("/api/performance", []() {
        String json = "{\"heap\":" + String(metrics.freeHeap) + ",\"tasks\":[";
        for(int i=0; i<5; i++) json += String(metrics.taskExecTime[i]) + (i < 4 ? "," : "");
        json += "]}";
        server.send(200, "application/json", json);
    });

    server.onNotFound([]() {
        if (!handleFileRead(server.uri())) server.send(404, "text/plain", "Não encontrado");
    });

    server.begin();
    for (;;) {
        uint32_t start = millis();
        server.handleClient();
        metrics.taskExecTime[3] = millis() - start;
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// 5. task monitor de performance (Core 0)
void taskPerf(void *p) {
    for (;;) {
        uint32_t start = millis();
        metrics.freeHeap = ESP.getFreeHeap();
        
        // Log Serial para Debug
        Serial.printf("Heap: %u | Gas: %d | Alarme: %s\n", metrics.freeHeap, metrics.gasLevel, metrics.alarmActive ? "ON" : "OFF");

        metrics.taskExecTime[4] = millis() - start;
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void loop() {
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(1000));
}
