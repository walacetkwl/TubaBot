#include "outputs.h"

namespace tb {

// Padrões do motor (opcional), herdados da v1.
static const Step V_BOOT[]   = {{120,0},{0,0}};
static const Step V_TAP[]    = {{25,0},{0,0}};
static const Step V_DOUBLE[] = {{25,60},{25,0},{0,0}};
static const Step V_PURR[]   = {{35,45},{35,45},{35,45},{35,45},{35,45},{35,45},{35,0},{0,0}};
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
static const Step V_ALARM[]  = {{400,300},{400,900},{0,0}};
static const Step V_SHORT[]  = {{30,0},{0,0}};
static const Step V_DIZZY[]  = {{40,40},{40,40},{40,40},{40,0},{0,0}};

struct PatternDef { const char* name; Sound snd; const Step* v; uint8_t vRepeats; };
static const PatternDef DEFS[(int)Pattern::Count] = {
  {"none", Sound::None, nullptr, 0},
  {"boot", Sound::Boot, V_BOOT, 1},
  {"tap", Sound::Tap, V_TAP, 1},
  {"double", Sound::Double, V_DOUBLE, 1},
  {"purr", Sound::Purr, V_PURR, 1},
  {"love", Sound::Love, V_LOVE, 1},
  {"menu", Sound::Menu, V_MENU, 1},
  {"timer", Sound::TimerDone, V_TIMER, 3},
  {"pomodoro", Sound::PomodoroDone, V_POMO, 3},
  {"reminder", Sound::Reminder, V_REMIND, 2},
  {"point", Sound::GamePoint, V_POINT, 1},
  {"gameover", Sound::GameOver, V_OVER, 1},
  {"record", Sound::Record, V_RECORD, 1},
  {"unlock", Sound::Unlock, V_UNLOCK, 1},
  {"test", Sound::Test, V_TEST, 1},
  {"alarm", Sound::Alarm, V_ALARM, 60},
  {"boop", Sound::Boop, V_SHORT, 1},
  {"giggle", Sound::Giggle, V_SHORT, 1},
  {"dizzy", Sound::Dizzy, V_DIZZY, 1},
  {"startled", Sound::Startled, V_SHORT, 1},
  {"yawn", Sound::Yawn, nullptr, 0},
  {"babble", Sound::Babble, nullptr, 0},
  {"swipe", Sound::Swipe, nullptr, 0},
  {"charge", Sound::Charge, V_SHORT, 1},
  {"lowbattery", Sound::LowBattery, V_DOUBLE, 1},
  {"breath", Sound::Breath, nullptr, 0},
  {"legend", Sound::Legend, V_UNLOCK, 1},
  {"nom", Sound::Nom, V_SHORT, 1},
  {"slurp", Sound::Slurp, V_SHORT, 1},
  {"sip", Sound::Sip, V_SHORT, 1},
  {"hum", Sound::Hum, V_SHORT, 1},
};

const char* patternName(Pattern p) { return DEFS[(int)p < (int)Pattern::Count ? (int)p : 0].name; }
Pattern patternFromName(const char* s) {
  for (int i = 1; i < (int)Pattern::Count; i++) if (tbStreq(s, DEFS[i].name)) return (Pattern)i;
  return Pattern::None;
}
bool patternIsAlert(Pattern p) { return p == Pattern::TimerDone || p == Pattern::PomodoroDone || p == Pattern::Reminder || p == Pattern::Alarm; }

void Sequencer::start(const Step* seq, uint8_t repeats, ms_t now, uint32_t maxTotalMs) {
  if (!seq || (seq[0].on == 0 && seq[0].off == 0)) { stop(); return; }
  seq_ = seq; repeatsLeft_ = repeats ? repeats - 1 : 0; idx_ = 0; inOff_ = false;
  active_ = true; level_ = seq[0].on > 0; phaseStart_ = now; startedAt_ = now; maxTotal_ = maxTotalMs;
}

bool Sequencer::update(ms_t now) {
  if (!active_) return level_ = false;
  if (now - startedAt_ >= maxTotal_) { stop(); return false; }
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

void OutputManager::configure(const HwCaps& c, const AlertPrefs& p) {
  caps_ = c;
  setPrefs(p);
}

void OutputManager::setPrefs(const AlertPrefs& p) {
  prefs_ = p;
  synth_.setVolume(p.volume);
  if (!AlertPolicy::soundAllowed(caps_, prefs_)) { synth_.stopAll(); soundUntil_ = 0; alertUntil_ = 0; }
  if (!AlertPolicy::vibeAllowed(caps_, prefs_)) { vibe_.stop(); vibeOut_ = false; }
}

void OutputManager::play(Pattern p, ms_t now, uint32_t seed) {
  if (p == Pattern::None || (int)p >= (int)Pattern::Count) return;
  const PatternDef& d = DEFS[(int)p];
  bool alert = patternIsAlert(p);
  // Um som de interface não interrompe um alerta em andamento (canal próprio).
  current_ = p;
  // Política aplicada na ORIGEM: um padrão proibido nem chega a iniciar.
  if (AlertPolicy::soundAllowed(caps_, prefs_) && d.snd != Sound::None) {
    synth_.play(d.snd, seed);
    uint32_t dur = Synth::durationMs(d.snd);
    uint32_t cap = p == Pattern::Alarm ? kMaxAlarmMs : kMaxPatternMs;
    if (dur > cap) dur = cap;
    ms_t until = now + dur;
    if (alert) alertUntil_ = until; else soundUntil_ = until;
    soundStarts_++;
    lastSound_ = p;
  }
  if (AlertPolicy::vibeAllowed(caps_, prefs_) && d.v) vibe_.start(d.v, d.vRepeats, now, p == Pattern::Alarm ? kMaxAlarmMs : kMaxPatternMs);
  else if (!AlertPolicy::vibeAllowed(caps_, prefs_)) vibe_.stop();
  update(now);
}

void OutputManager::stopAll() {
  synth_.stopAll(); vibe_.stop(); vibeOut_ = false;
  current_ = Pattern::None; soundUntil_ = alertUntil_ = 0;
}

void OutputManager::stopAlerts() {
  synth_.stopChannel(1);
  alertUntil_ = 0;
  if (patternIsAlert(current_)) { vibe_.stop(); vibeOut_ = false; current_ = Pattern::None; }
}

void OutputManager::update(ms_t now) {
  // ...e também a cada atualização: desligar som/vibração corta a saída em andamento.
  if (!AlertPolicy::soundAllowed(caps_, prefs_) && (soundUntil_ || alertUntil_)) { synth_.stopAll(); soundUntil_ = alertUntil_ = 0; }
  if (!AlertPolicy::vibeAllowed(caps_, prefs_)) vibe_.stop();
  // o alarme tem teto: depois de 2 min o som para (o aviso visual continua)
  if (alertUntil_ && now >= alertUntil_) { synth_.stopChannel(1); alertUntil_ = 0; }
  bool v = vibe_.update(now);
  if (!caps_.motor) v = false;
  if (v && !vibeOut_) vibeOnEdges_++;
  vibeOut_ = v;
  if (!soundActive(now) && !vibe_.active()) current_ = Pattern::None;
}

}  // namespace tb
