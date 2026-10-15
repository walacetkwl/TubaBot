#include "audio.h"

namespace tb {

// notas (Hz)
enum : uint16_t {
  C4 = 262, D4 = 294, E4 = 330, F4 = 349, G4 = 392, A4 = 440, B4 = 494,
  C5 = 523, D5 = 587, E5 = 659, F5 = 698, G5 = 784, A5 = 880, B5 = 988,
  C6 = 1047, D6 = 1175, E6 = 1319, G6 = 1568, C7 = 2093,
};
#define N(h, m, w, v, s) {h, m, Wave::w, v, s}
#define R(m) {0, m, Wave::Sine, 0, 0}
#define END {0, 0, Wave::Sine, 0, 0}

static const Note S_BOOT[] = {N(C5,90,Triangle,70,0), N(E5,90,Triangle,70,0), N(G5,90,Triangle,75,0), N(C6,160,Sine,80,0), N(G6,120,Sine,45,400), END};
static const Note S_TAP[] = {N(1200,28,Sine,55,200), END};
static const Note S_DOUBLE[] = {N(1100,25,Sine,55,0), R(40), N(1500,30,Sine,55,0), END};
static const Note S_BOOP[] = {N(520,70,Triangle,70,560), N(1150,40,Sine,50,-200), END};
static const Note S_PURR[] = {
  N(98,110,Triangle,55,8), N(110,110,Triangle,35,-8), N(98,110,Triangle,55,8), N(110,110,Triangle,35,-8),
  N(98,110,Triangle,55,8), N(110,110,Triangle,35,-8), N(98,110,Triangle,50,8), N(104,160,Triangle,25,-10), END};
static const Note S_LOVE[] = {N(G5,90,Sine,65,0), N(B5,90,Sine,65,0), N(D6,90,Sine,70,0), N(G6,220,Sine,70,200), END};
static const Note S_MENU[] = {N(900,16,Sine,40,0), END};
static const Note S_TIMER[] = {N(E6,120,Square,55,0), R(60), N(C6,120,Square,55,0), R(60), N(E6,120,Square,55,0), R(60), N(C6,160,Square,55,0), R(500), END};
static const Note S_POMO[] = {N(C5,150,Triangle,70,0), N(E5,150,Triangle,70,0), N(G5,150,Triangle,70,0), N(E5,150,Triangle,70,0), N(C6,300,Sine,80,0), R(500), END};
static const Note S_REMIND[] = {N(A5,220,Sine,70,0), N(E6,380,Sine,65,0), R(400), END};
static const Note S_ALARM[] = {
  N(G5,140,Triangle,80,0), N(C6,140,Triangle,80,0), N(E6,140,Triangle,80,0), N(G6,260,Triangle,85,0), R(80),
  N(E6,140,Triangle,80,0), N(G6,380,Triangle,85,0), R(600), END};
static const Note S_POINT[] = {N(1480,35,Square,40,300), END};
static const Note S_OVER[] = {N(G4,160,Triangle,70,0), N(E4,160,Triangle,70,0), N(C4,320,Triangle,70,-80), END};
static const Note S_RECORD[] = {N(C5,110,Square,55,0), N(C5,110,Square,55,0), N(G5,110,Square,60,0), N(C6,320,Square,60,0), END};
static const Note S_UNLOCK[] = {N(E6,70,Sine,55,0), N(G6,70,Sine,55,0), N(C7,70,Sine,55,0), N(G6,70,Sine,50,0), N(C7,200,Sine,60,200), END};
static const Note S_GIGGLE[] = {N(900,55,Triangle,60,500), R(25), N(1000,55,Triangle,60,500), R(25), N(1100,55,Triangle,60,500), R(25), N(1250,80,Triangle,55,600), END};
static const Note S_DIZZY[] = {N(900,120,Triangle,60,-250), N(700,120,Triangle,60,200), N(800,120,Triangle,55,-300), N(550,120,Triangle,55,150), N(600,260,Triangle,50,-300), END};
static const Note S_STARTLED[] = {N(420,90,Triangle,75,1100), N(1500,60,Sine,55,-200), END};
static const Note S_YAWN[] = {N(520,250,Triangle,50,-80), N(440,450,Triangle,45,-200), END};
static const Note S_SWIPE[] = {N(2400,40,Noise,18,0), END};
static const Note S_CHARGE[] = {N(C5,90,Sine,60,0), N(G5,90,Sine,60,0), N(C6,180,Sine,65,300), END};
static const Note S_LOWBAT[] = {N(E5,180,Triangle,60,0), N(C5,320,Triangle,60,-60), END};
static const Note S_BREATH[] = {N(220,500,Sine,40,30), END};
static const Note S_TEST[] = {N(1000,150,Sine,70,0), END};
static const Note S_LEGEND[] = {N(G5,90,Triangle,60,0), N(C6,90,Triangle,60,0), N(E6,90,Triangle,65,0), N(G6,140,Triangle,70,0), R(50),
                                N(E6,90,Sine,60,0), N(G6,90,Sine,60,0), N(C7,320,Sine,70,240), END};
static const Note S_NOM[] = {N(300,60,Square,45,-120), R(60), N(320,60,Square,45,-120), R(60), N(280,80,Square,40,-140), END};
static const Note S_SLURP[] = {N(500,140,Triangle,45,700), N(1300,60,Sine,35,-300), END};
static const Note S_SIP[] = {N(1800,70,Noise,14,0), R(50), N(1800,90,Noise,12,0), N(600,160,Sine,40,-150), END};
static const Note S_HUM[] = {N(147,700,Sine,40,0), N(147,700,Sine,35,4), END};

#undef N
#undef R
#undef END

struct SoundDef { const Note* seq; uint8_t repeats; };
static const SoundDef DEFS[(int)Sound::Count] = {
  {nullptr, 0},
  {S_BOOT, 1}, {S_TAP, 1}, {S_DOUBLE, 1}, {S_BOOP, 1}, {S_PURR, 1}, {S_LOVE, 1}, {S_MENU, 1},
  {S_TIMER, 3}, {S_POMO, 3}, {S_REMIND, 2}, {S_ALARM, 40},
  {S_POINT, 1}, {S_OVER, 1}, {S_RECORD, 1}, {S_UNLOCK, 1}, {S_GIGGLE, 1}, {S_DIZZY, 1}, {S_STARTLED, 1},
  {S_YAWN, 1}, {nullptr, 1} /* babble: gerado */, {S_SWIPE, 1}, {S_CHARGE, 1}, {S_LOWBAT, 1}, {S_BREATH, 1}, {S_TEST, 1},
  {S_LEGEND, 1}, {S_NOM, 1}, {S_SLURP, 1}, {S_SIP, 1}, {S_HUM, 1},
};

int Synth::channelOf(Sound s) {
  return (s == Sound::TimerDone || s == Sound::PomodoroDone || s == Sound::Reminder || s == Sound::Alarm) ? 1 : 0;
}

uint32_t Synth::durationMs(Sound s) {
  if ((int)s <= 0 || (int)s >= (int)Sound::Count) return 0;
  if (s == Sound::Babble) return 900;
  const SoundDef& d = DEFS[(int)s];
  uint32_t one = 0;
  for (const Note* n = d.seq; n->ms; n++) one += n->ms;
  return one * d.repeats;
}

void Synth::play(Sound s, uint32_t seed) {
  uint32_t n = (head_ + 1) & 15;
  if (n == tail_) return;  // fila cheia: descarta (nunca bloqueia o laço)
  q_[head_] = Cmd{1, (uint8_t)s, seed};
  __atomic_store_n(&head_, n, __ATOMIC_RELEASE);
}
void Synth::stopAll() {
  uint32_t n = (head_ + 1) & 15;
  if (n == tail_) return;
  q_[head_] = Cmd{2, 0, 0};
  __atomic_store_n(&head_, n, __ATOMIC_RELEASE);
}
void Synth::stopChannel(int ch) {
  uint32_t n = (head_ + 1) & 15;
  if (n == tail_) return;
  q_[head_] = Cmd{4, 0, (uint32_t)ch};
  __atomic_store_n(&head_, n, __ATOMIC_RELEASE);
}
void Synth::setVolume(int v) {
  v = tbClamp(v, 0, 100);
  uint32_t n = (head_ + 1) & 15;
  if (n == tail_) return;
  q_[head_] = Cmd{3, 0, (uint32_t)v};
  __atomic_store_n(&head_, n, __ATOMIC_RELEASE);
}

void Synth::startNote(Voice& v) {
  const Note& nt = v.seq[v.idx];
  v.pos = 0;
  v.len = (uint32_t)nt.ms * kAudioRate / 1000;
}

void Synth::apply(const Cmd& c) {
  if (c.type == 2) { for (auto& v : v_) v.on = false; return; }
  if (c.type == 4) { if (c.arg < (uint32_t)kVoices) v_[c.arg].on = false; return; }
  if (c.type == 3) { int v = (int)c.arg; gain_ = v * v * 256 / 10000; if (v > 0 && gain_ < 8) gain_ = 8; return; }
  Sound s = (Sound)c.sound;
  if ((int)s <= 0 || (int)s >= (int)Sound::Count) return;
  Voice& v = v_[channelOf(s)];
  if (s == Sound::Babble) {
    // "fala" de robô: chilreios curtos e determinísticos a partir da semente
    uint32_t r = c.arg ? c.arg : 0x9E3779B9u;
    int n = 4 + (int)(r % 5);
    for (int i = 0; i < n && i < 11; i++) {
      r ^= r << 13; r ^= r >> 17; r ^= r << 5;
      uint16_t hz = (uint16_t)(500 + r % 1100);
      int16_t sl = (int16_t)((int)((r >> 11) % 900) - 450);
      v.gen[i] = Note{hz, (uint16_t)(50 + (r >> 20) % 70), (r & 0x100) ? Wave::Triangle : Wave::Sine, 55, sl};
    }
    v.gen[n < 11 ? n : 11] = Note{0, 0, Wave::Sine, 0, 0};
    v.seq = v.gen; v.repeats = 1;
  } else {
    v.seq = DEFS[(int)s].seq; v.repeats = DEFS[(int)s].repeats;
  }
  v.idx = 0; v.phase = 0; v.on = true;
  startNote(v);
}

// seno em tabela de 64 pontos (amplitude 32767)
static const int16_t SIN64[65] = {
  0, 3212, 6393, 9512, 12539, 15446, 18204, 20787, 23170, 25329, 27245, 28898, 30273, 31356, 32137, 32609, 32767,
  32609, 32137, 31356, 30273, 28898, 27245, 25329, 23170, 20787, 18204, 15446, 12539, 9512, 6393, 3212, 0,
  -3212, -6393, -9512, -12539, -15446, -18204, -20787, -23170, -25329, -27245, -28898, -30273, -31356, -32137, -32609, -32767,
  -32609, -32137, -31356, -30273, -28898, -27245, -25329, -23170, -20787, -18204, -15446, -12539, -9512, -6393, -3212, 0};

void Synth::render(int16_t* out, int n) {
  // consome comandos
  uint32_t h = __atomic_load_n(&head_, __ATOMIC_ACQUIRE);
  while (tail_ != h) { apply(q_[tail_]); tail_ = (tail_ + 1) & 15; }
  bool any = false;
  for (int i = 0; i < n; i++) {
    int32_t mix = 0;
    for (auto& v : v_) {
      if (!v.on) continue;
      while (v.on && v.pos >= v.len) {
        v.idx++;
        if (v.seq[v.idx].ms == 0) {
          if (v.repeats > 1) { v.repeats--; v.idx = 0; }
          else { v.on = false; break; }
        }
        if (v.on) startNote(v);
      }
      if (!v.on) continue;
      any = true;
      const Note& nt = v.seq[v.idx];
      uint32_t pos = v.pos++;
      if (!nt.hz || !nt.vol) continue;
      int32_t hz = nt.hz + (int32_t)nt.slide * (int32_t)pos / (int32_t)(v.len ? v.len : 1);
      if (hz < 20) hz = 20;
      v.phase += (uint32_t)((uint64_t)hz * 65536u * 65536u / kAudioRate);
      uint32_t ph = v.phase >> 16;  // 0..65535
      int32_t s;
      switch (nt.wave) {
        case Wave::Sine: { uint32_t i6 = ph >> 10, fr = ph & 1023; s = SIN64[i6] + (((int32_t)SIN64[i6 + 1] - SIN64[i6]) * (int32_t)fr >> 10); break; }
        case Wave::Triangle: s = ph < 32768 ? (int32_t)ph * 2 - 32768 : 98303 - (int32_t)ph * 2; break;
        case Wave::Square: s = ph < 32768 ? 22000 : -22000; break;
        case Wave::Pulse: s = ph < 16384 ? 22000 : -22000; break;
        default: lfsr_ ^= lfsr_ << 13; lfsr_ ^= lfsr_ >> 17; lfsr_ ^= lfsr_ << 5; s = (int32_t)(lfsr_ & 0xFFFF) - 32768; break;
      }
      // envelope: ataque 4 ms e soltura 12 ms em cada nota (sem estalos)
      const uint32_t att = kAudioRate * 4 / 1000, rel = kAudioRate * 12 / 1000;
      int32_t env = 256;
      if (pos < att) env = (int32_t)(pos * 256 / att);
      uint32_t left = v.len - pos;
      if (left < rel) env = tbMin<int32_t>(env, (int32_t)(left * 256 / rel));
      mix += s * nt.vol / 100 * env / 256;
    }
    mix = mix * gain_ / 256;
    if (mix > 32767) mix = 32767;
    if (mix < -32768) mix = -32768;
    out[i] = (int16_t)mix;
  }
  (void)any;
  busy_ = v_[0].on || v_[1].on;
  rendered_ += (uint32_t)n;
}

}  // namespace tb
