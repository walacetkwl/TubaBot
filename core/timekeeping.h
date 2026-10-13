// Timers, Pomodoro, cronômetro, lembretes e relógio civil.
// Tudo baseado em tempo MONOTÔNICO: mudar hora/fuso não altera durações.
#pragma once
#include "tb_base.h"

namespace tb {

enum class RunState : uint8_t { Idle, Running, Paused, Done };

class Countdown {
 public:
  void set(uint32_t durationMs) { duration_ = durationMs; remaining_ = durationMs; state_ = RunState::Idle; }
  void start(ms_t now) { if (duration_ == 0) return; remaining_ = duration_; endAt_ = now + duration_; state_ = RunState::Running; }
  void startWithRemaining(ms_t now, uint32_t rem) { remaining_ = rem; endAt_ = now + rem; state_ = RunState::Running; }
  void pause(ms_t now) { if (state_ != RunState::Running) return; remaining_ = remainingMs(now); state_ = RunState::Paused; }
  void resume(ms_t now) { if (state_ != RunState::Paused) return; endAt_ = now + remaining_; state_ = RunState::Running; }
  void cancel() { state_ = RunState::Idle; remaining_ = duration_; }
  void acknowledge() { if (state_ == RunState::Done) { state_ = RunState::Idle; remaining_ = duration_; } }
  // true UMA vez, no momento da conclusão (mesmo com quadro atrasado).
  bool poll(ms_t now) {
    if (state_ == RunState::Running && now >= endAt_) { state_ = RunState::Done; remaining_ = 0; return true; }
    return false;
  }
  uint32_t remainingMs(ms_t now) const {
    if (state_ == RunState::Running) return now >= endAt_ ? 0 : (uint32_t)(endAt_ - now);
    return remaining_;
  }
  uint32_t durationMs() const { return duration_; }
  RunState state() const { return state_; }
  int permilleDone(ms_t now) const {
    if (!duration_) return 0;
    return (int)(1000 - (uint64_t)remainingMs(now) * 1000 / duration_);
  }
 private:
  uint32_t duration_ = 0;
  uint32_t remaining_ = 0;
  ms_t endAt_ = 0;
  RunState state_ = RunState::Idle;
};

enum class PomoPhase : uint8_t { Focus = 0, ShortBreak = 1, LongBreak = 2 };

struct PomodoroConfig { uint8_t focusMin = 25, shortMin = 5, longMin = 15, cyclesBeforeLong = 4; bool autoStart = false; };

class Pomodoro {
 public:
  PomodoroConfig cfg;
  // Eventos de conclusão
  enum class Event : uint8_t { None, FocusDone, BreakDone };

  void reset() { phase_ = PomoPhase::Focus; focusDone_ = 0; cd_.set(phaseMs(PomoPhase::Focus)); }
  void start(ms_t now) { if (cd_.state() == RunState::Idle) { cd_.set(phaseMs(phase_)); cd_.start(now); } }
  void toggle(ms_t now) {
    switch (cd_.state()) {
      case RunState::Idle: start(now); break;
      case RunState::Running: cd_.pause(now); break;
      case RunState::Paused: cd_.resume(now); break;
      default: break;
    }
  }
  // Pular etapa: avança SEM contar como foco concluído.
  void skip(ms_t now) { advance(now, false); }
  void cancel() { reset(); }
  Event poll(ms_t now) {
    if (!cd_.poll(now)) return Event::None;
    bool wasFocus = phase_ == PomoPhase::Focus;
    if (wasFocus) focusDone_++;
    advance(now, true);
    return wasFocus ? Event::FocusDone : Event::BreakDone;
  }
  // restaura de checkpoint (pausado — o tempo desligado não é adivinhado)
  void restore(PomoPhase ph, uint8_t focusDone, uint32_t remainingMs) {
    phase_ = ph; focusDone_ = focusDone; cd_.set(phaseMs(ph));
    cd_.startWithRemaining(0, remainingMs); cd_.pause(0);
  }
  PomoPhase phase() const { return phase_; }
  uint8_t focusDone() const { return focusDone_; }
  RunState state() const { return cd_.state(); }
  uint32_t remainingMs(ms_t now) const { return cd_.remainingMs(now); }
  int permille(ms_t now) const { return cd_.permilleDone(now); }
  bool active() const { return cd_.state() == RunState::Running || cd_.state() == RunState::Paused; }
  uint32_t phaseMs(PomoPhase p) const {
    uint32_t m = p == PomoPhase::Focus ? cfg.focusMin : (p == PomoPhase::ShortBreak ? cfg.shortMin : cfg.longMin);
    return m * 60000u;
  }
 private:
  void advance(ms_t now, bool completed) {
    if (phase_ == PomoPhase::Focus) {
      bool longNext = completed && cfg.cyclesBeforeLong && focusDone_ && (focusDone_ % cfg.cyclesBeforeLong) == 0;
      phase_ = longNext ? PomoPhase::LongBreak : PomoPhase::ShortBreak;
    } else {
      phase_ = PomoPhase::Focus;
    }
    cd_.set(phaseMs(phase_));
    if (cfg.autoStart) cd_.start(now);
  }
  PomoPhase phase_ = PomoPhase::Focus;
  uint8_t focusDone_ = 0;
  Countdown cd_;
};

class Stopwatch {
 public:
  static const uint64_t kLimitMs = 100ull * 3600 * 1000 - 10;  // 99:59:59,99
  void start(ms_t now) { if (state_ == RunState::Idle) { acc_ = 0; startedAt_ = now; state_ = RunState::Running; } }
  void pause(ms_t now) { if (state_ == RunState::Running) { acc_ = elapsedMs(now); state_ = RunState::Paused; } }
  void resume(ms_t now) { if (state_ == RunState::Paused) { startedAt_ = now; state_ = RunState::Running; } }
  void reset() { acc_ = 0; state_ = RunState::Idle; limitHit_ = false; }
  void toggle(ms_t now) {
    if (state_ == RunState::Idle) start(now);
    else if (state_ == RunState::Running) pause(now);
    else if (state_ == RunState::Paused && !limitHit_) resume(now);
  }
  uint64_t elapsedMs(ms_t now) const {
    uint64_t e = acc_ + (state_ == RunState::Running ? now - startedAt_ : 0);
    return e > kLimitMs ? kLimitMs : e;
  }
  // No limite máximo, para (sem voltar a zero silenciosamente).
  void poll(ms_t now) { if (state_ == RunState::Running && elapsedMs(now) >= kLimitMs) { acc_ = kLimitMs; state_ = RunState::Paused; limitHit_ = true; } }
  void restore(uint64_t elapsed) { acc_ = elapsed; state_ = elapsed ? RunState::Paused : RunState::Idle; }
  RunState state() const { return state_; }
  bool limitHit() const { return limitHit_; }
 private:
  uint64_t acc_ = 0;
  ms_t startedAt_ = 0;
  RunState state_ = RunState::Idle;
  bool limitHit_ = false;
};

// Relógio civil: válido só depois de ajustado (navegador, manual ou NTP opcional).
class CivilClock {
 public:
  void set(int64_t epochSec, ms_t now) { epochAtSet_ = epochSec; monoAtSet_ = now; valid_ = true; }
  void invalidate() { valid_ = false; }
  bool valid() const { return valid_; }
  int64_t epoch(ms_t now) const { return epochAtSet_ + (int64_t)((now - monoAtSet_) / 1000); }
  // Hora local decomposta (fuso de deslocamento fixo).
  struct Local { int year, month, day, hour, minute, second, weekday; };
  Local local(ms_t now, int offsetMin) const;
 private:
  int64_t epochAtSet_ = 0;
  ms_t monoAtSet_ = 0;
  bool valid_ = false;
};

CivilClock::Local civilFromEpoch(int64_t epochLocal);

}  // namespace tb
