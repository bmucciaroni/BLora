/*
 * ==============================================================================
 * PROJETO: Chat Ponto a Ponto (P2P) Endereçado com Heltec ESP32 WiFi LoRa 32 (V2)
 * ==============================================================================
 * 
 * Descrição:
 *  - Comunicação bidirecional direta e privada entre 2 placas (Placa A e Placa B).
 *  - Camada de Endereçamento (Node Addressing):
 *      -> Cada pacote contém [Network ID (0xB1A2)] + [Destinatário] + [Remetente].
 *      -> Placas ignoram qualquer mensagem de redes/sensores vizinhos ("Caixa cheia", etc).
 *      -> Placas ignoram mensagens não destinadas explicitamente ao seu ID.
 *  - Canal Exclusivo: Frequência sintonizada em 916.5 MHz e SyncWord em 0xB1 (hardware filter).
 *  - Feedback em tempo real no Display OLED 0.96" e no Monitor Serial (115200 baud).
 *  - Botão PRG (GPIO 0): envia um ping direcionado para a outra placa.
 *
 * COMO CARREGAR NAS PLACAS:
 *  1. Para a primeira placa:
 *     Deixe ativado: #define PLACA_A
 *     Carregue o código na Placa A.
 *
 *  2. Para a segunda placa:
 *     Comente a Placa A e descomente a Placa B:
 *     //#define PLACA_A
 *     #define PLACA_B
 *     Carregue o código na Placa B.
 * ==============================================================================
 */

#include "Arduino.h"
#include "LoRaWan_APP.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"

// ==============================================================================
// 1. SELEÇÃO DA IDENTIDADE DA PLACA
// ==============================================================================
// Descomente apenas UMA das duas linhas abaixo antes de carregar o código:
#define PLACA_A     // Placa 1: ID = 1 (Envia para Placa B)
//#define PLACA_B   // Placa 2: ID = 2 (Envia para Placa A)

#if defined(PLACA_A)
  const uint8_t MEU_ID   = 1;
  const uint8_t DEST_ID  = 2;
  const char*   NOME_MEU = "Placa A";
  const char*   NOME_DEST= "Placa B";
#elif defined(PLACA_B)
  const uint8_t MEU_ID   = 2;
  const uint8_t DEST_ID  = 1;
  const char*   NOME_MEU = "Placa B";
  const char*   NOME_DEST= "Placa A";
#else
  #error "Selecione PLACA_A ou PLACA_B no topo do arquivo!"
#endif

// ==============================================================================
// 2. DEFINIÇÃO DE ENDEREÇAMENTO E ISOLAMENTO DE REDE
// ==============================================================================
#define NETWORK_ID            0xB1A2    // Assinatura exclusiva da sua rede (filtro de firmware)
#define LORA_SYNCWORD         0xB1      // SyncWord exclusivo (filtro a nível de silício SX1276)

// Estrutura do cabeçalho binário (4 bytes)
struct __attribute__((packed)) LoRaHeader {
  uint16_t netId;   // Identificador da rede (0xB1A2)
  uint8_t  dest;    // ID do nó destinatário (quem deve ler)
  uint8_t  src;     // ID do nó remetente (quem enviou)
};

// ==============================================================================
// 3. DEFINIÇÃO DOS PINOS (Heltec WiFi LoRa 32 V2)
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
// 4. CONFIGURAÇÕES DO RÁDIO LoRa
// ==============================================================================
// Frequência de operação em 916.5 MHz (afastada do canal padrão 915.0 MHz usado por sensores vizinhos)
#define RF_FREQUENCY                                916500000 // Hz

#define TX_OUTPUT_POWER                             10        // dBm (10 dBm para bancada; 14 a 20 para campo aberto)
#define LORA_BANDWIDTH                              0         // [0: 125 kHz, 1: 250 kHz, 2: 500 kHz]
#define LORA_SPREADING_FACTOR                       7         // [SF7..SF12] (SF7 é rápido e excelente para chat)
#define LORA_CODINGRATE                             1         // [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8         // Tamanho do preâmbulo
#define LORA_SYMBOL_TIMEOUT                         0         // Timeout de símbolos
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false

#define BUFFER_SIZE                                 256       // Tamanho máximo do pacote

// ==============================================================================
// 5. OBJETOS E VARIÁVEIS GLOBAIS
// ==============================================================================
// O objeto 'display' é instanciado globalmente pela biblioteca Heltec (LoRaWan_APP.cpp)
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
  pinMode(0, INPUT_PULLUP);

  // Inicializa o Display OLED
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  
  // Tela de inicialização
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 10, "Iniciando LoRa P2P");
  display.drawString(64, 25, String(NOME_MEU));
  display.drawString(64, 42, "Destino: " + String(NOME_DEST));
  display.display();
  delay(600);

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

  // Configura o SyncWord personalizado no registrador do chip SX1276 (0x39)
  // Bloqueia pacotes de qualquer dispositivo com SyncWord padrão (0x12 ou 0x34)
  Radio.Write(0x39, LORA_SYNCWORD);

  // Inicia em modo de escuta contínua
  lora_idle = true;
  Radio.Rx(0);

  atualizarDisplay();

  Serial.println();
  Serial.println("==================================================================");
  Serial.printf("    HELTEC ESP32 LORA V2 - CHAT ENDERECADO [%s]\n", NOME_MEU);
  Serial.println("==================================================================");
  Serial.printf(" Meu Endereco    : Nó %d (%s)\n", MEU_ID, NOME_MEU);
  Serial.printf(" Endereco Destino: Nó %d (%s)\n", DEST_ID, NOME_DEST);
  Serial.printf(" Frequencia Privada : %.2f MHz | SyncWord: 0x%02X\n", RF_FREQUENCY / 1E6, LORA_SYNCWORD);
  Serial.printf(" Network ID Privado : 0x%04X (filtra 100%% de sinais vizinhos)\n", NETWORK_ID);
  Serial.println("------------------------------------------------------------------");
  Serial.println(" Instrucoes:");
  Serial.println("  - Digite na caixa de texto do Monitor Serial e aperte Enter.");
  Serial.println("  - Ou aperte o botao PRG na placa para disparar um Ping de teste.");
  Serial.println("==================================================================\n");
}

// ==============================================================================
// LOOP PRINCIPAL
// ==============================================================================
void loop() {
  // Processamento contínuo das interrupções do chip LoRa (SX1276)
  Radio.IrqProcess();

  // Botão PRG (GPIO 0) para teste rápido de hardware
  static bool lastBtnState = HIGH;
  bool btnState = digitalRead(0);
  if (lastBtnState == HIGH && btnState == LOW) {
    delay(50); // debounce
    if (digitalRead(0) == LOW) {
      if (lora_idle) {
        static int pingCount = 1;
        enviarMensagem("Ping de " + String(NOME_MEU) + " #" + String(pingCount++));
      }
    }
  }
  lastBtnState = btnState;

  // Leitura de texto digitado no Monitor Serial
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.length() > 0) {
      if (lora_idle) {
        enviarMensagem(input);
      } else {
        Serial.println("[AVISO] O radio ainda esta transmitindo o pacote anterior. Aguarde...");
      }
    }
  }
}

// ==============================================================================
// ENVIO DE MENSAGEM VIA LORA (COM CABEÇALHO DE ENDEREÇAMENTO)
// ==============================================================================
void enviarMensagem(String msg) {
  lora_idle = false;
  digitalWrite(LED, HIGH);

  lastTxMsg = msg;

  // Monta o cabeçalho de roteamento
  LoRaHeader header;
  header.netId = NETWORK_ID;
  header.dest  = DEST_ID;
  header.src   = MEU_ID;

  uint16_t maxTextLen = BUFFER_SIZE - sizeof(LoRaHeader) - 1;
  uint16_t msgLen = msg.length();
  if (msgLen > maxTextLen) {
    msgLen = maxTextLen;
  }

  // Empacotamento: [Cabeçalho (4 bytes)] + [Texto da mensagem]
  memcpy(txpacket, &header, sizeof(LoRaHeader));
  memcpy(txpacket + sizeof(LoRaHeader), msg.c_str(), msgLen);
  uint16_t totalLen = sizeof(LoRaHeader) + msgLen;
  txpacket[totalLen] = '\0';

  Serial.println();
  Serial.printf("[ENVIADO de %s -> %s]: %s\n", NOME_MEU, NOME_DEST, msg.c_str());

  atualizarDisplay();

  // Envia os bytes pelo rádio LoRa
  Radio.Send((uint8_t *)txpacket, totalLen);
}

// ==============================================================================
// CALLBACK: TRANSMISSÃO CONCLUÍDA
// ==============================================================================
void OnTxDone(void) {
  digitalWrite(LED, LOW);
  Serial.println("  └─> Pacote transmitido com sucesso via LoRa!");
  
  lora_idle = true;
  Radio.Rx(0); // Volta imediatamente à escuta contínua
}

// ==============================================================================
// CALLBACK: TIMEOUT DE TRANSMISSÃO
// ==============================================================================
void OnTxTimeout(void) {
  digitalWrite(LED, LOW);
  Radio.Sleep();
  Serial.println("  └─> [ERRO]: Timeout durante transmissao!");

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
// CALLBACK: ERRO DE CRC
// ==============================================================================
void OnRxError(void) {
  Serial.println("  └─> [AVISO]: Pacote recebido com erro de CRC!");
  Radio.Rx(0);
}

// ==============================================================================
// CALLBACK: MENSAGEM RECEBIDA VIA LORA (COM FILTRAGEM DE DESTINO)
// ==============================================================================
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  digitalWrite(LED, HIGH);
  Radio.Sleep();

  // 1. Validação de tamanho mínimo do cabeçalho
  if (size < sizeof(LoRaHeader)) {
    digitalWrite(LED, LOW);
    lora_idle = true;
    Radio.Rx(0);
    return;
  }

  LoRaHeader header;
  memcpy(&header, payload, sizeof(LoRaHeader));

  // 2. Filtro de Rede Privada: descarta pacotes de vizinhos ou dispositivos desconhecidos
  if (header.netId != NETWORK_ID) {
    digitalWrite(LED, LOW);
    lora_idle = true;
    Radio.Rx(0);
    return;
  }

  // 3. Filtro de Destinatário: descarta mensagens que não sejam para esta placa
  if (header.dest != MEU_ID) {
    digitalWrite(LED, LOW);
    lora_idle = true;
    Radio.Rx(0);
    return;
  }

  // 4. Extração do conteúdo da mensagem
  uint16_t textLen = size - sizeof(LoRaHeader);
  if (textLen > BUFFER_SIZE - 1) {
    textLen = BUFFER_SIZE - 1;
  }

  memcpy(rxpacket, payload + sizeof(LoRaHeader), textLen);
  rxpacket[textLen] = '\0';

  lastRxMsg = String(rxpacket);
  lastRssi = rssi;
  lastSnr = snr;

  const char* nomeRemetente = (header.src == 1) ? "Placa A" : "Placa B";

  // Feedback detalhado no Monitor Serial
  Serial.println();
  Serial.println("---------------- Nova Mensagem Recebida ----------------");
  Serial.printf("[RECEBIDO de %s]: %s\n", nomeRemetente, rxpacket);
  Serial.printf("Qualidade : RSSI = %d dBm | SNR = %d dB | Total Bytes = %d\n", rssi, snr, size);
  Serial.println("---------------------------------------------------------");

  // Atualiza tela OLED
  atualizarDisplay();

  digitalWrite(LED, LOW);

  lora_idle = true;
  Radio.Rx(0); // Retorna ao modo de escuta
}

// ==============================================================================
// ATUALIZAÇÃO DO DISPLAY OLED 0.96" (128x64)
// ==============================================================================
void atualizarDisplay() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

  // Cabeçalho: Identidade da placa e para quem ela envia
  display.drawString(0, 0, String(NOME_MEU) + " -> " + String(NOME_DEST));
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
    display.drawString(0, 49, "Pronto | 916.5MHz");
  }

  display.display();
}
