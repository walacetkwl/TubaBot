#include "json.h"

namespace tb {

static const char* skipWs(const char* p, const char* e) {
  while (p < e && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
  return p;
}

// Retorna ponteiro após o valor; nullptr se inválido.
static const char* skipValue(const char* p, const char* e, int depth) {
  if (depth > 12) return nullptr;
  p = skipWs(p, e);
  if (p >= e) return nullptr;
  char c = *p;
  if (c == '"') {
    p++;
    while (p < e) {
      unsigned char ch = (unsigned char)*p;
      if (ch == '"') return p + 1;
      if (ch < 0x20) return nullptr;
      if (ch == '\\') {
        p++;
        if (p >= e) return nullptr;
        if (*p == 'u') { for (int i = 1; i <= 4; i++) { if (p + i >= e) return nullptr; char h = p[i]; if (!((h >= '0' && h <= '9') || (h >= 'a' && h <= 'f') || (h >= 'A' && h <= 'F'))) return nullptr; } p += 4; }
        else if (!(*p == '"' || *p == '\\' || *p == '/' || *p == 'b' || *p == 'f' || *p == 'n' || *p == 'r' || *p == 't')) return nullptr;
      }
      p++;
    }
    return nullptr;
  }
  if (c == '{' || c == '[') {
    char close = c == '{' ? '}' : ']';
    p = skipWs(p + 1, e);
    if (p < e && *p == close) return p + 1;
    for (;;) {
      if (c == '{') {
        p = skipWs(p, e);
        if (p >= e || *p != '"') return nullptr;
        p = skipValue(p, e, depth + 1);
        if (!p) return nullptr;
        p = skipWs(p, e);
        if (p >= e || *p != ':') return nullptr;
        p++;
      }
      p = skipValue(p, e, depth + 1);
      if (!p) return nullptr;
      p = skipWs(p, e);
      if (p >= e) return nullptr;
      if (*p == ',') { p++; continue; }
      if (*p == close) return p + 1;
      return nullptr;
    }
  }
  if (c == 't') return (e - p >= 4 && tbMemcmp(p, "true", 4) == 0) ? p + 4 : nullptr;
  if (c == 'f') return (e - p >= 5 && tbMemcmp(p, "false", 5) == 0) ? p + 5 : nullptr;
  if (c == 'n') return (e - p >= 4 && tbMemcmp(p, "null", 4) == 0) ? p + 4 : nullptr;
  if (c == '-' || (c >= '0' && c <= '9')) {
    const char* s = p;
    if (*p == '-') p++;
    if (p >= e || !(*p >= '0' && *p <= '9')) return nullptr;
    while (p < e && ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' || *p == 'E' || *p == '+' || *p == '-')) p++;
    return p > s ? p : nullptr;
  }
  return nullptr;
}

bool jsonParse(const char* s, size_t n, JsonVal& root) {
  const char* e = s + n;
  const char* p = skipWs(s, e);
  const char* end = skipValue(p, e, 0);
  if (!end) return false;
  if (skipWs(end, e) != e) return false;
  root.p = p; root.n = (size_t)(end - p);
  return true;
}

static bool keyEquals(const char* q, const char* qe, const char* key) {
  // q aponta para '"' de abertura; compara sem escapes (chaves da API são ASCII simples)
  q++;
  size_t kl = tbStrlen(key);
  if ((size_t)(qe - q - 1) != kl) return false;
  return tbMemcmp(q, key, kl) == 0;
}

JsonVal JsonVal::get(const char* key) const {
  JsonVal r;
  if (!isObject()) return r;
  const char* e = p + n;
  const char* q = skipWs(p + 1, e);
  while (q < e && *q == '"') {
    const char* ke = skipValue(q, e, 1);
    if (!ke) return r;
    bool match = keyEquals(q, ke, key);
    q = skipWs(ke, e);
    if (q >= e || *q != ':') return r;
    q = skipWs(q + 1, e);
    const char* ve = skipValue(q, e, 1);
    if (!ve) return r;
    if (match) { r.p = q; r.n = (size_t)(ve - q); return r; }
    q = skipWs(ve, e);
    if (q < e && *q == ',') q = skipWs(q + 1, e);
  }
  return r;
}

JsonVal JsonVal::at(int i) const {
  JsonVal r;
  if (!isArray()) return r;
  const char* e = p + n;
  const char* q = skipWs(p + 1, e);
  int idx = 0;
  while (q < e && *q != ']') {
    const char* ve = skipValue(q, e, 1);
    if (!ve) return r;
    if (idx == i) { r.p = q; r.n = (size_t)(ve - q); return r; }
    idx++;
    q = skipWs(ve, e);
    if (q < e && *q == ',') q = skipWs(q + 1, e);
  }
  return r;
}

int JsonVal::arrayLen() const {
  int c = 0;
  while (at(c).ok()) c++;
  return c;
}

bool JsonVal::asBool(bool& out) const {
  if (!isBool()) return false;
  out = p[0] == 't';
  return true;
}

bool JsonVal::asInt(int64_t& out) const {
  if (!isNumber()) return false;
  const char* q = p; const char* e = p + n;
  bool neg = false;
  if (*q == '-') { neg = true; q++; }
  int64_t v = 0;
  int digits = 0;
  while (q < e && *q >= '0' && *q <= '9') {
    if (++digits > 18) return false;
    v = v * 10 + (*q - '0');
    q++;
  }
  if (q != e) return false;  // frações/expoentes não são aceitos onde se espera inteiro
  out = neg ? -v : v;
  return true;
}

static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : (c >= 'a' && c <= 'f' ? c - 'a' + 10 : c - 'A' + 10); }

bool JsonVal::asString(char* out, size_t cap) const {
  if (!isString() || cap == 0) return false;
  const char* q = p + 1; const char* e = p + n - 1;
  size_t o = 0;
  auto put = [&](uint8_t b) -> bool { if (o + 1 >= cap) return false; out[o++] = (char)b; return true; };
  while (q < e) {
    char c = *q++;
    if (c != '\\') { if (!put((uint8_t)c)) return false; continue; }
    char x = *q++;
    uint32_t cp = 0;
    switch (x) {
      case 'n': cp = '\n'; break; case 't': cp = '\t'; break; case 'r': cp = '\r'; break;
      case 'b': cp = 8; break; case 'f': cp = 12; break;
      case 'u': {
        cp = (uint32_t)(hexv(q[0]) << 12 | hexv(q[1]) << 8 | hexv(q[2]) << 4 | hexv(q[3]));
        q += 4;
        if (cp >= 0xD800 && cp <= 0xDBFF && q + 6 <= e && q[0] == '\\' && q[1] == 'u') {
          uint32_t lo = (uint32_t)(hexv(q[2]) << 12 | hexv(q[3]) << 8 | hexv(q[4]) << 4 | hexv(q[5]));
          if (lo >= 0xDC00 && lo <= 0xDFFF) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); q += 6; }
          else return false;
        } else if (cp >= 0xD800 && cp <= 0xDFFF) return false;
        break;
      }
      default: cp = (uint8_t)x;
    }
    if (cp == 0) return false;
    if (cp < 0x80) { if (!put((uint8_t)cp)) return false; }
    else if (cp < 0x800) { if (!put((uint8_t)(0xC0 | (cp >> 6))) || !put((uint8_t)(0x80 | (cp & 0x3F)))) return false; }
    else if (cp < 0x10000) { if (!put((uint8_t)(0xE0 | (cp >> 12))) || !put((uint8_t)(0x80 | ((cp >> 6) & 0x3F))) || !put((uint8_t)(0x80 | (cp & 0x3F)))) return false; }
    else { if (!put((uint8_t)(0xF0 | (cp >> 18))) || !put((uint8_t)(0x80 | ((cp >> 12) & 0x3F))) || !put((uint8_t)(0x80 | ((cp >> 6) & 0x3F))) || !put((uint8_t)(0x80 | (cp & 0x3F)))) return false; }
  }
  out[o] = 0;
  return utf8Valid(out, o);
}

// ── escritor ────────────────────────────────────────────────────────────────
void jsonEscape(StrBuf& sb, const char* s) {
  sb.put('"');
  for (; s && *s; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '"' || c == '\\') { sb.put('\\'); sb.put((char)c); }
    else if (c == '\n') sb.put("\\n");
    else if (c < 0x20) { sb.put("\\u00"); sb.hex(c, 2); }
    else sb.put((char)c);
  }
  sb.put('"');
}
void JsonOut::sep() { if (depth > 0 && depth < 8) { if (needComma[depth]) sb.put(','); needComma[depth] = true; } }
void JsonOut::keyName(const char* key) { sep(); if (key) { jsonEscape(sb, key); sb.put(':'); } }
JsonOut& JsonOut::beginObj(const char* key) { keyName(key); sb.put('{'); depth++; if (depth < 8) needComma[depth] = false; return *this; }
JsonOut& JsonOut::endObj() { sb.put('}'); depth--; return *this; }
JsonOut& JsonOut::beginArr(const char* key) { keyName(key); sb.put('['); depth++; if (depth < 8) needComma[depth] = false; return *this; }
JsonOut& JsonOut::endArr() { sb.put(']'); depth--; return *this; }
JsonOut& JsonOut::kstr(const char* key, const char* v) { keyName(key); jsonEscape(sb, v); return *this; }
JsonOut& JsonOut::kint(const char* key, int64_t v) { keyName(key); sb.num(v); return *this; }
JsonOut& JsonOut::kbool(const char* key, bool v) { keyName(key); sb.put(v ? "true" : "false"); return *this; }
JsonOut& JsonOut::knull(const char* key) { keyName(key); sb.put("null"); return *this; }
JsonOut& JsonOut::kraw(const char* key, const char* raw) { keyName(key); sb.put(raw); return *this; }
JsonOut& JsonOut::vstr(const char* v) { sep(); jsonEscape(sb, v); return *this; }
JsonOut& JsonOut::vint(int64_t v) { sep(); sb.num(v); return *this; }

}  // namespace tb
