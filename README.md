# 🌱 Sistema IoT para Monitoramento de Umidade do Solo em Pivô Central

Projeto desenvolvido como Trabalho de Conclusão de Curso (TCC) de Engenharia Elétrica por **Daniel Guilherme Montanher**, com o objetivo de monitorar a umidade do solo em áreas irrigadas por pivô central, utilizando sensores capacitivos, comunicação **LoRa®** e integração **MQTT** com supervisório em nuvem.

---

## 📡 Arquitetura do Sistema

O sistema é composto por três partes principais:

1. **Nó Sensor (LoRa Node)**
   - Mede a umidade do solo.
   - Envia os dados via LoRa® para o gateway.
   - Utiliza sensor capacitivo.
   - Microcontrolador: **ESP32**.

2. **Gateway LoRa–MQTT**
   - Realiza leitura de umidade de solo, temperatura e umidade relativa do ar.
   - Recebe dados dos nós via LoRa®.
   - Publica as informações no broker MQTT (**test.mosquitto.org**) no tópico **tcc/sensor/#**.
   - Atua como ponte entre os dispositivos de campo e a nuvem.

4. **Supervisório (Node-RED Dashboard)**
   - Exibe dados em tempo real.
   - Gera logs em CSV.
   - Mostra gráficos de umidade, temperatura e RSSI dos nós.

---

## 🧰 Tecnologias utilizadas

| Componente / Software | Função |
|-----------------------|--------|
| **ESP32** | Microcontrolador principal (nós e gateway) |
| **LoRa RFM950W** | Comunicação sem fio de longo alcance |
| **MQTT (Mosquitto)** | Protocolo de transporte |
| **Node-RED** | Supervisório e dashboard em tempo real |
| **Excel** | Armazenamento e análise de dados CSV |

---

## 🗂 Estrutura do Repositório

| Pasta | Conteúdo |
|-------|-----------|
| `/node` | Códigos dos nós sensores LoRa |
| `/gateway` | Código do gateway LoRa–MQTT |
| `/node_red` | Fluxo do Node-RED e funções JavaScript |
| `/docs` | Diagramas e imagens explicativas |
| `README.md` | Este arquivo |

---

## 👨‍💻 Autor

**Daniel Montanher**  
Engenharia Elétrica – FAG 2025  
📍 Cascavel – PR  

---

## 📝 Licença

Este projeto é de código aberto sob a licença MIT.  
Sinta-se livre para estudar, modificar e utilizar o código, citando a fonte.

