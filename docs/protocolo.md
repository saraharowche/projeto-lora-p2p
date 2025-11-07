# Protocolo de mensagens - Projeto LoRa P2P

Formato de payload:
SRC=N01;DST=N02;TYPE=SOS;SEQ=123;TS=TIMESTAMP;LOC=POCO1;TXT=

Types: SOS, OK, REQ_AGUA, REQ_ALIM, REQ_MED, LOC, FREE
ACK: ACK:<SEQ>:<SRC>
