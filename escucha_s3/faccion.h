// faccion.h - datos de la faccion y actualizacion con cada mensaje de EDDN
// Hace lo mismo que update() de escucha_faccion.py (mismos campos), sin tick, Discord ni GitHub.
#pragma once
#include <vector>
#include <set>
#include <algorithm>
#include <string.h>

JsonDocument db(&psram);                           // el faccion.json entero, en PSRAM
std::vector<String> nombres;                       // nombres de los sistemas (para filtrar rapido)
std::set<uint32_t> trafHoy[200];                   // pilotos distintos hoy en cada sistema (por posicion en nombres)
char diaTraf[11] = "";

void pideDestello(uint8_t r, uint8_t g, uint8_t b, uint8_t veces = 1);
long ahora() { time_t t; time(&t); return t > 1700000000 ? (long)t : 0; }

long diasCiviles(int y, unsigned m, unsigned d) {  // dias desde 1970-01-01 (algoritmo de H. Hinnant)
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}
long iso2epoch(const char *s) {                    // "2026-11-12T13:05:12Z" -> segundos UTC
  int Y, M, D, h, mi, se;
  if (!s || sscanf(s, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &mi, &se) != 6) return 0;
  return diasCiviles(Y, M, D) * 86400L + h * 3600L + mi * 60L + se;
}

void preparaNombres() {
  xSemaphoreTake(cerrojo, portMAX_DELAY);
  nombres.clear();
  for (JsonPair p : db["sistemas"].as<JsonObject>())
    if (!p.value()["fuera"].as<bool>() && nombres.size() < 200) nombres.push_back(p.key().c_str());
  xSemaphoreGive(cerrojo);
}

int indiceNombre(const char *n) {
  for (size_t i = 0; i < nombres.size(); i++) if (nombres[i] == n) return i;
  return -1;
}

// busca un trozo de bytes dentro de otro (como memmem)
const char *busca(const char *h, size_t hl, const char *n, size_t nl) {
  if (!nl || nl > hl) return nullptr;
  const char *fin = h + hl - nl;
  for (const char *p = h; p <= fin; p++) {
    p = (const char *)memchr(p, n[0], fin - p + 1);
    if (!p) return nullptr;
    if (!memcmp(p, n, nl)) return p;
  }
  return nullptr;
}

float r2(float v) { return roundf(v * 100) / 100; }

void estados(JsonArray dst, JsonArrayConst src) {  // [{"State":"Boom"},...] -> ["Boom",...]
  for (JsonObjectConst x : src) if (x["State"].is<const char *>()) dst.add(x["State"].as<const char *>());
}

// ---------- actualizacion de un sistema (como update() del escucha de Python) ----------
bool actualiza(JsonObjectConst m, long t) {
  const char *sys = m["StarSystem"];
  JsonArrayConst facs = m["Factions"];
  if (!sys || facs.isNull()) return false;
  // facciones ordenadas por influencia
  int n = facs.size(), ord[32];
  n = min(n, 32);
  for (int i = 0; i < n; i++) ord[i] = i;
  for (int i = 1; i < n; i++) for (int j = i; j > 0 && facs[ord[j]]["Influence"].as<float>() > facs[ord[j - 1]]["Influence"].as<float>(); j--) { int x = ord[j]; ord[j] = ord[j - 1]; ord[j - 1] = x; }
  int me = -1, sec = -1;
  for (int i = 0; i < n; i++) {
    const char *nm = facs[ord[i]]["Name"];
    if (nm && !strcmp(nm, FAC)) { if (me < 0) me = ord[i]; }
    else if (sec < 0) sec = ord[i];
  }
  const char *ctrlN = m["SystemFaction"]["Name"] | "";

  JsonObject S = db["sistemas"].as<JsonObject>();
  JsonObject old = S[sys];
  bool nuevo = old.isNull();
  if (!nuevo && old["t"].as<double>() >= t) return false;     // ya tengo un dato igual o mas nuevo
  if (me < 0) {                                               // la faccion ya no esta
    if (!nuevo && !old["inf"].isNull() && !old["fuera"].as<bool>()) {
      old.clear(); old["sys"] = sys; old["t"] = t; old["src"] = "EDDN"; old["fuera"] = true; old["inf"] = 0;
      Serial.printf("¡%s ya no esta en %s!\n", FAC, sys);
      return true;
    }
    return false;
  }
  if (nuevo) old = S[sys].to<JsonObject>();
  JsonObjectConst yo = facs[me];
  float inf = r2(yo["Influence"].as<float>() * 100);
  // desde ayer (prev) e historico (hinf), igual que en Python
  if (!old["inf"].isNull() && fabsf(old["inf"].as<float>() - inf) >= 0.01f) { old["prev"] = old["inf"].as<float>(); old["prev_t"] = old["t"].as<double>(); }
  JsonArray h = old["hinf"].is<JsonArray>() ? old["hinf"].as<JsonArray>() : old["hinf"].to<JsonArray>();
  if (h.size() == 0 && !old["inf"].isNull()) {
    if (!old["prev"].isNull() && !old["prev_t"].isNull()) { JsonArray p = h.add<JsonArray>(); p.add((long)old["prev_t"].as<double>()); p.add(old["prev"].as<float>()); }
    JsonArray p = h.add<JsonArray>(); p.add((long)old["t"].as<double>()); p.add(old["inf"].as<float>());
  }
  if (h.size() == 0 || fabsf(h[h.size() - 1][1].as<float>() - inf) >= 0.01f) { JsonArray p = h.add<JsonArray>(); p.add(t); p.add(inf); }
  while (h.size() > 14) h.remove(0);

  old.remove("fuera");
  old["sys"] = sys; old["t"] = t; old["src"] = "EDDN"; old["inf"] = inf;
  old["ctrl"] = !strcmp(ctrlN, FAC);
  old["estado"] = yo["FactionState"] | "None";
  estados(old["activos"].to<JsonArray>(), yo["ActiveStates"]);
  estados(old["pendientes"].to<JsonArray>(), yo["PendingStates"]);
  estados(old["recuperando"].to<JsonArray>(), yo["RecoveringStates"]);
  old["segunda"] = sec >= 0 ? (facs[sec]["Name"] | "") : "";
  old["segunda_inf"] = sec >= 0 ? r2(facs[sec]["Influence"].as<float>() * 100) : 0.0f;
  JsonArray con = old["conflictos"].to<JsonArray>();
  for (JsonObjectConst c : m["Conflicts"].as<JsonArrayConst>()) {
    const char *f1 = c["Faction1"]["Name"] | "", *f2 = c["Faction2"]["Name"] | "";
    bool soy1 = !strcmp(f1, FAC);
    if (!soy1 && strcmp(f2, FAC)) continue;
    JsonObject x = con.add<JsonObject>();
    x["tipo"] = c["WarType"]; x["estado"] = c["Status"];
    x["contra"] = soy1 ? f2 : f1;
    x["ganados"] = c[soy1 ? "Faction1" : "Faction2"]["WonDays"];
    x["perdidos"] = c[soy1 ? "Faction2" : "Faction1"]["WonDays"];
  }
  JsonArray todas = old["todas"].to<JsonArray>();
  for (int i = 0; i < n; i++) {
    JsonObjectConst f = facs[ord[i]];
    float fi = f["Influence"].as<float>();
    if (fi <= 0) continue;
    JsonObject x = todas.add<JsonObject>();
    x["n"] = f["Name"]; x["inf"] = r2(fi * 100);
    x["gov"] = f["Government"] | ""; x["alg"] = f["Allegiance"] | "";
    estados(x["act"].to<JsonArray>(), f["ActiveStates"]);
    estados(x["pend"].to<JsonArray>(), f["PendingStates"]);
    x["jug"] = f["isPlayer"];
  }
  old["ctrl_n"] = ctrlN;
  if (m["StarPos"].is<JsonArrayConst>()) { JsonArray p = old["pos"].to<JsonArray>(); for (JsonVariantConst v : m["StarPos"].as<JsonArrayConst>()) p.add(v.as<float>()); }
  if (nuevo) Serial.printf("NUEVO sistema: %s (%.1f %%)\n", sys, inf);
  return true;
}

// trafico: pilotos distintos por dia (id anonimo de EDDN), como en Python
void cuentaTrafico(const char *sys, const char *uploader, long t) {
  int i = indiceNombre(sys);
  if (i < 0 || !uploader || !*uploader) return;
  char dia[11]; time_t tt = t; struct tm g; gmtime_r(&tt, &g); strftime(dia, sizeof dia, "%Y-%m-%d", &g);
  if (strcmp(dia, diaTraf)) { strcpy(diaTraf, dia); for (auto &s : trafHoy) s.clear(); }   // dia nuevo
  uint32_t hsh = 2166136261u; for (const char *p = uploader; *p && p - uploader < 12; p++) { hsh ^= (uint8_t)*p; hsh *= 16777619u; }
  if (!trafHoy[i].insert(hsh).second) return;                  // ya contado hoy
  JsonObject tr = db["sistemas"][sys]["trafico"].is<JsonObject>() ? db["sistemas"][sys]["trafico"].as<JsonObject>()
                                                                   : db["sistemas"][sys]["trafico"].to<JsonObject>();
  int antes = tr[dia] | 0;
  tr[dia] = max(antes, (int)trafHoy[i].size());                 // no baja de lo que ya traia la Orange Pi
  while (tr.size() > 8) { const char *viejo = nullptr; for (JsonPair p : tr) if (!viejo || strcmp(p.key().c_str(), viejo) < 0) viejo = p.key().c_str(); if (!viejo) break; tr.remove(viejo); }
}

// ---------- un mensaje de EDDN ya descomprimido ----------
void procesa(const char *txt, size_t len) {
  // filtro rapido: si no aparece ningun sistema ni la faccion, fuera (la inmensa mayoria)
  bool hit = busca(txt, len, FAC, strlen(FAC)) != nullptr;
  for (size_t i = 0; !hit && i < nombres.size(); i++) hit = busca(txt, len, nombres[i].c_str(), nombres[i].length()) != nullptr;
  if (!hit) return;
  static JsonDocument filtro;                      // solo lo que hace falta del mensaje
  if (filtro.isNull()) {
    filtro["$schemaRef"] = true; filtro["header"]["gameversion"] = true; filtro["header"]["uploaderID"] = true;
    JsonObject m = filtro["message"].to<JsonObject>();
    for (const char *k : {"event", "StarSystem", "systemName", "timestamp", "StarPos"}) m[k] = true;
    m["SystemFaction"]["Name"] = true;
    JsonObject f = m["Factions"][0].to<JsonObject>();
    for (const char *k : {"Name", "Influence", "FactionState", "Government", "Allegiance", "isPlayer"}) f[k] = true;
    for (const char *k : {"ActiveStates", "PendingStates", "RecoveringStates"}) f[k][0]["State"] = true;
    JsonObject c = m["Conflicts"][0].to<JsonObject>();
    c["WarType"] = true; c["Status"] = true;
    c["Faction1"]["Name"] = true; c["Faction1"]["WonDays"] = true; c["Faction2"]["Name"] = true; c["Faction2"]["WonDays"] = true;
  }
  JsonDocument d(&psram);
  if (deserializeJson(d, txt, len, DeserializationOption::Filter(filtro))) { st.malos++; return; }
  const char *sch = d["$schemaRef"] | "", *gv = d["header"]["gameversion"] | "";
  if (!strncmp(gv, "3.", 2) || strstr(gv, "egacy") || strstr(sch, "/test")) return;   // galaxia antigua o pruebas
  JsonObjectConst m = d["message"];
  const char *sys = m["StarSystem"] | (m["systemName"] | "");
  long t = iso2epoch(m["timestamp"]); if (!t) t = ahora();
  xSemaphoreTake(cerrojo, portMAX_DELAY);
  st.mios++; st.ultimoMio = ahora();
  cuentaTrafico(sys, d["header"]["uploaderID"], t);
  const char *ev = m["event"] | "";
  bool act = strstr(sch, "/journal/") && (!strcmp(ev, "FSDJump") || !strcmp(ev, "Location") || !strcmp(ev, "CarrierJump"))
             && busca(txt, len, FAC, strlen(FAC)) && actualiza(m, t);
  bool nuevoNombre = act && indiceNombre(sys) < 0;
  float inf = act ? (db["sistemas"][sys]["inf"] | 0.0f) : 0;
  bool ctrl = act && (db["sistemas"][sys]["ctrl"] | false);
  xSemaphoreGive(cerrojo);
  if (act) {
    st.actualizados++;
    Serial.printf("EDDN %s: %.1f%% %s\n", sys, inf, ctrl ? "(controla)" : "");
    pideDestello(0, 35, 40);                       // cian = sistema actualizado
  } else pideDestello(25, 25, 25);                 // blanco = mensaje de uno de tus sistemas
  if (nuevoNombre) preparaNombres();
}
