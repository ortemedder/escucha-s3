// ESCUCHA EDDN CDSN en ESP32-S3 (N16R8) - reserva del escucha de la Orange Pi
// - Al arrancar descarga faccion.json de GitHub (sistemas y datos actuales)
// - Escucha EDDN en directo (ZeroMQ, tcp://eddn.edcd.io:9500) y actualiza los sistemas de la faccion
//   con el mismo formato que escucha_faccion.py (lo leen igual el panel de 10" y el radar)
// - Sirve los datos en http://<IP>:8090/faccion.json, el estado en /estado.json y una web detallada en /
// - NO sube a GitHub ni avisa a Discord (eso lo hace la Orange Pi)
//
// Placa: ESP32S3 Dev Module · Flash 16MB · PSRAM: OPI PSRAM · paquete esp32 2.0.17
// Librerias: ArduinoJson 7 (Benoit Blanchon)
// Archivos: escucha_s3.ino (este), zmtp.h (EDDN), faccion.h (datos), web.h (pagina), led.h (LED), inflar.h/.c (zlib), mi_wifi.h (tu WiFi)

#define VERSION "s3-v1.2"

// ---- comprobacion de los ajustes de la placa (Herramientas) ----
#if !CONFIG_IDF_TARGET_ESP32S3
#error "Placa equivocada: Herramientas -> Placa -> ESP32S3 Dev Module"
#endif
#ifndef BOARD_HAS_PSRAM
#error "Falta la PSRAM: Herramientas -> PSRAM -> OPI PSRAM"
#endif
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "mi_wifi.h"        // tu WiFi (archivo aparte)

const char *FAC = "Caballeros de Sion";
const char *URL_GITHUB = "https://raw.githubusercontent.com/ortemedder/cdsn-datos/main/faccion.json";
const int PUERTO = 8090;
const int LED_A = 48, LED_B = 38;                  // LED RGB (segun version de la placa)

// ---------- memoria: todo lo grande va a la PSRAM ----------
struct PsramAlloc : ArduinoJson::Allocator {
  void *allocate(size_t n) override { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM); }
  void deallocate(void *p) override { heap_caps_free(p); }
  void *reallocate(void *p, size_t n) override { return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM); }
} psram;

bool semillaOk = false;                            // ya tiene los datos de partida de GitHub
SemaphoreHandle_t cerrojo;                         // el escucha (nucleo 0) y la web (nucleo 1) comparten los datos
struct Stats { uint32_t msgs = 0, malos = 0, grandes = 0, mios = 0, actualizados = 0, conexiones = 0; long ultimoMio = 0, ultimoMsg = 0; bool conectado = false; } st;

void led(uint8_t r, uint8_t g, uint8_t b) { neopixelWrite(LED_A, r, g, b); neopixelWrite(LED_B, r, g, b); }

#include "faccion.h"
#include "zmtp.h"
#include "web.h"
#include "led.h"

// ---------- web ----------
WebServer web(PUERTO);

void webFaccion() {
  xSemaphoreTake(cerrojo, portMAX_DELAY);
  db["t"] = (double)ahora();
  size_t n = measureJson(db);
  char *buf = (char *)ps_malloc(n + 1);
  if (buf) serializeJson(db, buf, n + 1);
  xSemaphoreGive(cerrojo);
  if (!buf) { web.send(500, "text/plain", "sin memoria"); return; }
  web.sendHeader("Access-Control-Allow-Origin", "*");
  web.setContentLength(n);
  web.send(200, "application/json; charset=utf-8", "");
  WiFiClient c = web.client();
  for (size_t i = 0; i < n; i += 4096) c.write((const uint8_t *)buf + i, min((size_t)4096, n - i));
  free(buf);
}

uint32_t msgMin = 0;                               // mensajes en el ultimo minuto

void webInicio() { web.send_P(200, "text/html; charset=utf-8", PAGINA); }

void webEstado() {
  char t[400];
  snprintf(t, sizeof t, "{\"version\":\"%s\",\"conectado\":%s,\"conexiones\":%u,\"msgs\":%u,\"msgmin\":%u,\"malos\":%u,\"grandes\":%u,"
           "\"mios\":%u,\"actualizados\":%u,\"ultimoMsg\":%ld,\"ultimoMio\":%ld,\"min\":%lu,\"ram\":%u,\"psram\":%u,\"semilla\":%s,\"ip\":\"%s\"}",
           VERSION, st.conectado ? "true" : "false", st.conexiones, st.msgs, msgMin, st.malos, st.grandes, st.mios, st.actualizados,
           st.ultimoMsg, st.ultimoMio, (unsigned long)(millis() / 60000), ESP.getFreeHeap() / 1024, ESP.getFreePsram() / 1024,
           semillaOk ? "true" : "false", WiFi.localIP().toString().c_str());
  web.sendHeader("Access-Control-Allow-Origin", "*");
  web.send(200, "application/json", t);
}

// ---------- arranque ----------
// Datos de partida desde GitHub: se baja entero a la PSRAM, se lee y se combina con lo que ya haya
bool semilla() {
  WiFiClientSecure cli; cli.setInsecure();
  HTTPClient http; http.setTimeout(30000);
  if (!http.begin(cli, URL_GITHUB)) { Serial.println("GitHub: no se pudo empezar"); return false; }
  int cod = http.GET();
  if (cod != 200) { Serial.printf("GitHub: HTTP %d\n", cod); http.end(); return false; }
  int total = http.getSize();
  size_t cap = total > 0 ? total + 1 : 512 * 1024, n = 0;
  char *buf = (char *)ps_malloc(cap);
  if (!buf) { Serial.println("GitHub: sin memoria"); http.end(); return false; }
  WiFiClient *s = http.getStreamPtr();
  uint32_t t = millis();
  while (http.connected() && (total < 0 || (int)n < total) && millis() - t < 30000) {
    int k = s->available();
    if (k <= 0) { delay(5); continue; }
    if (n + k + 1 > cap) { cap *= 2; char *nb = (char *)ps_realloc(buf, cap); if (!nb) break; buf = nb; }
    n += s->readBytes(buf + n, min((size_t)k, cap - n - 1));
    t = millis();
  }
  http.end();
  buf[n] = 0;
  Serial.printf("GitHub: %u bytes de %d\n", (unsigned)n, total);
  JsonDocument tmp(&psram);
  DeserializationError e = deserializeJson(tmp, buf, n);
  free(buf);
  if (e) { Serial.printf("GitHub: JSON %s\n", e.c_str()); return false; }
  xSemaphoreTake(cerrojo, portMAX_DELAY);
  JsonObject S = db["sistemas"];
  int nuevos = 0;
  for (JsonPair p : tmp["sistemas"].as<JsonObject>())
    if (S[p.key()].isNull()) { S[p.key()] = p.value(); nuevos++; }   // lo recibido por EDDN es mas nuevo: se respeta
  for (JsonPair p : tmp.as<JsonObject>())
    if (strcmp(p.key().c_str(), "sistemas") && db[p.key()].isNull()) db[p.key()] = p.value();
  xSemaphoreGive(cerrojo);
  Serial.printf("GitHub: %d sistemas de partida\n", nuevos);
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.printf("\n===== ESCUCHA EDDN CDSN %s =====\n", VERSION);
  led(20, 10, 0);
  cerrojo = xSemaphoreCreateMutex();
  if (ESP.getFlashChipSize() < 16UL * 1024 * 1024) Serial.println("AVISO: Herramientas -> Flash Size -> 16MB");
  if (!ESP.getPsramSize()) { Serial.println("ERROR: sin PSRAM (Herramientas -> PSRAM: OPI PSRAM)"); led(40, 0, 0); while (true) delay(1000); }
  WiFi.mode(WIFI_STA); WiFi.setSleep(false);
  WiFi.begin(WIFI_NOMBRE, WIFI_CLAVE);
  Serial.print("WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.printf(" OK · IP %s\n", WiFi.localIP().toString().c_str());
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.google.com");
  for (int i = 0; i < 40 && !ahora(); i++) delay(250);
  db["faccion"] = FAC;
  db["sistemas"].to<JsonObject>();                 // aunque falle GitHub, lo que llegue por EDDN se guarda
  for (int i = 0; i < 3 && !(semillaOk = semilla()); i++) { Serial.println("GitHub: reintento en 10 s"); delay(10000); }
  preparaNombres();
  Serial.printf("Sistemas de partida: %d · PSRAM libre %u KB\n", (int)nombres.size(), ESP.getFreePsram() / 1024);
  web.on("/", webInicio);
  web.on("/faccion.json", webFaccion);
  web.on("/estado.json", webEstado);
  web.begin();
  Serial.printf("Web: http://%s:%d/\n", WiFi.localIP().toString().c_str(), PUERTO);
  xTaskCreatePinnedToCore(tareaEddn, "eddn", 16384, nullptr, 1, nullptr, 0);   // EDDN en el nucleo 0
}

void loop() {
  web.handleClient();
  static uint32_t tStats = 0, tLed = 0;
  uint32_t m = millis();
  if (m - tStats > 60000) {                        // resumen cada minuto
    tStats = m;
    static uint32_t antes = 0;
    Serial.printf("[%lu min] EDDN %s · %u msg/min · %s: %u (+%u act.) · RAM %u KB · PSRAM %u KB\n",
                  m / 60000, st.conectado ? "OK" : "--", st.msgs - antes, "CDSN", st.mios, st.actualizados,
                  ESP.getFreeHeap() / 1024, ESP.getFreePsram() / 1024);
    msgMin = st.msgs - antes;
    antes = st.msgs;
  }
  if (WiFi.status() != WL_CONNECTED) { actualizaLed(); static uint32_t tw = 0; if (m - tw > 10000) { tw = m; WiFi.reconnect(); } delay(20); return; }
  static uint32_t tSem = 0;                        // sin datos de partida: reintento cada 5 min
  if (!semillaOk && m - tSem > 300000) { tSem = m; if ((semillaOk = semilla())) preparaNombres(); }
  if (m - tLed > 20) { tLed = m; actualizaLed(); }
  static uint32_t tRev = 0;
  if (m - tRev > 10000) { tRev = m; revisaFaccion(); }
  delay(2);
}
