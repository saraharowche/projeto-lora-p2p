// === Tipo do botão (vem antes só por causa da Arduino IDE que lê depois do include as funçoes) ===
enum Press {NENHUM, CURTO, LONGO, MUITO_LONGO};

#include <Wire.h>
#include "heltec.h"
#include "HT_SSD1306Wire.h"  // mesmo driver do SimpleDemo

// Mesmo construtor do SimpleDemo (usa SDA_OLED, SCL_OLED, RST_OLED)
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
#define NODE_ID     "N02"   // este nó (central)
#define DEST_ID     "N01"   // nó periférico

const int PINO_BOOT = 0;    // botão PRG

// Respostas rápidas do central

// código interno 
const char* RESPOSTAS_CODE[] = {
  "OK_RECEBIDO",
  "EQUIPE_A_CAMINHO",
  "AGUARDE_CONTATO",
  "SEM_RECURSOS",
  "REGISTRADO"
};

// label para tela (vai no TXT por enquanto)
const char* RESPOSTAS_LABEL[] = {
  "OK recebido",
  "Equipe a caminho",
  "Aguarde contato",
  "Sem recursos no momento",
  "Pedido registrado"
};

const int QTD_RES = sizeof(RESPOSTAS_CODE)/sizeof(RESPOSTAS_CODE[0]);
int indiceResp = 0;

// Guarda última mensagem recebida (para poder responder)
String ultimoSRC = "";
String ultimoSEQ = "";
String ultimoTYPE = "";
String ultimoLOC = "";

// ---------- Helpers de parsing ----------

// Extrai campos como SRC=..., DST=..., TYPE=...
String extrairCampo(const String& msg, const String& chave) {
  int i = msg.indexOf(chave + "=");
  if (i < 0) return "";
  int inicio = i + chave.length() + 1;
  int fim = msg.indexOf(';', inicio);
  if (fim < 0) fim = msg.length();
  return msg.substring(inicio, fim);
}

// Converte TYPE=... em um texto mais amigavel para mostrar no OLED
String labelTipo(const String& type) {
  if (type == "SOS")       return "SOS / emergencia";
  if (type == "OK")        return "Estou bem";
  if (type == "REQ_AGUA")  return "Pede agua";
  if (type == "REQ_ALIM")  return "Pede alimento";
  if (type == "REQ_MED")   return "Pede ajuda medica";
  if (type == "LOC")       return "Enviou localizacao";
  if (type == "RESP")      return "Resposta";
  return type; // fallback: se nao conhecer, mostra o codigo mesmo
}

// ---------- UI ----------

void ui_menu() {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  display.drawString(0, 0,  String("Central ") + NODE_ID);

  if (ultimoSRC.length()) {
  String tipoBonito = labelTipo(ultimoTYPE);
  String linha = "Ultimo: " + ultimoSRC + " " + tipoBonito;
  display.drawStringMaxWidth(0, 12, 128, linha);

  if (ultimoLOC.length()) {
    display.drawStringMaxWidth(0, 32, 128, "Loc: " + ultimoLOC);
  }
} else {
  display.drawString(0, 12, "Aguardando msgs do periferico...");
}

  display.drawString(0, 48, "Resp.: " + String(RESPOSTAS_LABEL[indiceResp]));
  //display.drawString(0, 54, "PRG curto=Prox  longo=Enviar");

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
  // displayOn=false (nós mesmas cuidamos do OLED),
  // LoRaOn=true, SerialOn=true, PABOOST=true
  Heltec.begin(false, true, true, true, BAND);

  LoRa.setTxPower(TX_POWER, PA_OUTPUT_PA_BOOST_PIN);
  LoRa.setSyncWord(SYNC_WORD);
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
}

// ---------- ACK ----------

void enviar_ack(const String& seq, const String& src) {
  String ack = "ACK:" + seq + ":" + src;
  Serial.println("[TX] " + ack);

  LoRa.beginPacket();
  LoRa.print(ack);
  LoRa.endPacket();
}

// ---------- RESPOSTA PARA O PERIFÉRICO ----------

void enviar_resposta() {
  if (ultimoSRC == "" || ultimoSEQ == "") {
    ui_status("Nenhuma msg para responder",
              "Aguarde uma msg do periferico.");
    delay(1400);
    ui_menu();
    return;
  }

  String txt = RESPOSTAS_LABEL[indiceResp];

  // Resposta segue o mesmo formato geral:
  // SRC=N02;DST=N01;TYPE=RESP;SEQ=<igual da msg>;TS=...;LOC=<igual>;TXT=<resposta>
  String payload =
    "SRC=" + String(NODE_ID) +
    ";DST=" + String(DEST_ID) +
    ";TYPE=RESP;" +
    "SEQ=" + ultimoSEQ + ";" +
    "TS=" + String((uint32_t)(millis()/1000)) + ";" +
    "LOC=" + ultimoLOC + ";" +
    "TXT=" + txt;

  ui_status("Enviando resposta...", payload);
  Serial.println("[TX] " + payload);

  LoRa.beginPacket();
  LoRa.print(payload);
  LoRa.endPacket();

  delay(900);
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

    if (dur >= 3000) return MUITO_LONGO; // futuro: outra ação (ex: alerta especial)
    if (dur >= 1200) return LONGO;       // enviar resposta
    if (dur >= 50)   return CURTO;       // próxima resposta
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

  display.init();
  display.clear();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "OLED OK - Central");
  display.display();

  delay(1200);
  ui_menu();

  Serial.println("Central pronto.");
}

// ---------- LOOP ----------

void loop() {
  // Leitura do botão PRG
  Press p = ler_press();

  if (p == CURTO) {
    // troca resposta pré-definida
    indiceResp = (indiceResp + 1) % QTD_RES;
    ui_menu();
  }
  else if (p == LONGO) {
    // envia resposta para o último pedido recebido
    enviar_resposta();
  }
  // MUITO_LONGO pode ser usado depois para alguma função extra (ex.: reset, alerta especial)

  // Receber mensagens do periférico
  int tam = LoRa.parsePacket();
  if (tam) {
    String msg;
    while (LoRa.available()) msg += (char)LoRa.read();
    msg.trim();

    Serial.println("[RX] " + msg);

    if (msg.startsWith("SRC=")) {
      String src = extrairCampo(msg, "SRC");
      String dst = extrairCampo(msg, "DST");
      String type = extrairCampo(msg, "TYPE");
      String seq  = extrairCampo(msg, "SEQ");
      String loc  = extrairCampo(msg, "LOC");

      if (dst == NODE_ID) {
        // Guarda a última mensagem
        ultimoSRC  = src;
        ultimoTYPE = type;
        ultimoSEQ  = seq;
        ultimoLOC  = loc;

        // Envia ACK para o periférico
        enviar_ack(seq, src);

        // Mostra no OLED um resumo
        String linha = "De " + src + " Tipo=" + type + " Loc=" + loc;
        ui_status("Msg recebida!", linha);
        delay(1000);
        ui_menu();
      }
    }
  }
}