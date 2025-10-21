#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

#define PINO_LORA_SS   5
#define PINO_LORA_RST  14
#define PINO_LORA_DIO0 2
#define SENSOR_SOLO    34   // pino analógico do sensor de umidade

#define TEMPO_ENVIO_US (5ULL * 60ULL * 1000000ULL) // 30 min em microssegundos 

int id_no = 1;  // <<< ALTERE para 1, 2 ou outro ID no início do código

// --- Calibração do sensor ---
// Valor lido quando o sensor está 100% seco
int soloSeco = 2850;  
// Valor lido quando o sensor está 100% úmido
int soloUmido = 990;  

// variável que "sobrevive" ao deep sleep
RTC_DATA_ATTR float ultimaLeituraTratada = -1;  

// Função para ler média de N amostras do ADC
int lerMediaADC(int pino, int amostras) {
  long soma = 0;
  for (int i = 0; i < amostras; i++) {
    soma += analogRead(pino);
    delay(10); // pequeno atraso entre leituras
  }
  return soma / amostras;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== INICIANDO NO LoRa ===");

  // --- LoRa ---
  LoRa.setPins(PINO_LORA_SS, PINO_LORA_RST, PINO_LORA_DIO0);
  if (!LoRa.begin(915E6)) {
    Serial.println("❌ Falha ao iniciar LoRa!");
    while (1);
  }
  Serial.println("✅ LoRa iniciado em 915 MHz");

  // mede umidade (ADC bruto com média de 10 amostras)
  int valorADC = lerMediaADC(SENSOR_SOLO, 10);
  Serial.print("🌱 Umidade solo (ADC médio): ");
  Serial.println(valorADC);

  // converte para porcentagem (0 a 100)
  int valorPercent = map(valorADC, soloSeco, soloUmido, 0, 100);
  if (valorPercent < 0) valorPercent = 0;
  if (valorPercent > 100) valorPercent = 100;

  // aplica média móvel com última amostra tratada
  float valorFinal;
  if (ultimaLeituraTratada < 0) {
    // primeira vez: não tem histórico
    valorFinal = valorPercent;
  } else {
    valorFinal = (ultimaLeituraTratada + valorPercent) / 2.0;
  }
  ultimaLeituraTratada = valorFinal; // persiste mesmo após deep sleep

  Serial.print("💧 Umidade solo tratada (%): ");
  Serial.print(valorFinal);
  Serial.println("%");

  // envia pacote: "ID:VALOR"
  LoRa.beginPacket();
  LoRa.print(id_no);
  LoRa.print(":");
  LoRa.print((int)valorFinal);  // envia como inteiro
  LoRa.endPacket();

  Serial.print("📤 Enviado via LoRa -> ");
  Serial.print(id_no);
  Serial.print(":");
  Serial.println((int)valorFinal);

  // aguarda envio e entra em deep sleep
  delay(100);
  Serial.println("💤 Entrando em Deep Sleep por 30 min...");           
  esp_sleep_enable_timer_wakeup(TEMPO_ENVIO_US);     
  esp_deep_sleep_start();
}

void loop() {
  // nunca será executado
}
