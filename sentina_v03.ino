#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// Definições de Pinos
const int pinoMQ2 = 34;
const int pinoBuzzer = 5;
const int pinoServo = 13;

Servo meuServo;
WebServer server(80);

// Variáveis para controle de tempo (substituindo o delay)
unsigned long tempoAnterior = 0;
const long intervalo = 500;

void handleRoot() {
  int gas = analogRead(pinoMQ2);
  String html = "<html><body><h1>Status do Sentinela</h1>";
  html += "<p>Nivel de gas atual: " + String(gas) + "</p>";
  html += gas > 2000 ? "<h2 style='color:red;'>ALERTA: RISCO DETECTADO!</h2>" : "<h2 style='color:green;'>Sistema Seguro</h2>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  pinMode(pinoBuzzer, OUTPUT);
  
  meuServo.attach(pinoServo);
  meuServo.write(0);

  // Configura como Access Point
  WiFi.softAP("Sentinela_ESP32", "12345678");
  
  server.on("/", handleRoot);
  server.begin();
  
  Serial.println("Rede Wi-Fi 'Sentinela_ESP32' criada!");
  Serial.println("Acesse: 192.168.4.1 no seu navegador");
}

void loop() {
  server.handleClient(); // Gerencia as conexões web

  unsigned long tempoAtual = millis();
  
  
  if (tempoAtual - tempoAnterior >= intervalo) {
    tempoAnterior = tempoAtual;
    
    int valorGas = analogRead(pinoMQ2);
    
    if (valorGas > 2000) {
      digitalWrite(pinoBuzzer, HIGH);
      meuServo.write(90);
    } else {
      digitalWrite(pinoBuzzer, LOW);
      meuServo.write(0);
    }
  }
}