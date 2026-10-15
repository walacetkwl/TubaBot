#include "factory.h"

namespace tb {

static const char MAGIC[] = "TUBABOT-FABRICA 1";

const char* modelName(Model m) { return m == Model::Pro ? "Pro" : "Lite"; }

static bool parseUInt(const char* s, int lo, int hi, int& out) {
  if (!*s) return false;
  int v = 0;
  for (; *s; s++) {
    if (*s < '0' || *s > '9') return false;
    v = v * 10 + (*s - '0');
    if (v > 100000) return false;
  }
  if (v < lo || v > hi) return false;
  out = v;
  return true;
}

static bool parseBool(const char* s, uint8_t& out) {
  if (tbStreq(s, "1") || tbStreq(s, "sim") || tbStreq(s, "on")) { out = 1; return true; }
  if (tbStreq(s, "0") || tbStreq(s, "nao") || tbStreq(s, "não") || tbStreq(s, "off")) { out = 0; return true; }
  return false;
}

static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool parseFactory(const char* text, size_t len, FactoryConfig& out, const char** why) {
  out = FactoryConfig();
  const char* w = nullptr;
  if (why) *why = nullptr;
  if (!text || !len) { if (why) *why = "ausente"; return false; }
  // fim real: primeiro 0x00 ou 0xFF (flash apagada)
  size_t n = 0;
  while (n < len && n < FACTORY_MAX && text[n] != 0 && (uint8_t)text[n] != 0xFF) n++;
  size_t ml = sizeof MAGIC - 1;
  if (n < ml || tbMemcmp(text, MAGIC, ml) != 0) { if (why) *why = n ? "formato desconhecido" : "ausente"; return false; }
  FactoryConfig f;
  bool crcOk = false, bodySet = false, exprSet = false;
  size_t i = 0;
  while (i < n) {
    size_t ls = i;
    while (i < n && text[i] != '\n') i++;
    size_t le = i;
    if (i < n) i++;  // pula '\n'
    if (le > ls && text[le - 1] == '\r') le--;
    if (ls == 0) continue;  // linha mágica
    if (le == ls || text[ls] == '#') continue;
    char line[96];
    size_t ll = tbMin<size_t>(le - ls, sizeof line - 1);
    tbMemcpy(line, text + ls, ll); line[ll] = 0;
    char* eq = line;
    while (*eq && *eq != '=') eq++;
    if (!*eq) { w = "linha sem '='"; continue; }
    *eq = 0;
    const char* k = line;
    const char* v = eq + 1;
    if (tbStreq(k, "crc")) {
      uint32_t c = 0; int digits = 0;
      for (const char* p = v; *p; p++) { int h = hexVal(*p); if (h < 0) { digits = -1; break; } c = (c << 4) | (uint32_t)h; digits++; }
      if (digits != 8) { if (why) *why = "crc inválido"; return false; }
      if (crc32((const uint8_t*)text, ls) != c) { if (why) *why = "crc não confere"; return false; }
      crcOk = true;
      break;
    }
    int iv = 0;
    if (tbStreq(k, "modelo")) {
      if (tbStreq(v, "pro") || tbStreq(v, "Pro")) f.model = Model::Pro;
      else if (tbStreq(v, "lite") || tbStreq(v, "Lite")) f.model = Model::Lite;
      else w = "modelo inválido";
    } else if (tbStreq(k, "cor")) {
      int b = bodyFromKey(v); if (b >= 0) { f.body = (uint8_t)b; bodySet = true; } else w = "cor inválida";
    } else if (tbStreq(k, "expressao")) {
      int e = exprColorFromKey(v); if (e >= 0) { f.exprColor = (uint8_t)e; exprSet = true; } else w = "expressao inválida";
    } else if (tbStreq(k, "tema")) {
      if (tbStreq(v, "escuro")) f.theme = 0; else if (tbStreq(v, "claro")) f.theme = 1; else w = "tema inválido";
    } else if (tbStreq(k, "volume")) {
      if (parseUInt(v, 0, 100, iv)) f.volume = (uint8_t)iv; else w = "volume inválido";
    } else if (tbStreq(k, "brilho")) {
      if (parseUInt(v, 10, 100, iv)) f.brightnessPct = (uint8_t)iv; else w = "brilho inválido";
    } else if (tbStreq(k, "personalidade")) {
      if (tbStreq(v, "tranquilo")) f.personality = 0; else if (tbStreq(v, "brincalhao") || tbStreq(v, "brincalhão")) f.personality = 1; else w = "personalidade inválida";
    } else if (tbStreq(k, "mostrar_hora")) { if (!parseBool(v, f.showClock)) w = "mostrar_hora inválido"; }
    else if (tbStreq(k, "mostrar_clima")) { if (!parseBool(v, f.showWeather)) w = "mostrar_clima inválido"; }
    else if (tbStreq(k, "mostrar_bateria")) { if (!parseBool(v, f.showBattery)) w = "mostrar_bateria inválido"; }
    else if (tbStreq(k, "formato_24h")) { if (!parseBool(v, f.clock24h)) w = "formato_24h inválido"; }
    else if (tbStreq(k, "fuso")) { int t = tzIndexFromIana(v); if (t >= 0) f.tzIndex = (uint8_t)t; else w = "fuso desconhecido"; }
    else if (tbStreq(k, "serie")) {
      if (utf8Valid(v, tbStrlen(v))) tbStrlcpy(f.serial, v, sizeof f.serial); else w = "serie inválida";
    }
    // demais chaves: ignoradas (compatibilidade com perfis mais novos)
  }
  if (!crcOk) { if (why) *why = "sem crc"; return false; }
  if (bodySet && !exprSet) f.exprColor = BODIES[f.body].exprColor;
  if (f.model == Model::Lite) f.showBattery = 0;
  f.present = true;
  out = f;
  if (why) *why = w;  // aviso não fatal (valor ignorado)
  return true;
}

size_t formatFactory(const FactoryConfig& f, char* out, size_t cap) {
  StrBuf s(out, cap);
  s.put(MAGIC).put('\n');
  s.put("modelo=").put(f.model == Model::Pro ? "pro" : "lite").put('\n');
  s.put("cor=").put(BODIES[f.body < BODY_COUNT ? f.body : 3].key).put('\n');
  s.put("expressao=").put(EXPR_COLORS[f.exprColor < EXPR_COLOR_COUNT ? f.exprColor : 3].key).put('\n');
  s.put("tema=").put(f.theme ? "claro" : "escuro").put('\n');
  s.put("volume=").num(f.volume).put('\n');
  s.put("brilho=").num(f.brightnessPct).put('\n');
  s.put("personalidade=").put(f.personality ? "brincalhao" : "tranquilo").put('\n');
  s.put("mostrar_hora=").num(f.showClock).put('\n');
  s.put("mostrar_clima=").num(f.showWeather).put('\n');
  s.put("mostrar_bateria=").num(f.showBattery).put('\n');
  s.put("formato_24h=").num(f.clock24h).put('\n');
  s.put("fuso=").put(TZ_TABLE[f.tzIndex < TZ_COUNT ? f.tzIndex : 0].iana).put('\n');
  if (f.serial[0]) s.put("serie=").put(f.serial).put('\n');
  if (s.overflow) return 0;
  uint32_t c = crc32((const uint8_t*)out, s.len);
  s.put("crc=").hex(c, 8).put('\n');
  return s.overflow ? 0 : s.len;
}

void applyFactoryDefaults(const FactoryConfig& f, Settings& s) {
  s.eyeColor = f.exprColor;
  s.theme = f.theme;
  s.volume = f.volume;
  s.brightness = (uint8_t)tbClamp((int)f.brightnessPct * 255 / 100, 10, 255);
  s.personality = f.personality;
  s.showClock = f.showClock;
  s.showWeather = f.showWeather;
  s.showBattery = f.model == Model::Pro ? f.showBattery : 0;
  s.clock24h = f.clock24h;
  s.tzIndex = f.tzIndex;
}

}  // namespace tb
