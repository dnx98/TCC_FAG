#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_AHT10.h>

#define PINO_LORA_SS   5
#define PINO_LORA_RST  14
#define PINO_LORA_DIO0 2
#define SENSOR_SOLO    34   // pino analógico
#define MQTT_SERVER    "test.mosquitto.org"
#define MQTT_PORT      1883

WiFiClient espClient;
PubSubClient mqtt(espClient);
Adafruit_AHT10 aht;

// ---------- LISTA DE REDES ----------
struct Rede {
  const char* ssid;
  const char* senha;
};

Rede redes[] = {
  {"Montanher", "montanher123"},
  {"iPhone de Daniel", "12345679"},
  {"Dipelnet_Daniel_2.4GHz", "apto0104"}
};
const int nRedes = sizeof(redes) / sizeof(redes[0]);

// --- Calibração do sensor de solo ---
int soloSeco = 3000;  // Valor ADC quando seco
int soloUmido = 1070; // Valor ADC quando úmido

// ---------- Variáveis globais ----------
struct NodeData {
  String valor;
  int rssi;
  float snr;
  bool atualizado;
};

#define MAX_NOS 5
NodeData nos[MAX_NOS];  // suporte até 5 nós

// ---------- CONEXÕES ----------
void conectaWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  for (int i = 0; i < nRedes; i++) {
    Serial.printf("🔌 Tentando conectar em %s ...\n", redes[i].ssid);
    WiFi.begin(redes[i].ssid, redes[i].senha);

    unsigned long inicio = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - inicio < 10000) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("\n✅ Conectado em %s | IP: %s\n",
                    redes[i].ssid,
                    WiFi.localIP().toString().c_str());
      return; // sai da função porque já conectou
    } else {
      Serial.printf("\n❌ Falha em %s\n", redes[i].ssid);
    }
  }

  Serial.println("⚠️ Nenhuma rede conectou!");
}

void conectaMQTT() {
  while (!mqtt.connected() && WiFi.status() == WL_CONNECTED) {
    Serial.print("🔄 Conectando MQTT...");
    if (mqtt.connect("gateway_tcc")) {
      Serial.println("✅ Conectado ao broker!");
    } else {
      Serial.print("❌ Falha, rc=");
      Serial.println(mqtt.state());
      delay(2000);
    }
  }
}

// ---------- Função para média ADC ----------
int lerMediaADC(int pino, int amostras) {
  long soma = 0;
  for (int i = 0; i < amostras; i++) {
    soma += analogRead(pino);
    delay(10);
  }
  return soma / amostras;
}

// ---------- TASK: Recepção LoRa ----------
void taskLoRa(void *pvParameters) {
  for (;;) {
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      String recebido = "";
      while (LoRa.available()) {
        recebido += (char)LoRa.read();
      }
      int rssi = LoRa.packetRssi();
      float snr = LoRa.packetSnr();

      Serial.print("📥 Pacote recebido: ");
      Serial.print(recebido);
      Serial.print(" | RSSI=");
      Serial.print(rssi);
      Serial.print(" dBm | SNR=");
      Serial.println(snr);

      // esperado: "ID:VALOR"
      int sep = recebido.indexOf(":");
      if (sep > 0) {
        int id = recebido.substring(0, sep).toInt();
        String valorStr = recebido.substring(sep + 1);

        if (id > 0 && id <= MAX_NOS) {
          nos[id - 1].valor = valorStr;
          nos[id - 1].rssi = rssi;
          nos[id - 1].snr = snr;
          nos[id - 1].atualizado = true;
          Serial.printf("✅ Nó %d atualizado -> %s\n", id, valorStr.c_str());
        }
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// ---------- TASK: Sensores + Publicação MQTT ----------
void taskGatewaySensores(void *pvParameters) {
  static float ultimaLeituraTratada = -1;

  for (;;) {
    // Lê média de 10 amostras
    int valorADC = lerMediaADC(SENSOR_SOLO, 10);

    int soloPercent = map(valorADC, soloSeco, soloUmido, 0, 100);
    if (soloPercent < 0) soloPercent = 0;
    if (soloPercent > 100) soloPercent = 100;

    float valorFinal;
    if (ultimaLeituraTratada < 0) {
      valorFinal = soloPercent;
    } else {
      valorFinal = (ultimaLeituraTratada + soloPercent) / 2.0;
    }
    ultimaLeituraTratada = valorFinal;

    // Leitura AHT10
    sensors_event_t hum, temp;
    aht.getEvent(&hum, &temp);

    // Monta payload JSON
    String payloadGateway = String("{\"solo\":") + valorFinal +
                            ",\"temp\":" + temp.temperature +
                            ",\"ur\":" + hum.relative_humidity + "}";
    mqtt.publish("tcc/sensor/gateway", payloadGateway.c_str());

    // --- Prints no Serial ---
    Serial.print("🌱 Gateway ADC bruto: ");
    Serial.print(valorADC);
    Serial.print(" | 💧 Umidade tratada (%): ");
    Serial.println(valorFinal);

    Serial.print("📤 Publicado [gateway] -> ");
    Serial.println(payloadGateway);

    // Publica dados dos nós
    for (int i = 0; i < MAX_NOS; i++) {
      if (nos[i].atualizado) {
        String topico = "tcc/sensor/no" + String(i + 1);
        String payload = String("{\"valor\":") + nos[i].valor +
                         ",\"rssi\":" + nos[i].rssi +
                         ",\"snr\":" + nos[i].snr + "}";
        mqtt.publish(topico.c_str(), payload.c_str());
        Serial.print("📤 Publicado [");
        Serial.print(topico);
        Serial.print("] -> ");
        Serial.println(payload);

        nos[i].atualizado = false;
      }
    }

    vTaskDelay(25000 / portTICK_PERIOD_MS);
  }
}


// ---------- TASK: MQTT loop ----------
void taskMQTT(void *pvParameters) {
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!mqtt.connected()) conectaMQTT();
      mqtt.loop();
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// ---------- TASK: Monitor WiFi ----------
void taskWiFiMonitor(void *pvParameters) {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("⚠️ WiFi caiu! Tentando reconectar...");
      conectaWiFi();
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== INICIANDO GATEWAY ===");

  conectaWiFi();
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  conectaMQTT();

  LoRa.setPins(PINO_LORA_SS, PINO_LORA_RST, PINO_LORA_DIO0);
  if (!LoRa.begin(915E6)) {
    Serial.println("❌ Falha ao iniciar LoRa!");
    while (1);
  }
  Serial.println("✅ LoRa iniciado em 915 MHz");

  if (!aht.begin()) {
    Serial.println("❌ Erro: AHT10 não encontrado!");
    while (1);
  }
  Serial.println("✅ AHT10 inicializado");

  // Inicializa array de nós
  for (int i = 0; i < MAX_NOS; i++) {
    nos[i].atualizado = false;
  }

  // Cria tasks
  xTaskCreatePinnedToCore(taskLoRa, "LoRa", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(taskGatewaySensores, "Sensores", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(taskMQTT, "MQTT", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(taskWiFiMonitor, "WiFiMonitor", 4096, NULL, 1, NULL, 1);
}

void loop() {
  // não faz nada
}
