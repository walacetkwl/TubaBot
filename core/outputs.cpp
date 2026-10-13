#include "outputs.h"

namespace tb {

// Padrões ritmados próprios do TubaBot (liga/desliga; buzzer ATIVO).
static const Step B_BOOT[]   = {{40,60},{40,60},{90,0},{0,0}};
static const Step B_TAP[]    = {{18,0},{0,0}};
static const Step B_DOUBLE[] = {{18,50},{18,0},{0,0}};
static const Step B_PURR[]   = {{12,90},{12,90},{12,0},{0,0}};     // bipes discretos
static const Step B_LOVE[]   = {{30,70},{30,180},{30,70},{30,0},{0,0}};
static const Step B_MENU[]   = {{12,0},{0,0}};
static const Step B_TIMER[]  = {{120,80},{120,80},{120,600},{0,0}};
static const Step B_POMO[]   = {{250,120},{80,80},{80,600},{0,0}};
static const Step B_REMIND[] = {{60,120},{60,0},{0,0}};
static const Step B_POINT[]  = {{10,0},{0,0}};
static const Step B_OVER[]   = {{150,60},{60,0},{0,0}};
static const Step B_RECORD[] = {{40,40},{40,40},{40,40},{160,0},{0,0}};
static const Step B_UNLOCK[] = {{30,50},{30,50},{120,0},{0,0}};
static const Step B_TEST[]   = {{150,0},{0,0}};

static const Step V_BOOT[]   = {{120,0},{0,0}};
static const Step V_TAP[]    = {{25,0},{0,0}};
static const Step V_DOUBLE[] = {{25,60},{25,0},{0,0}};
static const Step V_PURR[]   = {{35,45},{35,45},{35,45},{35,45},{35,45},{35,45},{35,0},{0,0}};  // "ronronar" háptico
static const Step V_LOVE[]   = {{60,90},{60,250},{60,90},{60,0},{0,0}};
static const Step V_MENU[]   = {{15,0},{0,0}};
static const Step V_TIMER[]  = {{200,120},{200,600},{0,0}};
static const Step V_POMO[]   = {{300,150},{300,600},{0,0}};
static const Step V_REMIND[] = {{120,150},{120,0},{0,0}};
static const Step V_POINT[]  = {{15,0},{0,0}};
static const Step V_OVER[]   = {{180,0},{0,0}};
static const Step V_RECORD[] = {{60,60},{60,60},{150,0},{0,0}};
static const Step V_UNLOCK[] = {{50,60},{150,0},{0,0}};
static const Step V_TEST[]   = {{200,0},{0,0}};

struct PatternDef { const char* name; const Step* b; const Step* v; uint8_t repeats; };
static const PatternDef DEFS[(int)Pattern::Count] = {
  {"none", nullptr, nullptr, 0},
  {"boot", B_BOOT, V_BOOT, 1},
  {"tap", B_TAP, V_TAP, 1},
  {"double", B_DOUBLE, V_DOUBLE, 1},
  {"purr", B_PURR, V_PURR, 1},
  {"love", B_LOVE, V_LOVE, 1},
  {"menu", B_MENU, V_MENU, 1},
  {"timer", B_TIMER, V_TIMER, 3},      // limitado: 3 repetições, depois só visual
  {"pomodoro", B_POMO, V_POMO, 3},
  {"reminder", B_REMIND, V_REMIND, 2},
  {"point", B_POINT, V_POINT, 1},
  {"gameover", B_OVER, V_OVER, 1},
  {"record", B_RECORD, V_RECORD, 1},
  {"unlock", B_UNLOCK, V_UNLOCK, 1},
  {"test", B_TEST, V_TEST, 1},
};

const char* patternName(Pattern p) { return DEFS[(int)p < (int)Pattern::Count ? (int)p : 0].name; }
Pattern patternFromName(const char* s) {
  for (int i = 1; i < (int)Pattern::Count; i++) if (tbStreq(s, DEFS[i].name)) return (Pattern)i;
  return Pattern::None;
}

void Sequencer::start(const Step* seq, uint8_t repeats, ms_t now, uint32_t maxTotalMs) {
  if (!seq || (seq[0].on == 0 && seq[0].off == 0)) { stop(); return; }
  seq_ = seq; repeatsLeft_ = repeats ? repeats - 1 : 0; idx_ = 0; inOff_ = false;
  active_ = true; level_ = seq[0].on > 0; phaseStart_ = now; startedAt_ = now; maxTotal_ = maxTotalMs;
}

bool Sequencer::update(ms_t now) {
  if (!active_) return level_ = false;
  if (now - startedAt_ >= maxTotal_) { stop(); return false; }
  // avança várias fases se o laço atrasou (sem bloquear)
  for (int guard = 0; guard < 64 && active_; guard++) {
    const Step& s = seq_[idx_];
    uint16_t dur = inOff_ ? s.off : s.on;
    if (now - phaseStart_ < dur) break;
    phaseStart_ += dur;
    if (!inOff_) { inOff_ = true; continue; }
    inOff_ = false;
    idx_++;
    if (seq_[idx_].on == 0 && seq_[idx_].off == 0) {
      if (repeatsLeft_) { repeatsLeft_--; idx_ = 0; }
      else { stop(); return false; }
    }
  }
  level_ = active_ && !inOff_ && seq_[idx_].on > 0;
  return level_;
}

void OutputManager::play(Pattern p, ms_t now) {
  if (p == Pattern::None || (int)p >= (int)Pattern::Count) return;
  const PatternDef& d = DEFS[(int)p];
  current_ = p;
  // Política aplicada na ORIGEM: um padrão proibido nem chega a iniciar.
  if (AlertPolicy::soundAllowed(model_, prefs_)) buzz_.start(d.b, d.repeats, now, kMaxPatternMs);
  else buzz_.stop();
  if (AlertPolicy::vibeAllowed(model_, prefs_)) vibe_.start(d.v, d.repeats, now, kMaxPatternMs);
  else vibe_.stop();
  update(now);
}

void OutputManager::update(ms_t now) {
  // ...e também a cada atualização: desligar som/vibração corta a saída em andamento.
  if (!AlertPolicy::soundAllowed(model_, prefs_)) buzz_.stop();
  if (!AlertPolicy::vibeAllowed(model_, prefs_)) vibe_.stop();
  bool b = buzz_.update(now);
  bool v = vibe_.update(now);
  // Lite: nunca, sob nenhuma configuração.
  if (model_ == Model::Lite) { b = false; v = false; }
  if (b && !buzzOut_) buzzOnEdges_++;
  if (v && !vibeOut_) vibeOnEdges_++;
  buzzOut_ = b; vibeOut_ = v;
  if (!buzz_.active() && !vibe_.active()) current_ = Pattern::None;
}

}  // namespace tb
