// tx_rx.ino — LoRa + OLED usando o mesmo driver do SimpleDemo

#include <Wire.h>
#include "heltec.h"
#include "HT_SSD1306Wire.h"

// Estes símbolos (SDA_OLED, SCL_OLED, RST_OLED) vêm do core Heltec.
// O SimpleDemo usa exatamente essa linha:
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED,
                           GEOMETRY_128_64, RST_OLED);

// Em muitas Heltec/LoRa32, o pino Vext alimenta o OLED
#ifndef Vext
  #define Vext 21
#endif

void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW); // Liga alimentação do OLED
}

// ====== LORA ======
#define BAND 433E6

// Em uma placa use "N01", na outra "N02"
#define NODE_ID "N02"

unsigned long ultimoEnvio = 0;
const unsigned long INTERVALO_ENVIO = 3000;
int contador = 0;

// ====== UI ======
void desenhar(const String &l1, const String &l2 = "", const String &l3 = "") {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  display.drawString(0, 0, l1);
  if (l2.length()) display.drawString(0, 14, l2);
  if (l3.length()) display.drawStringMaxWidth(0, 28, 128, l3);

  display.display();
}

// ====== SETUP ======
void setup() {
  Serial.begin(115200);
  delay(100);

  // Inicializa LoRa (usando a lib Heltec). Não vamos usar Heltec.display.
  // Parâmetros: displayOn=false, LoRaOn=true, SerialOn=true, PABOOST=true, BAND
  Heltec.begin(false, true, true, true, BAND);

  // Liga Vext para alimentar o OLED, como o SimpleDemo faz
  VextON();
  delay(100);

  // I2C nos pinos corretos da Heltec
  Wire.begin(SDA_OLED, SCL_OLED);

  // Inicializa o display exatamente como no SimpleDemo
  display.init();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.clear();
  display.drawString(0, 0, String("Node ") + NODE_ID);
  display.drawString(0, 14, "Inicializando LoRa...");
  display.display();

  // Ajusta alguns parâmetros do rádio
  LoRa.setSyncWord(0x12);         // mesma SyncWord em todos os nós
  // (Heltec.begin já chamou LoRa.begin(BAND) internamente)

  Serial.println(String("Node ") + NODE_ID + " iniciado em 433MHz");
  desenhar(String("Node ") + NODE_ID, "LoRa OK, OLED OK");
  delay(1000);
}

// ====== LOOP ======
void loop() {
  // ---------- ENVIO ----------
  if (millis() - ultimoEnvio >= INTERVALO_ENVIO) {
    ultimoEnvio = millis();

    String msg = String("PING ") + NODE_ID + " #" + String(contador++);
    Serial.println("[TX] " + msg);

    LoRa.beginPacket();
    LoRa.print(msg);
    LoRa.endPacket();

    desenhar(String("TX (") + NODE_ID + ")", "Enviando:", msg);
  }

  // ---------- RECEPÇÃO ----------
  int tam = LoRa.parsePacket();
  if (tam) {
    String recebido;
    while (LoRa.available()) {
      recebido += (char)LoRa.read();
    }
    int rssi = LoRa.packetRssi();

    Serial.println("[RX] " + recebido + " | RSSI: " + String(rssi));

    String l2 = "RSSI: " + String(rssi);
    desenhar(String("RX (") + NODE_ID + ")", l2, recebido);

    delay(1200); // tempo pra ler no OLED
  }
}
