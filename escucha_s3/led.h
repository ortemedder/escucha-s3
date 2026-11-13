// led.h - el LED RGB cuenta como va la faccion
//  verde respirando = todo bien · amarillo = avisos · rojo = alerta grave · rosa = tick en < 30 min
//  morado 3 destellos = alerta nueva · blanco = dato de un sistema tuyo · cian = sistema actualizado
//  rojo parpadeando rapido = sin WiFi o sin EDDN
#pragma once

struct Destello { uint8_t r, g, b; uint32_t hasta; uint8_t veces; } destello = {0, 0, 0, 0, 0};
int nivelFaccion = 0;                              // 0 bien, 1 avisos, 2 grave
bool tickCerca = false;
std::vector<uint32_t> alertasVistas;               // para detectar alertas nuevas

void pideDestello(uint8_t r, uint8_t g, uint8_t b, uint8_t veces) {
  if (destello.veces > 1 && veces == 1 && millis() < destello.hasta) return;   // no tapar un aviso de alerta nueva
  destello = {r, g, b, millis() + 300u * veces, veces};
}

// misma regla que el radar y la web
int nivelAlerta(JsonObjectConst s) {
  for (JsonObjectConst c : s["conflictos"].as<JsonArrayConst>()) {
    const char *e = c["estado"] | "";
    if (!strcmp(e, "active") || !strcmp(e, "pending")) return 2;
  }
  auto tiene = [&](const char *k, const char *v) { for (JsonVariantConst x : s[k].as<JsonArrayConst>()) if (!strcmp(x | "", v)) return true; return false; };
  if (tiene("activos", "Retreat") || tiene("pendientes", "Retreat")) return 2;
  float inf = s["inf"] | 0.0f, seg = s["segunda_inf"] | 0.0f;
  if ((s["ctrl"] | false) && inf - seg < 5) return 2;
  if (!s["prev"].isNull() && s["prev"].as<float>() - inf >= 3) return 1;
  if (tiene("pendientes", "Expansion")) return 1;
  return 0;
}

// cada 10 s: nivel de la faccion, alertas nuevas y si el tick esta cerca
void revisaFaccion() {
  long a = ahora(), prox = 0;
  int nivel = 0, nuevas = 0;
  std::vector<uint32_t> ahoraAl;
  xSemaphoreTake(cerrojo, portMAX_DELAY);
  for (JsonPair p : db["sistemas"].as<JsonObject>()) {
    JsonObjectConst s = p.value();
    if (s["fuera"] | false) continue;
    int n = nivelAlerta(s);
    nivel = max(nivel, n);
    if (n) {
      uint32_t h = 2166136261u; for (const char *c = p.key().c_str(); *c; c++) { h ^= (uint8_t)*c; h *= 16777619u; }
      ahoraAl.push_back(h);
      if (!alertasVistas.empty() && std::find(alertasVistas.begin(), alertasVistas.end(), h) == alertasVistas.end()) nuevas++;
    }
    int disp = s["tick_disp"] | -1;               // proximo tick fiable de un sistema
    long tp = (long)(s["tick_prox"] | 0.0);
    if (tp && disp >= 0 && disp <= 180 && a) { while (tp < a - 1800) tp += 86400; if (!prox || tp < prox) prox = tp; }
  }
  long tg = (long)(db["tick_global"] | 0.0);
  xSemaphoreGive(cerrojo);
  if (!prox && tg && a) { prox = tg + 86400; while (prox < a - 1800) prox += 86400; }
  if (nuevas) { pideDestello(60, 0, 60, 3); Serial.printf("¡%d alerta(s) nueva(s)!\n", nuevas); }
  alertasVistas = ahoraAl;
  alertasVistas.push_back(0);                      // marca de "ya revisado" (la primera vez no avisa)
  nivelFaccion = nivel;
  tickCerca = prox && a && prox - a <= 1800 && prox - a > -1800;
}

void actualizaLed() {                              // llamar a menudo (cada ~20 ms)
  uint32_t m = millis();
  bool mal = WiFi.status() != WL_CONNECTED || !st.conectado;
  if (mal) { bool on = (m / 200) % 2; led(on ? 40 : 0, 0, 0); return; }
  if (m < destello.hasta) {                        // destellos
    bool on = destello.veces > 1 ? ((destello.hasta - m) / 150) % 2 : true;
    led(on ? destello.r : 0, on ? destello.g : 0, on ? destello.b : 0);
    return;
  }
  destello.veces = 0;
  float f = 0.15f + 0.85f * (0.5f - 0.5f * cosf(m / 4000.0f * 2 * PI));    // respiracion de 4 s
  uint8_t r, g, b;
  if (tickCerca)              { r = 40; g = 8;  b = 25; }   // rosa
  else if (nivelFaccion == 2) { r = 40; g = 0;  b = 0; }    // rojo
  else if (nivelFaccion == 1) { r = 35; g = 22; b = 0; }    // amarillo
  else                        { r = 0;  g = 30; b = 5; }    // verde
  led(r * f, g * f, b * f);
}
