// Matemática em float sem libm (o núcleo é freestanding).
// Só operações IEEE básicas (+ − × ÷ √): o resultado é idêntico no nativo e no
// WebAssembly — o teste de equivalência compara framebuffers bit a bit.
#pragma once
#include "tb_base.h"

namespace tb {

static const float kPi = 3.14159265358979f;
static const float kTau = 6.28318530717959f;

inline float fsqrt(float v) { return v > 0.f ? __builtin_sqrtf(v) : 0.f; }
inline float fabsf_(float v) { return v < 0.f ? -v : v; }
inline float fminf_(float a, float b) { return a < b ? a : b; }
inline float fmaxf_(float a, float b) { return a > b ? a : b; }
inline float fclamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float flerp(float a, float b, float t) { return a + (b - a) * t; }
inline int ifloor(float v) { int i = (int)v; return (v < 0.f && (float)i != v) ? i - 1 : i; }
inline int iround(float v) { return ifloor(v + 0.5f); }

// seno com redução de faixa e polinômio de grau 7 (erro < 2e-4): suficiente para animação.
inline float fsin(float x) {
  float k = (float)ifloor(x / kTau + 0.5f);
  x -= k * kTau;                         // [-π, π]
  if (x > kPi * 0.5f) x = kPi - x;       // [-π/2, π/2]
  else if (x < -kPi * 0.5f) x = -kPi - x;
  float x2 = x * x;
  return x * (1.f + x2 * (-0.16666667f + x2 * (0.0083333310f + x2 * -0.00019840874f)));
}
inline float fcos(float x) { return fsin(x + kPi * 0.5f); }

// atan2 aproximado (erro < 0,005 rad), resultado em [-π, π].
inline float fatan2(float y, float x) {
  float ax = fabsf_(x), ay = fabsf_(y);
  if (ax < 1e-9f && ay < 1e-9f) return 0.f;
  float a = ax > ay ? ay / ax : ax / ay;
  float s = a * a;
  float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
  if (ay > ax) r = kPi * 0.5f - r;
  if (x < 0.f) r = kPi - r;
  if (y < 0.f) r = -r;
  return r;
}

// suavizações (t em 0..1)
inline float easeOut(float t) { t = fclamp(t, 0.f, 1.f); return 1.f - (1.f - t) * (1.f - t) * (1.f - t); }
inline float easeInOut(float t) { t = fclamp(t, 0.f, 1.f); return t < 0.5f ? 4.f * t * t * t : 1.f - (-2.f * t + 2.f) * (-2.f * t + 2.f) * (-2.f * t + 2.f) / 2.f; }
inline float easeBack(float t) { t = fclamp(t, 0.f, 1.f); const float c1 = 1.70158f, c3 = c1 + 1.f; float u = t - 1.f; return 1.f + c3 * u * u * u + c1 * u * u; }
inline float smoothstep(float a, float b, float x) { float t = fclamp((x - a) / (b - a), 0.f, 1.f); return t * t * (3.f - 2.f * t); }

}  // namespace tb
