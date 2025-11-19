//GATEWAY separado apenas para visualização, mas ja está implementado no central.ino para ter um arquivo só, mas podemos fazer no futuro a separação para arquivos .h
#include <WiFi.h>
#include <HTTPClient.h>
#include "heltec.h"
#include "HT_SSD1306Wire.h"

const char* WIFI_SSID     = "SEU_WIFI";
const char* WIFI_PASSWORD = "SUA_SENHA";
const char* FIREBASE_URL  = "https://lora-telemetria-default-rtdb.firebaseio.com/telemetria/gateway01.json";

static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED,
                           GEOMETRY_128_64, RST_OLED);

#ifndef Vext
  #define Vext 21
#endif

#define LORA_BAND 915E6

void ui(const String&a,const String&b,const String&c){
  digitalWrite(Vext,LOW);delay(10);
  display.init();display.clear();display.setFont(ArialMT_Plain_10);
  display.drawString(0,0,a);display.drawString(0,16,b);display.drawString(0,32,c);display.display();
}

void wifiConnect(){
  WiFi.mode(WIFI_STA);WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  ui("WiFi","Conectando...",WIFI_SSID);
  unsigned long t=millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-t<20000)delay(500);
  if(WiFi.status()==WL_CONNECTED)ui("WiFi","Conectado!",WiFi.localIP().toString());
  else ui("WiFi","Falha","Sem conexao");
}

String field(const String&p,const String&k){
  String pat=k+"=";int i=p.indexOf(pat);if(i<0)return"";int s=i+pat.length();int e=p.indexOf(';',s);if(e<0)e=p.length();return p.substring(s,e);
}

void sendFB(const String&p){
  if(WiFi.status()!=WL_CONNECTED)wifiConnect();
  String src=field(p,"SRC"); if(src=="")src=field(p,"src");
  String json="{"raw":""+p+"","src":""+src+""}";
  HTTPClient http;http.begin(FIREBASE_URL);http.addHeader("Content-Type","application/json");
  int code=http.POST(json);
  ui("Firebase","HTTP "+String(code),src);
  http.end();
}

void setup(){
  pinMode(Vext,OUTPUT);digitalWrite(Vext,LOW);
  Serial.begin(115200);delay(500);
  ui("Gateway","Inicializando","LoRa+WiFi");
  Heltec.begin(true,true,true,true,LORA_BAND);
  wifiConnect();
  ui("Gateway","Pronto","Aguardando LoRa");
}

void loop(){
  int pk=LoRa.parsePacket();
  if(pk){
    String msg="";while(LoRa.available())msg+=(char)LoRa.read();
    msg.trim();
    ui("LoRa RX","...",msg);
    sendFB(msg);
  }
  delay(10);
}
