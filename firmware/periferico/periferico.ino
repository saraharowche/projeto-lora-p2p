// === Tipo do botão (vem antes por causa da Arduino IDE) ===
enum Press {NENHUM, CURTO, LONGO, MUITO_LONGO};

#include <Wire.h>
#include "heltec.h"
#include "HT_SSD1306Wire.h" 

// ===== OLED (mesmo padrão do SimpleDemo) =====
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED,
                           GEOMETRY_128_64, RST_OLED);

// Vext alimenta o OLED em muitas Heltec/LoRa32
#ifndef Vext
  #define Vext 21
#endif

void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);   // liga alimentação do display
}

// ==== CONFIGURAÇÃO BÁSICA ====
#define BAND        433E6
#define TX_POWER    17
#define SYNC_WORD   0x12
#define BAUD        115200
#define NODE_ID     "N01"   // este nó (periférico)
#define DEST_ID     "N02"   // nó central

const int PINO_BOOT = 0;    // botão PRG

// === LISTA DE MENSAGENS ===
// Códigos usados no payload (TYPE=...)
const char* OPCOES_CODE[] = {"SOS","OK","REQ_AGUA","REQ_ALIM","REQ_MED","LOC"};

// Labels bonitos para mostrar no display
const char* OPCOES_LABEL[] = {"SOS ; Emergência","OK","Preciso de agua","Preciso de alimento","Ajuda medica","Enviar Localização"};

const int QTD = sizeof(OPCOES_CODE)/sizeof(OPCOES_CODE[0]);

int indice = 0;
uint32_t seq = 0;

// Guarda só o texto da última resposta (TXT=...)
String ultimaResposta = "";

// ---------- Helper: extrair TXT do payload ----------
// Ex.: SRC=...;TXT=EQUIPE_A_CAMINHO; -> "EQUIPE_A_CAMINHO"
String extrairTXT(const String& msg) {
  int i = msg.indexOf("TXT=");
  if (i < 0) i = msg.indexOf("txt=");  // tolera minusculo
  if (i < 0) return msg;               // fallback: mostra msg toda

  int ini = i + 4;
  int fim = msg.indexOf(';', ini);
  if (fim < 0) fim = msg.length();
  return msg.substring(ini, fim);
}

// ---------- UI ----------

void ui_menu() {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  // Linha 0–10: mensagem atual selecionada
  display.drawStringMaxWidth(0, 0, 128,
                             "Msg: " + String(OPCOES_LABEL[indice]));

  // Linha 16–40: última resposta
  display.drawString(0, 16, "Ult resp:");
  if (ultimaResposta.length() > 0) {
    display.drawStringMaxWidth(0, 26, 128, ultimaResposta);
  } else {
    display.drawString(0, 26, "-- Sem resposta ainda --");
  }

  display.display();
}

void ui_status(const String& s1, const String& s2="") {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  display.drawStringMaxWidth(0, 0, 128, s1);
  if (s2.length()) display.drawStringMaxWidth(0, 14, 128, s2);

  display.display();
}

// ---------- RADIO ----------

void radio_setup() {
  // displayOn=false (nós mesmos cuidamos do OLED)
  // LoRaOn=true, SerialOn=true, PABOOST=true
  Heltec.begin(false, true, true, true, BAND);

  LoRa.setTxPower(TX_POWER, PA_OUTPUT_PA_BOOST_PIN);
  LoRa.setSyncWord(SYNC_WORD);
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
}

// ---------- PAYLOAD ----------

String montar_payload(const String& type) {
  String loc = (type == "LOC") ? "POCO1" : "";
  //depois montar algo mais elaborado para localização

  return "SRC=" + String(NODE_ID) +
         ";DST=" + String(DEST_ID) +
         ";TYPE=" + type +
         ";SEQ=" + String(seq++) +
         ";TS=" + String((uint32_t)(millis()/1000)) +
         ";LOC=" + loc +
         ";TXT=";
}

// ---------- ACK ----------

bool aguardar_ack(uint32_t timeout_ms, const String& esperadoSeq) {
  unsigned long t0 = millis();

  while (millis() - t0 < timeout_ms) {
    int tam = LoRa.parsePacket();
    if (tam) {
      String msg;
      while (LoRa.available()) msg += (char)LoRa.read();
      msg.trim();

      Serial.println("[RX] " + msg);

      if (msg.startsWith("ACK:") && msg.indexOf(esperadoSeq) >= 0)
        return true;
    }
  }
  return false;
}

// ---------- ENVIO ----------

void enviar_corrente() {
  String tipo = OPCOES_CODE[indice];
  String payload = montar_payload(tipo);

  ui_status("Enviando...", payload);
  Serial.println("[TX] " + payload);

  LoRa.beginPacket();
  LoRa.print(payload);
  LoRa.endPacket();

  String esperado = String(seq - 1);

  for (int t = 1; t <= 3; t++) {

    if (aguardar_ack(2500, esperado)) {
      ui_status("Enviado ✓", "SEQ " + esperado);
      delay(900);
      ui_menu();
      return;
    }

    Serial.println("[WARN] Reenvio " + String(t));
    LoRa.beginPacket(); LoRa.print(payload); LoRa.endPacket();
    delay(random(0, 400));
  }

  ui_status("Falha: sem ACK");
  delay(1000);
  ui_menu();
}

// ---------- BOTÃO ----------

Press ler_press() {
  static bool emPress = false;
  static unsigned long tPress = 0;

  int v = digitalRead(PINO_BOOT);

  if (!emPress && v == LOW) {
    emPress = true;
    tPress = millis();
    delay(20); // debounce
  }

  if (emPress && v == HIGH) {
    unsigned long dur = millis() - tPress;
    emPress = false;

    if (dur >= 3000) return MUITO_LONGO; // SOS
    if (dur >= 1200) return LONGO;       // enviar
    if (dur >= 50)   return CURTO;       // próximo
  }

  return NENHUM;
}

// ---------- SETUP ----------

void setup() {
  pinMode(PINO_BOOT, INPUT_PULLUP);
  Serial.begin(BAUD);
  delay(200);

  radio_setup();

  // Liga alimentação do OLED e I2C correto, como no SimpleDemo
  VextON();
  delay(100);
  Wire.begin(SDA_OLED, SCL_OLED);

  // OLED
  display.init();
  display.clear();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "OLED OK - Periferico");
  display.display();

  delay(1200);
  ui_menu();

  Serial.println("Periferico pronto.");
}

// ---------- LOOP ----------

void loop() {
  Press p = ler_press();

  if (p == CURTO) {
    // vai pra próxima opção de mensagem
    indice = (indice + 1) % QTD;
    ui_menu();
  }
  else if (p == LONGO) {
    // envia a mensagem atual
    enviar_corrente();
  }
  else if (p == MUITO_LONGO) {
    // atalho para SOS
    int ant = indice;
    indice = 0;   // SOS
    enviar_corrente();
    indice = ant;
  }

  // Recebe respostas do central
  int tam = LoRa.parsePacket();
  if (tam) {
    String msg;
    while (LoRa.available()) msg += (char)LoRa.read();
    msg.trim();

    Serial.println("[RX] " + msg);

    if (msg.startsWith("SRC=") || msg.startsWith("src=")) {
      ultimaResposta = extrairTXT(msg); 
      ui_menu();                         // redesenha com a nova resposta
    }
  }
}
