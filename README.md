# Heltec ESP32 LoRa V2 - Chat Ponto a Ponto (P2P) Endereçado

Sistema de comunicação privativo e direcionado via rádio LoRa entre duas placas **Heltec WiFi LoRa 32 (V2)**, com endereçamento de nós (Node Addressing), isolamento contra dispositivos vizinhos, e feedback em tempo real no monitor serial e display OLED 0.96".

---

## 🔒 Camadas de Isolamento e Segurança

Para evitar interferências de outros dispositivos LoRa próximos (como sensores de telemetria e caixas d'água):

1. **Endereçamento de Nós (Destino & Origem):**
   - Cada pacote contém um cabeçalho binário compacto de 4 bytes:
     `[Network ID: 0xB1A2] [Destinatário] [Remetente] [Mensagem]`
   - A **Placa A (Nó 1)** só processa mensagens destinadas ao Nó 1.
   - A **Placa B (Nó 2)** só processa mensagens destinadas ao Nó 2.
2. **Network ID Privado (`0xB1A2`):**
   - Descarta imediatamente qualquer pacote transmitido em broadcast aberto por terceiros.
3. **SyncWord Exclusivo (`0xB1`):**
   - Configurado diretamente no chip Semtech SX1276 (registrador `0x39`). O próprio hardware de rádio descarta qualquer sinal com SyncWord padrão (`0x12` ou `0x34`) antes de repassar ao microcontrolador.
4. **Frequência Dedicada (916.5 MHz):**
   - Canal deslocado para longe da frequência padrão de 915.0 MHz utilizada por sensores comerciais.

---

## 🚀 Como Gravar nas Placas

No topo do arquivo [`Bruna.ino`](Bruna.ino), escolha a identidade da placa:

### Para a Placa A:
```cpp
#define PLACA_A     // Placa 1: ID = 1 (Envia para Placa B)
//#define PLACA_B
```
Conecte a primeira placa na USB e clique em **Carregar**.

### Para a Placa B:
```cpp
//#define PLACA_A
#define PLACA_B     // Placa 2: ID = 2 (Envia para Placa A)
```
Conecte a segunda placa na USB e clique em **Carregar**.

---

## 📱 Feedback no Display OLED

O cabeçalho superior do display indica a identidade e a rota:
- Na Placa A: `Placa A -> Placa B`
- Na Placa B: `Placa B -> Placa A`
- Linha `TX:` última mensagem transmitida.
- Linha `RX:` última mensagem recebida da outra placa.
- Linha inferior: qualidade do sinal (`RSSI` e `SNR`).

---

## ⚠️ Aviso Crítico de Hardware

> **NUNCA ligue a placa sem a antena LoRa conectada.**
> Transmitir sinal de rádio sem antena pode queimar o circuito amplificador de potência (PA) do chip SX1276 permanentemente devido à alta reflexão de onda estacionária (VSWR).

---

## 🛠️ Pinout (Heltec WiFi LoRa 32 V2)

| Recurso | Pino GPIO | Função |
|---|---|---|
| **OLED SDA** | GPIO 4 | Dados I2C display |
| **OLED SCL** | GPIO 15 | Clock I2C display |
| **OLED RST** | GPIO 16 | Reset do display |
| **Vext Control** | GPIO 21 | Liga alimentação OLED (`LOW` = ligado) |
| **LED Onboard** | GPIO 25 | LED de status |
| **Botão PRG** | GPIO 0 | Botão de usuário (disparo de teste) |
| **LoRa CS / SS** | GPIO 18 | SPI Chip Select |
| **LoRa RST** | GPIO 14 | Reset do SX1276 |
| **LoRa DIO0** | GPIO 26 | Interrupção RX/TX |

---

## ⚙️ Configuração na Arduino IDE

1. **Placa:** `Ferramentas` > `Placa` > `Heltec ESP32 Series` > **WiFi LoRa 32(V2)**
2. **Monitor Serial:** **115200 baud**, com final de linha em **Nova linha (Newline)**.
