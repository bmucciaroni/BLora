/*
 * ==============================================================================
 * PROJETO: Chat Bidirecional LoRa com Heltec ESP32 WiFi LoRa 32 (V2)
 * ==============================================================================
 * 
 * Descrição:
 *  - Comunicação bidirecional sem fio ponto-a-ponto entre 2 placas Heltec LoRa V2.
 *  - O mesmo código é gravado em AMBAS as placas.
 *  - O que você digitar no Monitor Serial de uma placa é enviado via LoRa e 
 *    exibido no Monitor Serial e no display OLED da outra placa (e vice-versa).
 *  - Feedback em tempo real no Display OLED 0.96" integrado (SSD1306 I2C).
 *  - Indicador de força de sinal (RSSI) e relação sinal-ruído (SNR).
 *  - Pisca o LED onboard (GPIO 25) durante transmissões e recepções.
 *
 * AVISO IMPORTANTE DE HARDWARE:
 *  NUNCA ligue a placa ou inicie o código sem a antena LoRa conectada!
 *  Transmitir sem antena pode queimar o circuito amplificador de potência (PA)
 *  do rádio SX1276 permanentemente devido à onda estacionária.
 *
 * Configurações da IDE Arduino:
 *  - Placa: "WiFi LoRa 32(V2)" (em Ferramentas -> Placa -> Heltec ESP32 Series)
 *  - Upload Speed: 921600 ou 115200
 *  - Monitor Serial: Baud Rate 115200, "Nova linha" (Newline) ou "Ambos NL e CR"
 * ==============================================================================
 */

#include "Arduino.h"
#include "LoRaWan_APP.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"

// ==============================================================================
// DEFINIÇÃO DOS PINOS (Heltec WiFi LoRa 32 V2)
// ==============================================================================
#ifndef SDA_OLED
#define SDA_OLED 4
#endif

#ifndef SCL_OLED
#define SCL_OLED 15
#endif

#ifndef RST_OLED
#define RST_OLED 16
#endif

#ifndef Vext
#define Vext 21     // GPIO 21 controla a alimentação Vext (OLED e sensores)
#endif

#ifndef LED
#define LED 25      // LED embutido na placa
#endif

#ifndef HELTEC_BOARD
#define HELTEC_BOARD 0
#endif

#ifndef SLOW_CLK_TPYE
#define SLOW_CLK_TPYE 0
#endif

// ==============================================================================
// CONFIGURAÇÕES DO RÁDIO LoRa
// ==============================================================================
// Frequência de operação (deve ser idêntica nas duas placas)
// 915E6 (915 MHz) - Padrão Brasil / Américas
// 868E6 (868 MHz) - Europa
// 433E6 (433 MHz) - Ásia / Faixa 433
#define RF_FREQUENCY                                915000000 // Hz

#define TX_OUTPUT_POWER                             5         // dBm (Use 5 para bancada próxima; 14 a 20 para longo alcance)
#define LORA_BANDWIDTH                              0         // [0: 125 kHz, 1: 250 kHz, 2: 500 kHz]
#define LORA_SPREADING_FACTOR                       7         // [SF7..SF12] (SF7 é rápido e excelente para chat)
#define LORA_CODINGRATE                             1         // [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8         // Tamanho do preâmbulo (igual para TX e RX)
#define LORA_SYMBOL_TIMEOUT                         0         // Timeout de símbolos
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false

#define BUFFER_SIZE                                 256       // Tamanho máximo do pacote

// ==============================================================================
// OBJETOS E VARIÁVEIS GLOBAIS
// ==============================================================================
// O objeto 'display' já é instanciado globalmente pela biblioteca Heltec (LoRaWan_APP.cpp)
extern SSD1306Wire display;

static RadioEvents_t RadioEvents;
char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];

bool lora_idle = true;             // Indica se o rádio está livre para enviar
String lastTxMsg = "Nenhuma";      // Última mensagem enviada
String lastRxMsg = "Aguardando..."; // Última mensagem recebida
int16_t lastRssi = 0;              // RSSI do último pacote recebido
int8_t lastSnr = 0;                // SNR do último pacote recebido

// Protótipos de funções
void atualizarDisplay();
void enviarMensagem(String msg);
void OnTxDone(void);
void OnTxTimeout(void);
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
void OnRxTimeout(void);
void OnRxError(void);

// ==============================================================================
// SETUP
// ==============================================================================
void setup() {
  Serial.begin(115200);

  // Ativa a alimentação Vext (essencial para ligar a tela OLED no Heltec V2)
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW); // LOW ativa o circuito de alimentação
  delay(100);

  // Configura LED de status e botão PRG onboard (GPIO 0)
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  pinMode(0, INPUT_PULLUP); // Botão PRG / USER na placa

  // Inicializa o Display OLED
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  
  // Tela de boas-vindas inicial
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 15, "Iniciando LoRa...");
  display.drawString(64, 35, "Heltec ESP32 V2");
  display.display();

  // Inicializa o MCU Heltec
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  // Configura os eventos de interrupção do rádio LoRa
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxDone = OnRxDone;
  RadioEvents.RxTimeout = OnRxTimeout;
  RadioEvents.RxError = OnRxError;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);

  // Configuração de transmissão (TX)
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

  // Configuração de recepção contínua (RX)
  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                    0, true, 0, 0, LORA_IQ_INVERSION_ON, true);

  // Coloca o rádio em modo de escuta contínua inicial
  lora_idle = true;
  Radio.Rx(0);

  atualizarDisplay();

  Serial.println();
  Serial.println("==================================================================");
  Serial.println("         HELTEC ESP32 LORA V2 - CHAT SERIAL BIDIRECIONAL          ");
  Serial.println("==================================================================");
  Serial.printf(" Frequencia: %.2f MHz | SF: %d | BW: 125 kHz\n", RF_FREQUENCY / 1E6, LORA_SPREADING_FACTOR);
  Serial.println(" Instrucoes:");
  Serial.println("  1. Digite o texto na caixa de entrada acima e pressione Enter.");
  Serial.println("  2. O texto sera transmitido para a outra placa via LoRa.");
  Serial.println("  3. Mensagens recebidas da outra placa serao exibidas aqui e no OLED.");
  Serial.println("==================================================================");
  Serial.println();
}

// ==============================================================================
// LOOP PRINCIPAL
// ==============================================================================
void loop() {
  // Processamento obrigatório das interrupções do chip LoRa (SX1276)
  Radio.IrqProcess();

  // Botão PRG (GPIO 0) integrado para teste rápido sem precisar digitar
  static bool lastBtnState = HIGH;
  bool btnState = digitalRead(0);
  if (lastBtnState == HIGH && btnState == LOW) {
    delay(50); // debounce
    if (digitalRead(0) == LOW) {
      if (lora_idle) {
        static int pingCount = 1;
        enviarMensagem("Ping #" + String(pingCount++));
      }
    }
  }
  lastBtnState = btnState;

  // Verifica se há dados digitados no Monitor Serial
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim(); // Remove '\r', espaços em branco extras

    if (input.length() > 0) {
      if (lora_idle) {
        enviarMensagem(input);
      } else {
        Serial.println("[AVISO] O radio esta ocupado enviando outro pacote. Aguarde...");
      }
    }
  }
}

// ==============================================================================
// ENVIO DE MENSAGEM VIA LORA
// ==============================================================================
void enviarMensagem(String msg) {
  lora_idle = false;
  digitalWrite(LED, HIGH); // Acende o LED durante o envio

  lastTxMsg = msg;

  // Copia mensagem para o buffer de transmissão
  strncpy(txpacket, msg.c_str(), BUFFER_SIZE - 1);
  txpacket[BUFFER_SIZE - 1] = '\0';

  Serial.println();
  Serial.print("[ENVIADO]: ");
  Serial.println(msg);

  atualizarDisplay();

  // Envia os bytes pelo rádio LoRa
  Radio.Send((uint8_t *)txpacket, strlen(txpacket));
}

// ==============================================================================
// CALLBACK: TRANSMISSÃO CONCLUÍDA COM SUCESSO
// ==============================================================================
void OnTxDone(void) {
  digitalWrite(LED, LOW);
  Serial.println("  └─> Pacote transmitido com sucesso via LoRa!");
  
  // Retorna imediatamente à escuta contínua de mensagens
  lora_idle = true;
  Radio.Rx(0);
}

// ==============================================================================
// CALLBACK: TIMEOUT DE TRANSMISSÃO
// ==============================================================================
void OnTxTimeout(void) {
  digitalWrite(LED, LOW);
  Radio.Sleep();
  Serial.println("  └─> [ERRO]: Timeout durante transmissao!");

  // Retorna à escuta
  lora_idle = true;
  Radio.Rx(0);
}

// ==============================================================================
// CALLBACK: TIMEOUT DE RECEPÇÃO
// ==============================================================================
void OnRxTimeout(void) {
  Radio.Rx(0);
}

// ==============================================================================
// CALLBACK: ERRO DE RECEPÇÃO (CRC / PACOTE CORROMPIDO)
// ==============================================================================
void OnRxError(void) {
  Serial.println("  └─> [AVISO]: Pacote recebido com erro de CRC!");
  Serial.println("      (Dica: se as placas estiverem coladas na mesa, afaste-as em 1 a 2 metros)");
  Radio.Rx(0);
}

// ==============================================================================
// CALLBACK: MENSAGEM RECEBIDA VIA LORA
// ==============================================================================
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  digitalWrite(LED, HIGH);
  Radio.Sleep();

  // Converte o payload recebido em texto seguro com terminador nulo
  uint16_t rxLen = (size < BUFFER_SIZE - 1) ? size : BUFFER_SIZE - 1;
  memcpy(rxpacket, payload, rxLen);
  rxpacket[rxLen] = '\0';

  lastRxMsg = String(rxpacket);
  lastRssi = rssi;
  lastSnr = snr;

  // Feedback detalhado no Monitor Serial
  Serial.println();
  Serial.println("---------------- Nova Mensagem Recebida ----------------");
  Serial.print("[RECEBIDO]: ");
  Serial.println(lastRxMsg);
  Serial.printf("Qualidade : RSSI = %d dBm | SNR = %d dB | Bytes = %d\n", rssi, snr, size);
  Serial.println("---------------------------------------------------------");

  // Atualiza tela OLED
  atualizarDisplay();

  digitalWrite(LED, LOW);

  // Retorna imediatamente ao modo de escuta de novos pacotes
  lora_idle = true;
  Radio.Rx(0);
}

// ==============================================================================
// ATUALIZAÇÃO DO DISPLAY OLED 0.96" (128x64)
// ==============================================================================
void atualizarDisplay() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

  // Cabeçalho estilizado com frequência
  display.drawString(0, 0, "Heltec LoRa Chat (915)");
  display.drawHorizontalLine(0, 13, 128);

  // Última mensagem enviada (TX)
  String txText = "TX: " + lastTxMsg;
  if (txText.length() > 20) {
    txText = txText.substring(0, 17) + "...";
  }
  display.drawString(0, 16, txText);

  // Última mensagem recebida (RX)
  String rxText = "RX: " + lastRxMsg;
  if (rxText.length() > 20) {
    rxText = rxText.substring(0, 17) + "...";
  }
  display.drawString(0, 31, rxText);

  // Linha divisória inferior
  display.drawHorizontalLine(0, 46, 128);

  // Indicador de qualidade do sinal (RSSI e SNR)
  if (lastRssi != 0) {
    display.drawString(0, 49, "RSSI:" + String(lastRssi) + "dBm  SNR:" + String(lastSnr) + "dB");
  } else {
    display.drawString(0, 49, "Aguardando sinal...");
  }

  display.display();
}
