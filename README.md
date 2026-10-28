# Heltec ESP32 LoRa V2 - Chat Serial Bidirecional & Transceiver

Sistema de comunicação bidirecional via rádio LoRa ponto a ponto (P2P) entre duas placas **Heltec WiFi LoRa 32 (V2)**, com feedback em tempo real no monitor serial e no display OLED integrado de 0.96".

---

## 🚀 Funcionalidades

- **Comunicação Bidirecional:** O mesmo firmware roda em ambas as placas. O que for digitado no Monitor Serial de uma placa é transmitido via LoRa e exibido no Serial e no display da outra (e vice-versa).
- **Feedback no Display OLED 0.96" (SSD1306):**
  - Cabeçalho com identificação e frequência (915 MHz).
  - Última mensagem transmitida (`TX: ...`).
  - Última mensagem recebida (`RX: ...`).
  - Nível de sinal do último pacote (`RSSI` em dBm e `SNR` em dB).
- **Botão Físico de Teste (PRG):** Pressione o botão `PRG` (GPIO 0) na placa para disparar um ping de teste automático sem precisar digitar no Serial.
- **LED Indicador de Status:** O LED onboard (GPIO 25) pisca durante envios e recepções de pacotes.

---

## ⚠️ Aviso Crítico de Hardware

> **NUNCA ligue a placa ou inicie o código sem a antena LoRa conectada.**
> Transmitir sinal de rádio sem antena pode queimar o circuito amplificador de potência (PA) do chip SX1276 permanentemente devido à alta reflexão de onda estacionária (VSWR).

---

## 🛠️ Hardware & Pinout (Heltec WiFi LoRa 32 V2)

| Recurso | Pino GPIO | Observação |
|---|---|---|
| **OLED SDA** | GPIO 4 | Linha de dados I2C do display |
| **OLED SCL** | GPIO 15 | Linha de clock I2C do display |
| **OLED RST** | GPIO 16 | Reset do display |
| **Vext Control** | GPIO 21 | Liga a alimentação do OLED (`LOW` = ligado) |
| **LED Onboard** | GPIO 25 | LED de status |
| **Botão PRG** | GPIO 0 | Botão de usuário (pull-up interno) |
| **LoRa CS / SS** | GPIO 18 | SPI Chip Select |
| **LoRa RST** | GPIO 14 | Reset do rádio SX1276 |
| **LoRa DIO0** | GPIO 26 | Interrupção de RX/TX Done |

---

## ⚙️ Configuração na Arduino IDE

1. **Placa Selecionada:**
   - Menu: `Ferramentas` > `Placa` > `Heltec ESP32 Series` > **WiFi LoRa 32(V2)**
2. **Frequência:**
   - 915 MHz (`915000000 Hz`) por padrão (faixa ISM Brasil / Américas).
3. **Monitor Serial:**
   - Velocidade: **115200 baud**
   - Final de linha: **Nova linha (Newline / `\n`)** ou **Ambos (NL & CR)**

---

## 📝 Dicas de Teste em Bancada

- **Saturação de Sinal por Proximidade:** Se as duas placas estiverem coladas na mesa (menos de 50 cm), a intensidade do sinal pode saturar o receptor. O código está configurado com potência reduzida (`TX_OUTPUT_POWER = 5 dBm`) para bancada. Afaste as placas em 1 a 2 metros para melhores resultados. Para alcance longo na rua, aumente para `14` a `20` dBm.
- **Disparo Rápido:** Basta pressionar o botão `PRG` na placa transmissora para validar a recepção na outra placa imediatamente.
