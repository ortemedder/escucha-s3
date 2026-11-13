// zmtp.h - suscriptor ZeroMQ minimo (ZMTP 3.0, mecanismo NULL) para EDDN y descompresion zlib
// EDDN publica en tcp://eddn.edcd.io:9500 mensajes de una sola trama, comprimidos con zlib.
#pragma once
#include "inflar.h"                                // descompresor zlib (miniz) incluido en el proyecto

const char *EDDN_HOST = "eddn.edcd.io";
const uint16_t EDDN_PUERTO = 9500;
const size_t MAX_COMP = 256 * 1024;                // mensaje comprimido mas grande que se acepta
const size_t MAX_TXT = 1024 * 1024;                // y descomprimido (los de mercado pueden ser grandes)

// lee exactamente n bytes (o falla si se corta o pasan "espera" ms sin datos)
bool leeTodo(WiFiClient &c, uint8_t *b, size_t n, uint32_t espera = 60000) {
  size_t h = 0; uint32_t t = millis();
  while (h < n) {
    int k = c.read(b + h, n - h);
    if (k > 0) { h += k; t = millis(); continue; }
    if (!c.connected() || millis() - t > espera) return false;
    delay(1);
  }
  return true;
}

bool saludoZmtp(WiFiClient &c) {
  uint8_t g[64] = {0};
  g[0] = 0xFF; g[9] = 0x7F;                        // firma
  g[10] = 3; g[11] = 0;                            // version 3.0
  memcpy(g + 12, "NULL", 4);                       // mecanismo NULL (sin cifrado)
  c.write(g, 64);
  uint8_t r[64];
  if (!leeTodo(c, r, 64, 15000) || r[0] != 0xFF || r[9] != 0x7F) { Serial.println("EDDN: saludo invalido"); return false; }
  // READY con Socket-Type = SUB
  const uint8_t ready[] = {0x04, 25, 5, 'R', 'E', 'A', 'D', 'Y', 11, 'S', 'o', 'c', 'k', 'e', 't', '-', 'T', 'y', 'p', 'e', 0, 0, 0, 3, 'S', 'U', 'B'};
  c.write(ready, sizeof ready);
  // suscripcion a todo (ZMTP 3.0: mensaje con 0x01 delante del tema; tema vacio = todo)
  const uint8_t sub[] = {0x00, 1, 0x01};
  c.write(sub, sizeof sub);
  return true;
}

void tareaEddn(void *) {
  uint8_t *comp = (uint8_t *)ps_malloc(MAX_COMP);
  char *txt = (char *)ps_malloc(MAX_TXT);
  tinfl_decompressor *dec = (tinfl_decompressor *)heap_caps_malloc(sizeof(tinfl_decompressor), MALLOC_CAP_SPIRAM);
  if (!comp || !txt || !dec) { Serial.println("EDDN: sin memoria"); vTaskDelete(nullptr); }
  while (true) {
    WiFiClient c;
    st.conectado = false;
    if (WiFi.status() != WL_CONNECTED || !c.connect(EDDN_HOST, EDDN_PUERTO)) { Serial.println("EDDN: no conecta, reintento en 30 s"); delay(30000); continue; }
    c.setNoDelay(true);
    if (!saludoZmtp(c)) { c.stop(); delay(30000); continue; }
    st.conectado = true; st.conexiones++;
    Serial.println("EDDN conectado");
    while (true) {
      uint8_t cab[9];
      if (!leeTodo(c, cab, 2, 600000)) break;      // 10 min sin nada: se reconecta
      uint8_t flags = cab[0];
      uint64_t n = cab[1];
      if (flags & 0x02) {                          // tamano largo: 8 bytes (big-endian), ya tengo el primero
        if (!leeTodo(c, cab + 2, 7)) break;
        n = 0; for (int i = 1; i <= 8; i++) n = (n << 8) | cab[i];
      }
      if (n > MAX_COMP || (flags & 0x04)) {        // comando o mensaje enorme: se lee y se tira
        if (!(flags & 0x04)) st.grandes++;
        uint8_t basura[512]; uint64_t q = n; bool ok = true;
        while (q && (ok = leeTodo(c, basura, min((uint64_t)sizeof basura, q)))) q -= min((uint64_t)sizeof basura, q);
        if (!ok) break;
        continue;
      }
      if (!leeTodo(c, comp, n)) break;
      st.msgs++; st.ultimoMsg = ahora();
      // descomprimir (zlib)
      tinfl_init(dec);
      size_t inN = n, outN = MAX_TXT - 1;
      tinfl_status r = tinfl_decompress(dec, comp, &inN, (mz_uint8 *)txt, (mz_uint8 *)txt, &outN,
                                        TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
      if (r != TINFL_STATUS_DONE) { st.malos++; continue; }
      txt[outN] = 0;
      procesa(txt, outN);
    }
    Serial.println("EDDN: conexion perdida, reconecto en 30 s");
    st.conectado = false;
    c.stop();
    delay(30000);
  }
}
