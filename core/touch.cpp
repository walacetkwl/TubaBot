#include "touch.h"
#include "fmath.h"

namespace tb {

void TouchClassifier::reset() {
  TouchTiming t = timing;
  *this = TouchClassifier();
  timing = t;
}

void TouchClassifier::setMode(TouchMode m) {
  if (m == mode_) return;
  mode_ = m;
  pendingTap_ = false;
  gameExitFired_ = false;
  if (down_) {
    // a pressão em andamento pertence ao modo anterior: não vira ação no novo
    if (m == TouchMode::Game) gameIgnore_ = true;
    else { consumed_ = true; consumedKeepRecovery_ = false; }
  }
}

void TouchClassifier::consumeCurrent(bool keepRecovery) {
  pendingTap_ = false;
  if (down_) { consumed_ = true; consumedKeepRecovery_ = keepRecovery; }
  if (mode_ == TouchMode::Game && down_) gameIgnore_ = true;
}

void TouchClassifier::push(TouchEv e, ms_t t) {
  if (e == TouchEv::Move && qn_ > 0 && q_[qn_ - 1].type == TouchEv::Move) qn_--;  // aglutina
  if (qn_ >= 16) return;
  TouchEvent& ev = q_[qn_++];
  ev = TouchEvent();
  ev.type = e; ev.t = t;
  ev.x = (int16_t)x_; ev.y = (int16_t)y_; ev.x0 = (int16_t)x0_; ev.y0 = (int16_t)y0_;
  ev.vx = (int16_t)vx_; ev.vy = (int16_t)vy_;
  ev.hold = lastHold_;
  if (e == TouchEv::Tap && mode_ == TouchMode::UiDouble) { ev.x = (int16_t)pendX_; ev.y = (int16_t)pendY_; }
}

void TouchClassifier::sample(bool down, int x, int y, ms_t t) {
  step(t);
  if (down) {
    if (!raw_) { raw_ = true; edges_++; }
    if (!down_) {
      x_ = x0_ = lastMoveX_ = x; y_ = y0_ = lastMoveY_ = y;
      onPress(t);
      return;
    }
    // ainda pressionado (inclusive depois de uma falha curta do controlador)
    int dx = x - x_, dy = y - y_;
    if (dx || dy) {
      path_ += (uint32_t)iround(fsqrt((float)(dx * dx + dy * dy)));
      x_ = x; y_ = y;
      int ddx = x_ - x0_, ddy = y_ - y0_;
      if (!moved_ && ddx * ddx + ddy * ddy > timing.tapSlopPx * timing.tapSlopPx) moved_ = true;
      if (tbAbs(x_ - lastMoveX_) + tbAbs(y_ - lastMoveY_) >= 2) { lastMoveX_ = x_; lastMoveY_ = y_; push(TouchEv::Move, t); }
      if (moved_ && mode_ != TouchMode::Game && !consumed_ && !stuck_ && t - pressAt_ >= timing.petMinMs &&
          path_ - petPathMark_ >= timing.petStepPx) {
        petting_ = true; petPathMark_ = path_;
        lastHold_ = (uint32_t)(t - pressAt_);
        push(TouchEv::PetStroke, t);
      }
    }
    if (histN_ == 6) { for (int i = 1; i < 6; i++) hist_[i - 1] = hist_[i]; histN_ = 5; }
    hist_[histN_++] = Sm{t, (int16_t)x, (int16_t)y};
  } else if (raw_) {
    raw_ = false; rawUpAt_ = t; edges_++;
  }
}

void TouchClassifier::onPress(ms_t t) {
  down_ = true; pressAt_ = t;
  moved_ = longFired_ = petting_ = false;
  path_ = petPathMark_ = 0;
  consumed_ = consumedKeepRecovery_ = false;
  gameExitFired_ = false; stuckReported_ = false;
  vx_ = vy_ = 0;
  histN_ = 0;
  hist_[histN_++] = Sm{t, (int16_t)x_, (int16_t)y_};
  lastHold_ = 0;
  push(TouchEv::Down, t);
  if (mode_ == TouchMode::Game && !gameIgnore_ && !stuck_) push(TouchEv::GamePress, t);
}

void TouchClassifier::onRelease(ms_t t) {
  down_ = false;
  uint32_t hold = (uint32_t)(t - pressAt_);
  lastHold_ = hold;
  // velocidade de saída: últimas amostras com pelo menos ~50 ms de base
  vx_ = vy_ = 0;
  if (histN_ >= 2) {
    const Sm& b = hist_[histN_ - 1];
    int k = histN_ - 2;
    while (k > 0 && b.t - hist_[k].t < 50) k--;
    const Sm& a = hist_[k];
    ms_t dt = b.t - a.t;
    if (dt > 0 && dt < 400) { vx_ = (int)((b.x - a.x) * 1000 / (int)dt); vy_ = (int)((b.y - a.y) * 1000 / (int)dt); }
  }
  push(TouchEv::Up, t);
  if (stuck_) { stuck_ = false; pendingTap_ = false; return; }
  if (mode_ == TouchMode::Game) { gameIgnore_ = false; return; }
  if (consumed_) {
    consumed_ = false;
    if (consumedKeepRecovery_ && hold >= timing.recoveryMs && !moved_) push(TouchEv::RecoveryArmed, t);
    return;
  }
  bool isTap = !moved_ && !longFired_ && hold < timing.longMs;
  if (!isTap && pendingTap_) { pendingTap_ = false; push(TouchEv::Tap, pendingTapAt_); }
  if (!moved_ && hold >= timing.recoveryMs) { push(TouchEv::RecoveryArmed, t); return; }
  if (isTap) {
    if (mode_ == TouchMode::UiDouble) {
      if (pendingTap_ && pressAt_ - pendingTapAt_ <= timing.doubleWindowMs) { pendingTap_ = false; push(TouchEv::DoubleTap, t); }
      else { pendingTap_ = true; pendingTapAt_ = t; pendX_ = x_; pendY_ = y_; }
    } else {
      push(TouchEv::Tap, t);
    }
    return;
  }
  if (!moved_) { push(TouchEv::LongRelease, t); return; }
  int dx = x_ - x0_, dy = y_ - y0_;
  int adx = tbAbs(dx), ady = tbAbs(dy);
  if (!petting_ && hold <= timing.swipeMaxMs && tbMax(adx, ady) >= timing.swipeMinPx) {
    if (adx >= ady) push(dx < 0 ? TouchEv::SwipeLeft : TouchEv::SwipeRight, t);
    else push(dy < 0 ? TouchEv::SwipeUp : TouchEv::SwipeDown, t);
    return;
  }
  if (petting_) push(TouchEv::Pet, t);
  // senão: foi um arraste (rolagem de lista etc.), já entregue como Move/Up
}

void TouchClassifier::step(ms_t now) {
  if (down_ && !raw_ && now - rawUpAt_ >= timing.releaseDebounceMs) onRelease(rawUpAt_);
  if (down_) {
    ms_t held = now - pressAt_;
    if (!stuck_ && held >= timing.stuckMs) {
      stuck_ = true; pendingTap_ = false;
      if (!stuckReported_) { stuckReported_ = true; push(TouchEv::StuckDetected, now); }
    }
    if (mode_ == TouchMode::Game) {
      if (!gameIgnore_ && !gameExitFired_ && !stuck_ && !moved_ && held >= timing.gameExitMs) {
        gameExitFired_ = true; push(TouchEv::GameHoldExit, pressAt_ + timing.gameExitMs);
      }
    } else if (!longFired_ && !moved_ && !consumed_ && !stuck_ && held >= timing.longMs) {
      longFired_ = true;
      if (pendingTap_) { pendingTap_ = false; push(TouchEv::Tap, pendingTapAt_); }
      push(TouchEv::LongPress, pressAt_ + timing.longMs);
    }
  }
  if (pendingTap_ && mode_ == TouchMode::UiDouble) {
    bool secondInWindow = down_ && pressAt_ - pendingTapAt_ <= timing.doubleWindowMs;
    if (!secondInWindow && now - pendingTapAt_ > timing.doubleWindowMs) {
      pendingTap_ = false;
      push(TouchEv::Tap, pendingTapAt_);
    }
  }
}

int TouchClassifier::poll(ms_t now, TouchEvent* out, int maxOut) {
  step(now);
  int n = 0;
  for (int i = 0; i < qn_ && n < maxOut; i++) out[n++] = q_[i];
  qn_ = 0;
  return n;
}

}  // namespace tb
