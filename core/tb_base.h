// TubaBot core — tipos básicos e utilitários.
// O núcleo é C++17 "freestanding": sem STL, sem heap, sem exceções, sem RTTI.
// Assim o MESMO código compila para o ESP32-C3 (Arduino), para testes nativos
// (Mac/Linux) e para WebAssembly (simulador no navegador).
#pragma once

#include <stdint.h>
#include <stddef.h>

namespace tb {

// Versão do firmware/núcleo (exibida no portal e em Informações).
#define TB_VERSION "0.9.0"

// Tempo monotônico em milissegundos (64 bits internos — sem rollover prático).
typedef uint64_t ms_t;

template <typename T> inline T tbMin(T a, T b) { return a < b ? a : b; }
template <typename T> inline T tbMax(T a, T b) { return a > b ? a : b; }
template <typename T> inline T tbClamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
template <typename T> inline T tbAbs(T v) { return v < 0 ? -v : v; }

void tbMemset(void* d, int v, size_t n);
void tbMemcpy(void* d, const void* s, size_t n);
int tbMemcmp(const void* a, const void* b, size_t n);
size_t tbStrlen(const char* s);
// Copia com terminação garantida; retorna bytes copiados (sem o '\0').
size_t tbStrlcpy(char* dst, const char* src, size_t cap);
bool tbStreq(const char* a, const char* b);
bool tbStrStarts(const char* s, const char* prefix);

// Pequeno construtor de strings em buffer fixo (sem printf).
struct StrBuf {
  char* buf;
  size_t cap;
  size_t len;
  bool overflow;
  StrBuf(char* b, size_t c) : buf(b), cap(c), len(0), overflow(false) { if (c) b[0] = 0; }
  StrBuf& put(char c);
  StrBuf& put(const char* s);
  StrBuf& putN(const char* s, size_t n);
  StrBuf& num(int64_t v);
  StrBuf& unum(uint64_t v);
  StrBuf& numPad(int64_t v, int width, char pad = '0');
  // valor em centésimos -> "12,34" (vírgula decimal, pt-BR)
  StrBuf& fixed2(int64_t hundredths, char sep = ',');
  StrBuf& hex(uint32_t v, int digits);
  const char* c_str() const { return buf; }
};

// "mm:ss" ou "h:mm:ss" a partir de ms (arredonda para cima segundos restantes se ceil=true)
void formatDuration(StrBuf& sb, ms_t ms, bool ceilSeconds);

// CRC32 (polinômio IEEE) para validar blobs persistidos.
uint32_t crc32(const uint8_t* data, size_t len, uint32_t seed = 0);

// Gerador pseudoaleatório determinístico (xorshift32) — injetável nos testes.
struct Rng {
  uint32_t s;
  explicit Rng(uint32_t seed = 0x1234567u) : s(seed ? seed : 0x1234567u) {}
  void seed(uint32_t v) { s = v ? v : 0x1234567u; }
  uint32_t next() { uint32_t x = s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; s = x; return x; }
  // inteiro uniforme em [lo, hi] inclusive
  int32_t range(int32_t lo, int32_t hi) {
    if (hi <= lo) return lo;
    uint32_t span = (uint32_t)(hi - lo) + 1u;
    // rejeição para evitar viés de módulo
    uint32_t limit = 0xFFFFFFFFu - (0xFFFFFFFFu % span);
    uint32_t r;
    do { r = next(); } while (r >= limit);
    return lo + (int32_t)(r % span);
  }
};

// UTF-8: decodifica um codepoint; avança *p. Retorna 0xFFFD em sequência inválida.
uint32_t utf8Next(const char*& p, const char* end);
// Conta codepoints de uma string UTF-8 válida; retorna -1 se inválida.
int utf8Count(const char* s, size_t len);
bool utf8Valid(const char* s, size_t len);

}  // namespace tb
