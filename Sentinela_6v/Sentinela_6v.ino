#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <LittleFS.h>
#include <sqlite3.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// pinos
#define PIN_MQ2 34
#define PIN_FLAME 35
#define PIN_SERVO 13
#define PIN_RELAY 14
#define PIN_TRIG 12
#define PIN_ECHO 27
#define PIN_BUZZER 5
#define WDT_TIMEOUT_MS 15000

//globais
int GAS_THRESHOLD;
float DIST_THRESHOLD;
bool rfidLearnMode = false;

struct SystemMetrics {
    uint32_t taskExecTime[5]; 
    uint32_t freeHeap;
    int gasLevel;
    float distance;
    int flameIntensity;
    bool alarmActive;
    struct {
        bool authorized;
        String name;
        String uid;
    } rfid;
} metrics;

WebServer server(80);
Servo gasValve;
sqlite3 *db_sentinela;
Preferences prefs;
SemaphoreHandle_t xMutexMetrics;

// banco & helpers
void executeSQL(const char* sql) {
    char *zErrMsg = 0;
    sqlite3_exec(db_sentinela, sql, NULL, NULL, &zErrMsg);
    if (zErrMsg) sqlite3_free(zErrMsg);
}

void insertLog(const char* type, const char* msg) {
    char query[256];
    snprintf(query, sizeof(query), "INSERT INTO logs (tipo, mensagem) VALUES ('%s', '%s');", type, msg);
    executeSQL(query);
}

// task 0 Core 1
void taskSensors(void *p) {
    esp_task_wdt_add(NULL);
    for (;;) {
        uint32_t start = millis();
        esp_task_wdt_reset();

        // media gas
        long sum = 0;
        for(int i=0; i<10; i++) sum += analogRead(PIN_MQ2);
        int gVal = sum / 10;

       
        int fVal = map(analogRead(PIN_FLAME), 4095, 0, 0, 100);
        if(fVal < 0) fVal = 0;

        // timeout ultra
        digitalWrite(PIN_TRIG, LOW); delayMicroseconds(2);
        digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
        digitalWrite(PIN_TRIG, LOW);
        long duration = pulseIn(PIN_ECHO, HIGH, 30000);
        float dVal = (duration == 0) ? 400.0 : (duration * 0.034 / 2);

        if (xSemaphoreTake(xMutexMetrics, pdMS_TO_TICKS(10))) {
            metrics.gasLevel = gVal;
            metrics.distance = dVal;
            metrics.flameIntensity = fVal;
            metrics.taskExecTime[0] = millis() - start;
            xSemaphoreGive(xMutexMetrics);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// task 1 Core 1
void taskSecurity(void *p) {
    esp_task_wdt_add(NULL);
    int confirmation = 0;
    unsigned long lastAction = 0;

    for (;;) {
        esp_task_wdt_reset();
        uint32_t start = millis();

        int gas;
        bool active;
        if (xSemaphoreTake(xMutexMetrics, pdMS_TO_TICKS(10))) {
            gas = metrics.gasLevel;
            active = metrics.alarmActive;
            xSemaphoreGive(xMutexMetrics);
        }

        // protecao eletrica
        if (millis() - lastAction > 2000) {
            if (active && gas < 20) {  }
            else if (gas > GAS_THRESHOLD) {
                confirmation++;
                if (confirmation >= 3 && !active) {
                    digitalWrite(PIN_BUZZER, HIGH);
                    digitalWrite(PIN_RELAY, HIGH);
                    gasValve.write(90);
                    metrics.alarmActive = true;
                    insertLog("warn", "ALERTA: Vazamento de gás detectado!");
                }
            } else {
                confirmation = 0;
                if (active && (millis() - lastAction > 15000)) {
                    digitalWrite(PIN_BUZZER, LOW);
                    digitalWrite(PIN_RELAY, LOW);
                    gasValve.write(0);
                    metrics.alarmActive = false;
                    lastAction = millis();
                    insertLog("info", "Segurança: Ambiente normalizado.");
                }
            }
        }
        metrics.taskExecTime[1] = millis() - start;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// task 2 Core 0
void taskWeb(void *p) {
    esp_task_wdt_add(NULL);

    server.on("/api/data", HTTP_GET, []() {
        StaticJsonDocument<512> doc;
        xSemaphoreTake(xMutexMetrics, portMAX_DELAY);
        doc["gas"] = metrics.gasLevel;
        doc["dist"] = metrics.distance;
        doc["alarm"] = metrics.alarmActive;
        doc["flame"] = metrics.flameIntensity;
        JsonObject rfid = doc.createNestedObject("rfid");
        rfid["authorized"] = metrics.rfid.authorized;
        rfid["name"] = metrics.rfid.name;
        rfid["uid"] = metrics.rfid.uid;
        xSemaphoreGive(xMutexMetrics);
        String out; serializeJson(doc, out);
        server.send(200, "application/json", out);
    });

    server.on("/api/shutdown", HTTP_POST, []() {
        if(metrics.rfid.authorized) {
            metrics.alarmActive = false;
            digitalWrite(PIN_BUZZER, LOW);
            digitalWrite(PIN_RELAY, LOW);
            gasValve.write(0);
            insertLog("info", "Sistema desligado via Web por " + metrics.rfid.name);
            server.send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            server.send(403, "application/json", "{\"status\":\"unauthorized\"}");
        }
    });
    
    server.on("/api/logs", HTTP_GET, []() {
        sqlite3_stmt *res;
        const char *sql = "SELECT data_hora, tipo, mensagem FROM logs ORDER BY id DESC LIMIT 20;";
        StaticJsonDocument<2048> doc;
        JsonArray arr = doc.createNestedArray("logs");
        if (sqlite3_prepare_v2(db_sentinela, sql, -1, &res, NULL) == SQLITE_OK) {
            while (sqlite3_step(res) == SQLITE_ROW) {
                JsonObject obj = arr.createNestedObject();
                obj["time"] = (const char*)sqlite3_column_text(res, 0);
                obj["type"] = (const char*)sqlite3_column_text(res, 1);
                obj["msg"] = (const char*)sqlite3_column_text(res, 2);
            }
        }
        sqlite3_finalize(res);
        String out; serializeJson(doc, out);
        server.send(200, "application/json", out);
    });

    server.on("/api/config", HTTP_POST, []() {
        if (server.hasArg("plain")) {
            StaticJsonDocument<200> doc;
            deserializeJson(doc, server.arg("plain"));
            GAS_THRESHOLD = doc["gasThreshold"];
            prefs.putInt("gasThres", GAS_THRESHOLD);
            server.send(200, "application/json", "{\"status\":\"saved\"}");
        }
    });

    server.on("/api/performance", HTTP_GET, []() {
        StaticJsonDocument<512> doc;
        doc["heap"] = ESP.getFreeHeap();
        JsonArray t = doc.createNestedArray("tasks_ms");
        for(int i=0; i<5; i++) t.add(metrics.taskExecTime[i]);
        String out; serializeJson(doc, out);
        server.send(200, "application/json", out);
    });

    server.begin();
    for (;;) {
        uint32_t start = millis();
        server.handleClient();
        esp_task_wdt_reset();
        metrics.taskExecTime[3] = millis() - start;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// task 3 Core 0
void taskSQLite(void *p) {
    esp_task_wdt_add(NULL);
    for (;;) {
        uint32_t start = millis();
        esp_task_wdt_reset();
        
        // Log preventivo a cada 1 minuto
        char query[128];
        snprintf(query, sizeof(query), "INSERT INTO logs (tipo, mensagem) VALUES ('info', 'Status: Gás em %d');", metrics.gasLevel);
        executeSQL(query);

        metrics.taskExecTime[2] = millis() - start;
        vTaskDelay(pdMS_TO_TICKS(60000)); 
    }
}

void setup() {
    Serial.begin(115200);
    
    prefs.begin("sentinela", false);
    GAS_THRESHOLD = prefs.getInt("gasThres", 250);
    DIST_THRESHOLD = prefs.getFloat("distThres", 100.0);

    if (LittleFS.begin(true)) {
        sqlite3_open("/littlefs/sentinela.db", &db_sentinela);
        executeSQL("CREATE TABLE IF NOT EXISTS logs (id INTEGER PRIMARY KEY, tipo TEXT, mensagem TEXT, data_hora DATETIME DEFAULT (datetime('now','localtime')));");
    }

    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_RELAY, OUTPUT);
    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);
    gasValve.attach(PIN_SERVO, 500, 2400);
    gasValve.write(0);

    xMutexMetrics = xSemaphoreCreateMutex();

    esp_task_wdt_config_t twdt = { .timeout_ms = WDT_TIMEOUT_MS, .idle_core_mask = 3, .trigger_panic = true };
    esp_task_wdt_init(&twdt);
    esp_task_wdt_add(NULL);

    WiFi.begin("SSID_REDE", "SENHA_REDE");
    while (WiFi.status() != WL_CONNECTED) { esp_task_wdt_reset(); delay(500); }

    xTaskCreatePinnedToCore(taskSensors,  "Sensors",  4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(taskSecurity, "Security", 4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(taskWeb,      "Web",      8192, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(taskSQLite,   "SQLite",   8192, NULL, 1, NULL, 0);

    insertLog("info", "Sistema Online e Monitorando.");
}

void loop() {
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(1000));
}
