// Clima (opcional, desligado por padrão): Open-Meteo, sem chave de API.
// O firmware só faz o GET HTTPS; quando pedir, a URL e a interpretação da
// resposta ficam aqui (testadas no PC).
#include "app.h"

namespace tb {

const char* weatherText(uint8_t c) {
  if (c == 0) return "Céu limpo";
  if (c <= 2) return "Poucas nuvens";
  if (c == 3) return "Nublado";
  if (c == 45 || c == 48) return "Neblina";
  if (c >= 51 && c <= 57) return "Garoa";
  if (c >= 61 && c <= 67) return "Chuva";
  if (c >= 71 && c <= 77) return "Neve";
  if (c >= 80 && c <= 82) return "Pancadas de chuva";
  if (c == 85 || c == 86) return "Neve";
  if (c >= 95) return "Tempestade";
  return "—";
}

Icon weatherIcon(uint8_t c, bool day) {
  if (c == 0) return day ? Icon::Sun : Icon::Moon;
  if (c <= 2) return day ? Icon::PartCloud : Icon::Cloud;
  if (c == 3) return Icon::Cloud;
  if (c == 45 || c == 48) return Icon::Fog;
  if ((c >= 51 && c <= 67) || (c >= 80 && c <= 82)) return Icon::Rain;
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return Icon::Snow;
  if (c >= 95) return Icon::Storm;
  return Icon::Cloud;
}

// Número JSON (pode ter fração) -> décimos, arredondado. Rejeita expoente.
static bool asTenths(const JsonVal& v, int32_t& out) {
  if (!v.isNumber()) return false;
  const char* p = v.p; const char* e = v.p + v.n;
  bool neg = false;
  if (p < e && *p == '-') { neg = true; p++; }
  int64_t ip = 0; int digits = 0;
  while (p < e && *p >= '0' && *p <= '9') { ip = ip * 10 + (*p - '0'); p++; if (++digits > 9) return false; }
  int frac = 0, fd = 0, next = 0;
  if (p < e && *p == '.') {
    p++;
    while (p < e && *p >= '0' && *p <= '9') { if (fd == 0) frac = *p - '0'; else if (fd == 1) next = *p - '0'; fd++; p++; }
  }
  if (p < e && (*p == 'e' || *p == 'E')) return false;
  int64_t t = ip * 10 + frac + (next >= 5 ? 1 : 0);
  out = (int32_t)(neg ? -t : t);
  return true;
}

static void putE4(StrBuf& s, int32_t v) {
  if (v < 0) { s.put('-'); v = -v; }
  s.num(v / 10000).put('.').numPad(v % 10000, 4);
}

bool App::weatherWant(ms_t now, char* url, size_t cap) {
  if (!set_.weatherEnabled) return false;
  NetStatus ns; plat_.netStatus(ns);
  if (ns.sta != StaState::Connected) return false;
  if (weatherTryAt_ && now < weatherTryAt_) return false;
  weatherTryAt_ = now + 120000;  // nova tentativa em 2 min se não houver resposta
  int32_t lat = set_.weatherLatE4, lon = set_.weatherLonE4;
  if (set_.weatherCity != 255) { lat = CITY_TABLE[set_.weatherCity].latE4; lon = CITY_TABLE[set_.weatherCity].lonE4; }
  StrBuf s(url, cap);
  s.put("https://api.open-meteo.com/v1/forecast?latitude="); putE4(s, lat);
  s.put("&longitude="); putE4(s, lon);
  s.put("&current=temperature_2m,relative_humidity_2m,weather_code,is_day"
        "&daily=weather_code,temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=3");
  return !s.overflow;
}

void App::weatherResult(const char* body, size_t len, int httpCode, ms_t now) {
  JsonVal root;
  bool ok = httpCode == 200 && body && jsonParse(body, len, root) && root.isObject();
  WeatherData w;
  if (ok) {
    JsonVal cur = root.get("current"), daily = root.get("daily");
    int32_t t = 0, hum = 0, code = 0, day = 1;
    ok = cur.isObject() && asTenths(cur.get("temperature_2m"), t) && asTenths(cur.get("weather_code"), code);
    if (ok) {
      w.tempC10 = (int16_t)t; w.code = (uint8_t)tbClamp<int32_t>(code / 10, 0, 99);
      if (asTenths(cur.get("relative_humidity_2m"), hum)) w.humidity = (uint8_t)tbClamp<int32_t>(hum / 10, 0, 100);
      if (asTenths(cur.get("is_day"), day)) w.isDay = day >= 5;
      if (daily.isObject()) {
        JsonVal dc = daily.get("weather_code"), dx = daily.get("temperature_2m_max"), dn = daily.get("temperature_2m_min");
        int n = dc.isArray() ? tbMin(dc.arrayLen(), 3) : 0;
        for (int i = 0; i < n; i++) {
          int32_t c = 0, hi = 0, lo = 0;
          if (!asTenths(dc.at(i), c) || !asTenths(dx.at(i), hi) || !asTenths(dn.at(i), lo)) break;
          w.days[i] = WeatherData::Day{(uint8_t)tbClamp<int32_t>(c / 10, 0, 99), (int16_t)hi, (int16_t)lo};
          w.nDays = i + 1;
        }
      }
      if (w.nDays) { w.hiC10 = w.days[0].hiC10; w.loC10 = w.days[0].loC10; }
      else { w.hiC10 = w.loC10 = w.tempC10; }
    }
  }
  if (ok) {
    w.valid = true; w.updatedAt = now;
    weather_ = w; weatherFailed_ = false;
    weatherTryAt_ = now + 30ull * 60000;  // atualiza a cada 30 min
    event("clima atualizado");
  } else {
    weatherFailed_ = true;
    weatherTryAt_ = now + 5ull * 60000;
    event("clima: falha na consulta");
  }
  forceFrame_ = true;
}

}  // namespace tb
