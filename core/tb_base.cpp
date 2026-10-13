#include "tb_base.h"

namespace tb {

void tbMemset(void* d, int v, size_t n) {
  uint8_t* p = (uint8_t*)d;
  while (n--) *p++ = (uint8_t)v;
}
void tbMemcpy(void* d, const void* s, size_t n) {
  uint8_t* a = (uint8_t*)d;
  const uint8_t* b = (const uint8_t*)s;
  while (n--) *a++ = *b++;
}
int tbMemcmp(const void* a, const void* b, size_t n) {
  const uint8_t* x = (const uint8_t*)a;
  const uint8_t* y = (const uint8_t*)b;
  for (size_t i = 0; i < n; i++) if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
  return 0;
}
size_t tbStrlen(const char* s) { size_t n = 0; if (!s) return 0; while (s[n]) n++; return n; }
size_t tbStrlcpy(char* dst, const char* src, size_t cap) {
  if (!cap) return 0;
  size_t i = 0;
  if (src) while (src[i] && i + 1 < cap) { dst[i] = src[i]; i++; }
  dst[i] = 0;
  return i;
}
bool tbStreq(const char* a, const char* b) {
  if (!a || !b) return a == b;
  while (*a && *a == *b) { a++; b++; }
  return *a == *b;
}
bool tbStrStarts(const char* s, const char* p) {
  while (*p) { if (*s++ != *p++) return false; }
  return true;
}

StrBuf& StrBuf::put(char c) {
  if (len + 1 < cap) { buf[len++] = c; buf[len] = 0; } else overflow = true;
  return *this;
}
StrBuf& StrBuf::put(const char* s) { if (s) while (*s) put(*s++); return *this; }
StrBuf& StrBuf::putN(const char* s, size_t n) { for (size_t i = 0; i < n && s[i]; i++) put(s[i]); return *this; }
StrBuf& StrBuf::unum(uint64_t v) {
  char tmp[24]; int n = 0;
  do { tmp[n++] = (char)('0' + (v % 10)); v /= 10; } while (v);
  while (n) put(tmp[--n]);
  return *this;
}
StrBuf& StrBuf::num(int64_t v) {
  if (v < 0) { put('-'); return unum((uint64_t)(-(v + 1)) + 1u); }
  return unum((uint64_t)v);
}
StrBuf& StrBuf::numPad(int64_t v, int width, char pad) {
  char tmp[24]; StrBuf t(tmp, sizeof tmp); t.num(v);
  for (int i = (int)t.len; i < width; i++) put(pad);
  return put(tmp);
}
StrBuf& StrBuf::fixed2(int64_t h, char sep) {
  if (h < 0) { put('-'); h = -h; }
  unum((uint64_t)(h / 100)); put(sep);
  int r = (int)(h % 100);
  put((char)('0' + r / 10)); put((char)('0' + r % 10));
  return *this;
}
StrBuf& StrBuf::hex(uint32_t v, int digits) {
  static const char* H = "0123456789ABCDEF";
  for (int i = digits - 1; i >= 0; i--) put(H[(v >> (i * 4)) & 0xF]);
  return *this;
}

void formatDuration(StrBuf& sb, ms_t ms, bool ceilSeconds) {
  uint64_t s = ceilSeconds ? (ms + 999) / 1000 : ms / 1000;
  uint64_t h = s / 3600, m = (s / 60) % 60, sec = s % 60;
  if (h) { sb.unum(h).put(':').numPad((int64_t)m, 2); }
  else sb.numPad((int64_t)m, 2);
  sb.put(':').numPad((int64_t)sec, 2);
}

uint32_t crc32(const uint8_t* data, size_t len, uint32_t seed) {
  uint32_t c = ~seed;
  for (size_t i = 0; i < len; i++) {
    c ^= data[i];
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
  }
  return ~c;
}

uint32_t utf8Next(const char*& p, const char* end) {
  const uint8_t* s = (const uint8_t*)p;
  if (p >= end) return 0;
  uint8_t c = s[0];
  if (c < 0x80) { p++; return c; }
  int n; uint32_t cp;
  if ((c & 0xE0) == 0xC0) { n = 1; cp = c & 0x1F; }
  else if ((c & 0xF0) == 0xE0) { n = 2; cp = c & 0x0F; }
  else if ((c & 0xF8) == 0xF0) { n = 3; cp = c & 0x07; }
  else { p++; return 0xFFFD; }
  if ((size_t)(end - p) < (size_t)(n + 1)) { p = end; return 0xFFFD; }
  for (int i = 1; i <= n; i++) {
    if ((s[i] & 0xC0) != 0x80) { p += i; return 0xFFFD; }
    cp = (cp << 6) | (s[i] & 0x3F);
  }
  p += n + 1;
  // rejeita formas sobrelongas e surrogates
  if ((n == 1 && cp < 0x80) || (n == 2 && cp < 0x800) || (n == 3 && cp < 0x10000) ||
      (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) return 0xFFFD;
  return cp;
}

bool utf8Valid(const char* s, size_t len) {
  const char* p = s; const char* e = s + len;
  while (p < e) {
    const char* before = p;
    uint32_t cp = utf8Next(p, e);
    if (cp == 0xFFFD) {
      // aceita U+FFFD literal codificado corretamente (EF BF BD)
      size_t n = (size_t)(p - before);
      if (!(n == 3 && (uint8_t)before[0] == 0xEF && (uint8_t)before[1] == 0xBF && (uint8_t)before[2] == 0xBD)) return false;
    }
    if (cp == 0) return false;
  }
  return true;
}

int utf8Count(const char* s, size_t len) {
  if (!utf8Valid(s, len)) return -1;
  const char* p = s; const char* e = s + len; int n = 0;
  while (p < e) { utf8Next(p, e); n++; }
  return n;
}

}  // namespace tb
