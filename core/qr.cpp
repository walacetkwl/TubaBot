// Gerador de QR Code (biblioteca de Project Nayuki, licença MIT — ver
// core/third_party/qrcodegen.inc) compilado dentro do núcleo, sem libc/heap.
#include "qr.h"
#include <limits.h>

extern "C" {
void* memset(void* d, int v, size_t n);
void* memcpy(void* d, const void* s, size_t n);
void* memmove(void* d, const void* s, size_t n);
}
#define assert(x) ((void)0)
static size_t qr_strlen(const char* s) { return tb::tbStrlen(s); }
static const char* qr_strchr(const char* s, int c) { for (; *s; s++) if (*s == (char)c) return s; return c ? nullptr : s; }
static int qr_abs(int v) { return v < 0 ? -v : v; }
static long qr_labs(long v) { return v < 0 ? -v : v; }
#define strlen qr_strlen
#define strchr qr_strchr
#define abs qr_abs
#define labs qr_labs
extern "C" {
#include "third_party/qrcodegen.inc"
}
#undef strlen
#undef strchr
#undef abs
#undef labs

namespace tb {
bool qrEncodeText(const char* text, QrCode& out) {
  uint8_t tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(QrCode::kMaxVersion)];
  bool ok = qrcodegen_encodeText(text, tmp, out.data, qrcodegen_Ecc_MEDIUM, 1, QrCode::kMaxVersion, qrcodegen_Mask_AUTO, true);
  out.size = ok ? qrcodegen_getSize(out.data) : 0;
  return ok;
}
bool QrCode::module(int x, int y) const { return size && qrcodegen_getModule(data, x, y); }
}  // namespace tb
