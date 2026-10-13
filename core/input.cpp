#include "input.h"

namespace tb {

void GestureClassifier::reset() {
  InputTiming t = timing;
  *this = GestureClassifier();
  timing = t;
}

void GestureClassifier::push(GestureEvent* out, int& n, int maxOut, Gesture g, ms_t t) {
  if (n < maxOut) { out[n].g = g; out[n].t = t; n++; }
}

void GestureClassifier::setMode(InputMode m, ms_t now) {
  (void)now;
  if (m == mode_) return;
  mode_ = m;
  pendingTap_ = false;
  gameExitFired_ = false;
  if (stable_) {
    // a pressão em andamento pertence ao modo anterior: não vira ação no novo modo
    if (m == InputMode::Game) gameIgnoreUntilRelease_ = true;
    else { consumed_ = true; consumedKeepRecovery_ = false; }
  }
}

void GestureClassifier::consumeCurrent(bool keepRecovery) {
  pendingTap_ = false;
  if (stable_) { consumed_ = true; consumedKeepRecovery_ = keepRecovery; }
  if (mode_ == InputMode::Game && stable_) gameIgnoreUntilRelease_ = true;
}

void GestureClassifier::sample(bool level, ms_t t) {
  step(t);  // confirma uma borda pendente antes de aceitar a nova amostra
  if (level != raw_) {
    raw_ = level;
    rawChange_ = t;
    rawEdges_++;
  }
}

void GestureClassifier::onPress(ms_t t) {
  stable_ = true;
  pressStart_ = t;
  consumed_ = false;
  consumedKeepRecovery_ = false;
  gameExitFired_ = false;
  stuckReported_ = false;
}

void GestureClassifier::onRelease(ms_t t, GestureEvent* out, int& n, int maxOut) {
  stable_ = false;
  ms_t dur = t - pressStart_;
  if (stuck_) { stuck_ = false; pendingTap_ = false; return; }
  if (mode_ == InputMode::Game) { gameIgnoreUntilRelease_ = false; return; }
  if (consumed_) {
    consumed_ = false;
    if (consumedKeepRecovery_ && dur >= timing.recoveryMs) push(out, n, maxOut, Gesture::RecoveryArmed, t);
    return;
  }
  if (dur < timing.mediumMs) {
    if (pendingTap_ && pressStart_ - pendingTapAt_ <= timing.doubleWindowMs) {
      pendingTap_ = false;
      push(out, n, maxOut, Gesture::DoubleTap, t);
    } else {
      pendingTap_ = true;
      pendingTapAt_ = t;
      pendingTapPress_ = pressStart_;
    }
    return;
  }
  // pressão média/longa: um toque curto pendente antes dela é uma interação separada
  if (pendingTap_) { pendingTap_ = false; push(out, n, maxOut, Gesture::Tap, pendingTapAt_); }
  if (dur < timing.longMs) push(out, n, maxOut, Gesture::Medium, t);
  else if (dur < timing.recoveryMs) push(out, n, maxOut, Gesture::Long, t);
  else push(out, n, maxOut, Gesture::RecoveryArmed, t);
}

int GestureClassifier::poll(ms_t now, GestureEvent* out, int maxOut) {
  step(now);
  int n = 0;
  for (int i = 0; i < qn_ && n < maxOut; i++) out[n++] = q_[i];
  qn_ = 0;
  return n;
}

void GestureClassifier::step(ms_t now) {
  GestureEvent* out = q_;
  int& n = qn_;
  const int maxOut = 8;
  // Debounce: confirma a mudança se o nível bruto ficou estável por debounceMs.
  if (raw_ != stable_ && now - rawChange_ >= timing.debounceMs) {
    ms_t edge = rawChange_;  // timestamp da borda real (corrige a latência do debounce)
    if (raw_) {
      onPress(edge);
      if (mode_ == InputMode::Game && !gameIgnoreUntilRelease_ && !stuck_)
        push(out, n, maxOut, Gesture::GamePress, edge);
    } else {
      onRelease(edge, out, n, maxOut);
    }
  }
  if (stable_) {
    ms_t held = now - pressStart_;
    if (!stuck_ && held >= timing.stuckMs) {
      stuck_ = true;
      pendingTap_ = false;
      if (!stuckReported_) { stuckReported_ = true; push(out, n, maxOut, Gesture::StuckDetected, now); }
    }
    if (mode_ == InputMode::Game && !gameIgnoreUntilRelease_ && !gameExitFired_ && !stuck_ &&
        held >= timing.gameExitMs) {
      gameExitFired_ = true;
      push(out, n, maxOut, Gesture::GameHoldExit, pressStart_ + timing.gameExitMs);
    }
  }
  // Toque simples só é confirmado quando a janela de duplo toque expira
  // e nenhuma nova pressão começou dentro dela.
  if (pendingTap_ && mode_ == InputMode::Normal) {
    bool secondPressInWindow = stable_ && pressStart_ - pendingTapAt_ <= timing.doubleWindowMs;
    if (!secondPressInWindow && now - pendingTapAt_ > timing.doubleWindowMs) {
      pendingTap_ = false;
      push(out, n, maxOut, Gesture::Tap, pendingTapAt_);
    }
  }
}

HoldStage GestureClassifier::holdStage(ms_t now) const {
  if (!stable_) return HoldStage::None;
  if (stuck_) return HoldStage::Stuck;
  ms_t h = now - pressStart_;
  if (h >= timing.recoveryMs) return HoldStage::Recovery;
  if (h >= timing.longMs) return HoldStage::Long;
  if (h >= timing.mediumMs) return HoldStage::Medium;
  return HoldStage::Short;
}

}  // namespace tb
