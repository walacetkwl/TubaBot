#include "app.h"
#include "app_ids.h"

namespace tb {

static const PersonalityTuning TUNE_CALM = {3000, 6500, 4000, 9000, 22, 14, 10, 3};
static const PersonalityTuning TUNE_PLAY = {2000, 4500, 1800, 4500, 34, 18, 25, 18};

const char* screenName(Screen s) {
  static const char* const N[(int)Screen::Count] = {
    "intro", "first_use", "face", "clock", "today", "launcher", "more_apps", "quick", "pomodoro", "timers", "timer_set", "stopwatch",
    "alarms", "alarm_edit", "games", "game", "expressions", "breath", "level", "dice", "weather", "stats",
    "appearance", "color", "overlays", "volume", "brightness", "wifi", "system", "config", "info", "torch",
    "confirm", "recovery", "resume", "screen_off", "ota"};
  return (int)s < (int)Screen::Count ? N[(int)s] : "?";
}

static const uint16_t INTRO_FULL_MS = 2800, INTRO_SHORT_MS = 900;

// ── inicialização e persistência ────────────────────────────────────────────
void App::loadAll(ms_t now) {
  (void)now;
  uint8_t buf[BLOB_MAX];
  size_t len = 0;
  // perfil de fábrica primeiro: ele define os padrões das preferências desta unidade
  {
    char fb[FACTORY_MAX];
    size_t fl = 0;
    facWhy_ = nullptr;
    if (plat_.factoryLoad(fb, sizeof fb, &fl)) parseFactory(fb, fl, fac_, &facWhy_);
    else { fac_ = FactoryConfig(); facWhy_ = "ausente"; }
  }
  defaultSettings(set_);
  applyFactoryDefaults(fac_, set_);
  DecodeResult r = DecodeResult::Empty;
  if (plat_.storageLoad(BlobKind::Settings, buf, sizeof buf, &len)) r = decodeBlob(BlobKind::Settings, SETTINGS_VERSION, buf, len, &set_, sizeof set_);
  if (r != DecodeResult::Ok && r != DecodeResult::Empty && r != DecodeResult::Migrated) { defaultSettings(set_); applyFactoryDefaults(fac_, set_); recovered_ = true; setDirty_ = true; }
  if (r == DecodeResult::Migrated) setDirty_ = true;
  sanitizeSettings(set_);
  if (fac_.model != Model::Pro) set_.showBattery = 0;

  defaultProgress(prog_);
  r = DecodeResult::Empty; len = 0;
  if (plat_.storageLoad(BlobKind::Progress, buf, sizeof buf, &len)) r = decodeBlob(BlobKind::Progress, PROGRESS_VERSION, buf, len, &prog_, sizeof prog_);
  if (r != DecodeResult::Ok && r != DecodeResult::Empty && r != DecodeResult::Migrated) { defaultProgress(prog_); recovered_ = true; progDirty_ = true; }
  prog_.mood = tbClamp<int16_t>(prog_.mood, -100, 100);
  prog_.bond = tbMin<uint16_t>(prog_.bond, 1000);
  prog_.unlocked &= (1u << UNL_COUNT) - 1;
  if (prog_.timingBestPct100 > 10000) prog_.timingBestPct100 = 0;

  tbMemset(&ckpt_, 0, sizeof ckpt_);
  r = DecodeResult::Empty; len = 0;
  if (plat_.storageLoad(BlobKind::Checkpoint, buf, sizeof buf, &len)) r = decodeBlob(BlobKind::Checkpoint, CHECKPOINT_VERSION, buf, len, &ckpt_, sizeof ckpt_);
  if (r != DecodeResult::Ok) tbMemset(&ckpt_, 0, sizeof ckpt_);
  if (ckpt_.pomoPhase > 2) ckpt_.valid = 0;
}

void App::flush(ms_t now, bool force) {
  if (!setDirty_ && !progDirty_ && !ckptDirty_) return;
  if (!force && (now - lastChange_ < 1500 || (lastSave_ && now - lastSave_ < 3000))) return;
  uint8_t buf[BLOB_MAX];
  if (setDirty_) {
    size_t n = encodeBlob(BlobKind::Settings, SETTINGS_VERSION, &set_, sizeof set_, buf, sizeof buf);
    if (n && plat_.storageSave(BlobKind::Settings, buf, n)) { setDirty_ = false; writes_++; } else event("falha ao salvar preferências");
  }
  if (progDirty_) {
    size_t n = encodeBlob(BlobKind::Progress, PROGRESS_VERSION, &prog_, sizeof prog_, buf, sizeof buf);
    if (n && plat_.storageSave(BlobKind::Progress, buf, n)) { progDirty_ = false; writes_++; } else event("falha ao salvar progresso");
  }
  if (ckptDirty_) {
    size_t n = encodeBlob(BlobKind::Checkpoint, CHECKPOINT_VERSION, &ckpt_, sizeof ckpt_, buf, sizeof buf);
    if (n && plat_.storageSave(BlobKind::Checkpoint, buf, n)) { ckptDirty_ = false; writes_++; } else event("falha ao salvar sessão");
  }
  lastSave_ = now;
}

void App::applyPrefs(ms_t now) {
  AlertPrefs ap; ap.sound = set_.sound; ap.silent = set_.silent; ap.vibration = set_.vibration; ap.volume = set_.volume;
  out_.configure(caps_, ap);  // política central
  pomo_.cfg.focusMin = set_.pomoFocusMin; pomo_.cfg.shortMin = set_.pomoShortMin; pomo_.cfg.longMin = set_.pomoLongMin;
  pomo_.cfg.cyclesBeforeLong = set_.pomoCyclesBeforeLong; pomo_.cfg.autoStart = set_.pomoAutoStart;
  if (pomo_.state() == RunState::Idle) pomo_.reset();
  for (int i = 0; i < kTimers; i++)
    if (timers_[i].state() == RunState::Idle && timers_[i].durationMs() == 0) timers_[i].set(set_.timerPresetS[1 + i] * 1000);
  plat_.setTimezone(TZ_TABLE[set_.tzIndex].posix);
  if (!set_.micReactions) { sound_.reset(); face_.setDance(false, 0); }
  if (fac_.model != Model::Pro) set_.showBattery = 0;
  applyTheme();
  updateBrightness(now);
}

void App::begin(const HwCaps& caps, ms_t now, uint32_t seed, bool devBuild) {
  caps_ = caps; devBuild_ = devBuild;
  rng_.seed(seed);
  loadAll(now);       // 1º: fábrica + preferências — antes de QUALQUER saída
  // Lite x Pro é propriedade de fábrica: no Lite não existe bateria (nem leitura, nem opções)
  caps_.battery = caps.battery && fac_.model == Model::Pro;
  appliedTheme_ = -1;
  applyTheme();
  applyPrefs(now);
  pomo_.reset();
  for (int i = 0; i < kTimers; i++) timers_[i].set(set_.timerPresetS[1 + i] * 1000);
  StrBuf t(token_, sizeof token_); t.hex(rng_.next(), 8).hex(rng_.next(), 8);
  StrBuf p(pin_, sizeof pin_); p.numPad(rng_.range(0, 999999), 6);
  face_.reset(now);
  baseExpr_ = (Expr)set_.favoriteExpr;
  face_.setExpression(baseExpr_, now, true);
  bootAt_ = screenAt_ = lastInteraction_ = lastMinuteTick_ = now;
  nextIdleExpr_ = now + 60000;
  nextDrift_ = now + 120000;
  for (int i = 0; i < REMINDER_COUNT; i++) reminderDue_[i] = now + (ms_t)set_.reminders[i].intervalMin * 60000;
  screen_ = Screen::Intro;
  setInputModeFor(screen_);
  canvas_.invalidateAll();
  out_.play(Pattern::Boot, now);  // já obedece silencioso/volume salvos
  event(recovered_ ? "boot (dados restaurados ao padrão)" : "boot");
  forceFrame_ = true;
}

// ── utilidades ──────────────────────────────────────────────────────────────
void App::event(const char* msg) {
  tbStrlcpy(lastEvent_, msg, sizeof lastEvent_);
  eventSeq_++;
  plat_.log(msg);
}

void App::toast(const char* msg, ms_t now, uint16_t ms) {
  tbStrlcpy(toast_, msg, sizeof toast_);
  toastUntil_ = now + ms;
  forceFrame_ = true;
}

void App::noteInteraction(ms_t now) {
  lastInteraction_ = now;
  energy_ = tbMin(100, energy_ + 5);
  nextIdleExpr_ = now + 60000;
}

bool App::listScreen(Screen s) const {
  return s == Screen::GamesMenu || s == Screen::TimerSet || s == Screen::Appearance || s == Screen::Overlays ||
         s == Screen::System || s == Screen::Wifi;
}

void App::setInputModeFor(Screen s) {
  TouchMode m = TouchMode::Ui;
  if (alertN_ == 0 && s == Screen::Game) m = TouchMode::Game;
  else if (alertN_ == 0 && s == Screen::Face) m = TouchMode::UiDouble;
  input_.setMode(m);
}

void App::gotoScreen(Screen s, ms_t now) {
  if (screen_ == Screen::Game && s != Screen::Game && s != Screen::Confirm && s != Screen::RecoveryConfirm && s != Screen::ScreenOff) game_ = nullptr;
  Screen prev = screen_;
  screen_ = s; screenAt_ = now; forceFrame_ = true; lastInteraction_ = now;
  pressedHit_ = -1; pressedItem_ = -1; quickDrag_ = -1; quickArm_ = -1;
  if (homeScreen(s)) homePage_ = s;
  setInputModeFor(s);
  if (s == Screen::Face) { face_.setExpression(baseExpr_, now); exprUntil_ = 0; }
  if (s != Screen::Face) dockUntil_ = 0;
  if (listScreen(s) || gridScreen(s)) buildList(now);
  if (s == Screen::Expressions && prev != Screen::Expressions) exprSel_ = 0;
  canvas_.invalidateAll();
}

void App::goHome(ms_t now, Screen page) { transDir_ = 0; gotoScreen(page, now); }

void App::goBack(ms_t now) {
  transDir_ = 1;
  switch (screen_) {
    case Screen::Launcher: case Screen::Quick: gotoScreen(homePage_, now); break;
    case Screen::TimerSet: gotoScreen(Screen::Timers, now); break;
    case Screen::AlarmEdit: gotoScreen(Screen::Alarms, now); break;
    case Screen::Game: game_ = nullptr; gotoScreen(Screen::GamesMenu, now); break;
    case Screen::Config: gotoScreen(set_.setupDone ? Screen::Wifi : Screen::FirstUse, now); break;
    case Screen::Torch: gotoScreen(returnScreen_ == Screen::Torch ? homePage_ : returnScreen_, now); break;
    case Screen::ColorPick: case Screen::Overlays: gotoScreen(Screen::Appearance, now); break;
    case Screen::Info: gotoScreen(returnScreen_ == Screen::System ? Screen::System : Screen::MoreApps, now); break;
    case Screen::Stopwatch: case Screen::Alarms: case Screen::Weather: case Screen::Breath: case Screen::Dice:
    case Screen::Expressions: case Screen::Stats: case Screen::Level:
      gotoScreen(Screen::MoreApps, now); break;
    default:
      if (homeScreen(screen_)) return;
      gotoScreen(Screen::Launcher, now);
  }
}

int App::availableExprs(Expr* out, int max) const {
  static const Expr base[] = {Expr::Happy, Expr::Neutral, Expr::Surprised, Expr::Love, Expr::Sad, Expr::Sleepy, Expr::Angry,
                              Expr::WinkL, Expr::WinkR, Expr::Pet, Expr::Dead};
  int n = 0;
  for (Expr e : base) if (n < max) out[n++] = e;
  const Progress& Pr = testMode_ ? progTest_ : prog_;
  for (int i = 0; i < UNL_COUNT; i++) if ((Pr.unlocked >> i) & 1u) if (n < max) out[n++] = exprForUnlock(i);
  return n;
}

void App::unlock(UnlockId id, ms_t now) {
  Progress& Pr = P();
  if ((Pr.unlocked >> id) & 1u) return;
  Pr.unlocked |= 1u << id;
  markProgress(now);
  char b[40]; StrBuf s(b, sizeof b); s.put("Nova: ").put(exprName(exprForUnlock(id))).put("!");
  toast(b, now, 2600);
  out_.play(Pattern::Unlock, now);
  event(b);
  if (id != UNL_RADIANTE) checkRadiant(now);
}

void App::checkRadiant(ms_t now) {
  uint32_t others = ((1u << UNL_COUNT) - 1) & ~(1u << UNL_RADIANTE);
  if ((P().unlocked & others) == others) unlock(UNL_RADIANTE, now);
}

void App::addPet(ms_t now) {
  if (lastPetAt_ && now - lastPetAt_ < 2000) return;  // ruído/toques repetidos não multiplicam
  lastPetAt_ = now;
  Progress& Pr = P();
  Pr.pets++;
  Pr.mood = (int16_t)tbMin(100, Pr.mood + 5);
  Pr.bond = (uint16_t)tbMin(1000, Pr.bond + 3);
  markProgress(now);
  unlock(UNL_TIMIDO, now);
}

void App::react(Expr e, ms_t now, uint16_t ms, Pattern snd) {
  face_.setExpression(e, now);
  exprUntil_ = now + ms;
  if (snd != Pattern::None) out_.play(snd, now, rng_.next());
  forceFrame_ = true;
}

FaceStyle App::faceStyle() const {
  FaceStyle st;
  bool light = set_.theme != 0;
  st.eye = exprColorRgb(set_.eyeColor, light);
  st.glow = set_.eyeGlow != 0 && !light;  // no fundo branco o halo vira mancha: só no escuro
  st.light = light;
  st.fx = light ? mixColor(st.eye, 0x000000, 60) : (Color)0xFFFFFF;
  st.zzz = light ? (Color)0x4F5BC4 : (Color)0xA9B4FF;
  return st;
}

// Tema da interface + cor de destaque (a cor das expressões). Trocar de tema
// repinta a tela inteira com o novo fundo.
void App::applyTheme() {
  bool light = set_.theme != 0;
  Color accent = exprColorRgb(set_.eyeColor, light);
  // destaque legível: "branco" no escuro e "grafite" no claro viram neutros fortes
  ui::setTheme(light, accent);
  if (appliedTheme_ != (int)light || appliedAccent_ != accent) {
    appliedTheme_ = light; appliedAccent_ = accent;
    canvas_.invalidateAll();
    forceFrame_ = true;
  }
}

// ── alertas ─────────────────────────────────────────────────────────────────
void App::startAlertOutput(ms_t now) {
  if (!alertN_) return;
  switch (alerts_[0].type) {
    case AlertType::Timer: out_.play(Pattern::TimerDone, now); break;
    case AlertType::PomoFocusDone: case AlertType::PomoBreakDone: out_.play(Pattern::PomodoroDone, now); break;
    case AlertType::Reminder: out_.play(Pattern::Reminder, now); break;
    case AlertType::Alarm: out_.play(Pattern::Alarm, now); break;
  }
}

void App::pushAlert(AlertType t, uint8_t idx, ms_t now) {
  if (alertN_ == kAlertQueue) {  // fila limitada: descarta o mais antigo (preserva o atual na tela)
    for (int i = 1; i < alertN_ - 1; i++) alerts_[i] = alerts_[i + 1];
    alertN_--;
  }
  alerts_[alertN_++] = Alert{t, idx, now};
  if (alertN_ == 1) {
    if (screen_ == Screen::ScreenOff) wake(now);
    if (screen_ == Screen::Game && game_ && game_->phase() == GamePhase::Playing && !game_->paused()) {
      game_->pause(now);  // pausa justa; reflexo/tempo certo viram rodada inválida
      alertPausedGame_ = true;
    }
    startAlertOutput(now);
    input_.consumeCurrent(true);
    setInputModeFor(screen_);
  }
  static const char* const names[] = {"timer concluído", "foco concluído", "pausa concluída", "lembrete", "alarme"};
  event(names[(int)t]);
  canvas_.invalidateAll();
  forceFrame_ = true;
}

void App::ackAlert(ms_t now, bool snooze) {
  if (!alertN_) return;
  Alert a = alerts_[0];
  if (a.type == AlertType::Timer) timers_[a.index].acknowledge();
  if (a.type == AlertType::Reminder) reminderDue_[a.index] = now + (snooze ? 10ull * 60000 : (ms_t)set_.reminders[a.index].intervalMin * 60000);
  if (a.type == AlertType::Alarm) {
    if (snooze) alarmSnoozeUntil_[a.index] = now + 5ull * 60000;
    else {
      alarmSnoozeUntil_[a.index] = 0;
      if (set_.alarms[a.index].days == 0) { set_.alarms[a.index].enabled = 0; markSettings(now); }  // alarme único
    }
  }
  for (int i = 0; i + 1 < alertN_; i++) alerts_[i] = alerts_[i + 1];
  alertN_--;
  out_.stopAlerts();
  if (alertN_) startAlertOutput(now);
  setInputModeFor(screen_);
  event(snooze ? "aviso adiado" : "alerta reconhecido");
  canvas_.invalidateAll();
  forceFrame_ = true;
}

void App::onAlertTouch(const TouchEvent& e, ms_t now) {
  if (e.type == TouchEv::Down) { pressedHit_ = hitTest(e.x, e.y); return; }
  if (e.type == TouchEv::Up) { pressedHit_ = -1; return; }
  if (e.type != TouchEv::Tap && e.type != TouchEv::DoubleTap && e.type != TouchEv::LongRelease) return;
  int h = hitTest(e.x, e.y);
  bool canSnooze = alerts_[0].type == AlertType::Reminder || alerts_[0].type == AlertType::Alarm;
  if (h == B_ALERT_SNOOZE && canSnooze) ackAlert(now, true);
  else ackAlert(now, false);
}

// ── tempo ───────────────────────────────────────────────────────────────────
void App::writeCheckpoint(ms_t now) {
  tbMemset(&ckpt_, 0, sizeof ckpt_);
  ckpt_.pomoActive = pomo_.active();
  ckpt_.pomoPhase = (uint8_t)pomo_.phase();
  ckpt_.pomoFocusDone = pomo_.focusDone();
  ckpt_.pomoRemainingMs = pomo_.remainingMs(now);
  for (int i = 0; i < kTimers; i++) {
    RunState s = timers_[i].state();
    ckpt_.timerActive[i] = s == RunState::Running || s == RunState::Paused;
    ckpt_.timerRemainingMs[i] = timers_[i].remainingMs(now);
    ckpt_.timerDurationMs[i] = timers_[i].durationMs();
  }
  ckpt_.swActive = sw_.state() != RunState::Idle;
  ckpt_.swElapsedMs = (uint32_t)tbMin<uint64_t>(sw_.elapsedMs(now), 0xFFFFFFFFu);
  ckpt_.valid = 1;
  ckptDirty_ = true; lastChange_ = now;
}

bool App::checkpointPending() const {
  if (!ckpt_.valid) return false;
  if (ckpt_.pomoActive || ckpt_.swActive) return true;
  for (int i = 0; i < kTimers; i++) if (ckpt_.timerActive[i]) return true;
  return false;
}

void App::resumeFromCheckpoint(ms_t now) {
  Screen dest = Screen::Face;
  if (ckpt_.pomoActive) { pomo_.restore((PomoPhase)ckpt_.pomoPhase, ckpt_.pomoFocusDone, ckpt_.pomoRemainingMs); dest = Screen::Pomodoro; }
  for (int i = 0; i < kTimers; i++) if (ckpt_.timerActive[i] && ckpt_.timerDurationMs[i]) {
    timers_[i].set(ckpt_.timerDurationMs[i]);
    timers_[i].startWithRemaining(now, tbMax<uint32_t>(1, ckpt_.timerRemainingMs[i]));
    timers_[i].pause(now);
    if (dest == Screen::Face) { dest = Screen::Timers; timerSel_ = i; }
  }
  if (ckpt_.swActive) { sw_.restore(ckpt_.swElapsedMs); if (dest == Screen::Face) dest = Screen::Stopwatch; }
  writeCheckpoint(now);
  event("sessão retomada (pausada)");
  gotoScreen(dest, now);
}

bool App::quietNow(ms_t now) const {
  if (!set_.quietEnabled || !clock_.valid()) return false;
  int h = clock_.local(now, TZ_TABLE[set_.tzIndex].offsetMin).hour;
  int a = set_.quietStartH, b = set_.quietEndH;
  return a <= b ? (h >= a && h < b) : (h >= a || h < b);
}

void App::updateTimeServices(ms_t now) {
  Pomodoro::Event pe = pomo_.poll(now);
  if (pe != Pomodoro::Event::None) {
    if (pe == Pomodoro::Event::FocusDone) {
      Progress& Pr = P();
      Pr.pomodorosDone++;
      Pr.mood = (int16_t)tbMin(100, Pr.mood + 4);
      markProgress(now);
      unlock(UNL_FOCADO, now);
      pushAlert(AlertType::PomoFocusDone, 0, now);
    } else {
      pushAlert(AlertType::PomoBreakDone, 0, now);
    }
    writeCheckpoint(now);
  }
  for (int i = 0; i < kTimers; i++) if (timers_[i].poll(now)) { pushAlert(AlertType::Timer, (uint8_t)i, now); writeCheckpoint(now); }
  sw_.poll(now);
  bool focusRunning = pomo_.phase() == PomoPhase::Focus && pomo_.state() == RunState::Running;
  for (int i = 0; i < REMINDER_COUNT; i++) {
    if (!set_.reminders[i].enabled) { reminderPending_[i] = false; continue; }
    if (now >= reminderDue_[i]) {
      reminderDue_[i] = now + (ms_t)set_.reminders[i].intervalMin * 60000;
      if (quietNow(now)) { event("lembrete pulado (descanso)"); continue; }
      if (focusRunning) { reminderPending_[i] = true; continue; }  // respeita o foco
      pushAlert(AlertType::Reminder, (uint8_t)i, now);
    }
    if (reminderPending_[i] && !focusRunning) { reminderPending_[i] = false; pushAlert(AlertType::Reminder, (uint8_t)i, now); }
  }
  updateAlarms(now);
}

// Alarmes: hora civil (exige relógio válido). Cada minuto dispara no máximo uma vez.
void App::updateAlarms(ms_t now) {
  for (int i = 0; i < ALARM_COUNT; i++) {
    if (alarmSnoozeUntil_[i] && now >= alarmSnoozeUntil_[i]) {
      alarmSnoozeUntil_[i] = 0;
      if (set_.alarms[i].enabled) pushAlert(AlertType::Alarm, (uint8_t)i, now);
    }
  }
  if (!clock_.valid()) return;
  int64_t ep = clock_.epoch(now) + (int64_t)TZ_TABLE[set_.tzIndex].offsetMin * 60;
  int64_t minute = ep / 60;
  CivilClock::Local L = civilFromEpoch(ep);
  for (int i = 0; i < ALARM_COUNT; i++) {
    const Alarm& a = set_.alarms[i];
    if (!a.enabled || alarmFiredMinute_[i] == minute) continue;
    if (L.hour != a.hour || L.minute != a.minute) continue;
    if (a.days && !((a.days >> L.weekday) & 1)) continue;
    alarmFiredMinute_[i] = minute;
    pushAlert(AlertType::Alarm, (uint8_t)i, now);
  }
}

int App::nextAlarmIndex(ms_t now, int* minutesAway) const {
  if (!clock_.valid()) return -1;
  int64_t ep = clock_.epoch(now) + (int64_t)TZ_TABLE[set_.tzIndex].offsetMin * 60;
  CivilClock::Local L = civilFromEpoch(ep);
  int best = -1, bestMin = 0x7FFFFFFF;
  int nowMin = L.hour * 60 + L.minute;
  for (int i = 0; i < ALARM_COUNT; i++) {
    const Alarm& a = set_.alarms[i];
    if (!a.enabled) continue;
    for (int d = 0; d < 8; d++) {
      int wd = (L.weekday + d) % 7;
      int m = d * 1440 + a.hour * 60 + a.minute - nowMin;
      if (m <= 0) continue;
      if (a.days && !((a.days >> wd) & 1)) continue;
      if (m < bestMin) { bestMin = m; best = i; }
      break;
    }
  }
  if (minutesAway) *minutesAway = bestMin;
  return best;
}

void App::setTime(int64_t epoch, ms_t now) {
  clock_.set(epoch, now);
  clockFromRtc_ = false;
  if (caps_.rtc) plat_.rtcWrite(epoch);
  int64_t minute = (epoch + (int64_t)TZ_TABLE[set_.tzIndex].offsetMin * 60) / 60;
  for (int i = 0; i < ALARM_COUNT; i++) alarmFiredMinute_[i] = minute;  // ajuste de hora não dispara alarme retroativo
  event("hora ajustada");
}

void App::rtcTime(int64_t epoch, ms_t now) {
  if (epoch < 1577836800 || epoch > 4102444800) return;  // RTC sem hora válida
  clock_.set(epoch, now);
  clockFromRtc_ = true;
  int64_t minute = (epoch + (int64_t)TZ_TABLE[set_.tzIndex].offsetMin * 60) / 60;
  for (int i = 0; i < ALARM_COUNT; i++) alarmFiredMinute_[i] = minute;
  event("hora lida do RTC");
}

// ── personalidade ───────────────────────────────────────────────────────────
Expr App::idleExpression(ms_t now) {
  const Progress& Pr = P();
  if (pomo_.state() == RunState::Running) return pomo_.phase() == PomoPhase::Focus ? Expr::Focus : Expr::Rest;
  if (power_.known && power_.battery && power_.percent >= 0 && power_.percent <= 10 && !power_.charging) return Expr::Tired;
  if (energy_ < 25) return Expr::Sleepy;
  if (clock_.valid()) {
    int h = clock_.local(now, TZ_TABLE[set_.tzIndex].offsetMin).hour;
    if (h >= 23 || h < 6) return Expr::Sleepy;
  }
  if (Pr.mood < -20) return Expr::Neutral;
  Expr fav = (Expr)set_.favoriteExpr;
  int r = rng_.range(0, 99);
  bool playful = set_.personality == (uint8_t)Personality::Brincalhao;
  if (r < (playful ? 50 : 60)) return fav;
  if (r < (playful ? 80 : 85)) return Expr::Happy;
  if (playful && r < 90) return Expr::Curious;
  return Expr::Neutral;
}

void App::updatePersonality(ms_t now) {
  if (now - lastMinuteTick_ >= 60000) {
    lastMinuteTick_ += 60000;
    if (now - lastInteraction_ > 120000) energy_ = tbMax(5, energy_ - 1);
    Progress& Pr = P();
    if (Pr.mood > 20) Pr.mood--; else if (Pr.mood < 20) Pr.mood++;  // deriva lenta, em memória
  }
  // o rosto muda levemente de lugar de tempos em tempos (parece mais vivo)
  if (now >= nextDrift_) { nextDrift_ = now + 120000; face_.setDrift(rng_.range(-3, 3), rng_.range(-3, 3)); }
  // dança: música detectada
  bool dancing = sound_.music() && set_.micReactions && screen_ == Screen::Face;
  face_.setDance(dancing, sound_.beatPhase(now));
  if (dancing) {
    if (!danceSince_) { danceSince_ = now; danceCredit_ = now; react(((P().unlocked >> UNL_DANCANTE) & 1u) ? Expr::Groovy : Expr::Hit, now, 60000); }
    if (now - danceCredit_ >= 1000) {
      danceCredit_ += 1000;
      Progress& Pr = P();
      if (Pr.danceSeconds < 65000) Pr.danceSeconds++;
      if (Pr.danceSeconds >= 30) { if (!((Pr.unlocked >> UNL_DANCANTE) & 1u)) { unlock(UNL_DANCANTE, now); face_.setExpression(Expr::Groovy, now); } }
      if (Pr.danceSeconds % 30 == 0) markProgress(now);
    }
  } else if (danceSince_) {
    danceSince_ = 0;
    if (exprUntil_) exprUntil_ = now;
  }
  if (screen_ != Screen::Face && screen_ != Screen::ColorPick) return;
  // olhar: dedo > inclinação > ocioso
  if (input_.pressed() && !input_.stuck()) {
    face_.setLookTarget(true, (input_.x() - SCR_CX) * 1000 / 100, (input_.y() - (int)FACE_CY) * 1000 / 100);
  } else if (lookUntil_ > now) {
    face_.setLookTarget(true, lookX_, lookY_);
  } else if (set_.tiltLook && caps_.imu && motion_.active() && motion_.pose() != Pose::OnBack) {
    int tx = motion_.tiltX(), ty = -motion_.tiltY();
    face_.setLookTarget(tbAbs(tx) > 120 || tbAbs(ty) > 120, tx, ty);
  } else if (motion_.active() && motion_.pose() == Pose::OnBack) {
    face_.setLookTarget(true, 0, -900);  // deitado: olha para cima
  } else {
    face_.setLookTarget(false, 0, 0);
  }
  face_.setPetting(input_.petting() || (input_.pressed() && !input_.moved() && input_.holdMs(now) >= 600 && input_.holdMs(now) < 2000));
  if (input_.pressed() && !input_.moved() && input_.holdMs(now) >= 2000 && input_.holdMs(now) < 8000) {
    face_.setExpression(Expr::Love, now); exprUntil_ = now + 400;
  }
  if (exprUntil_ && now >= exprUntil_) { exprUntil_ = 0; face_.setExpression(baseExpr_, now); }
  if (!exprUntil_ && !input_.pressed() && now >= nextIdleExpr_) {
    int f = set_.animLevel == 0 ? 160 : (set_.animLevel == 2 ? 70 : 100);
    nextIdleExpr_ = now + (set_.personality ? 45000ull : 90000ull) * f / 100;
    baseExpr_ = idleExpression(now);
    face_.setExpression(baseExpr_, now);
  }
}

// Brilho efetivo: preferência, período de descanso, lanterna e tela apagada.
void App::updateBrightness(ms_t now) {
  uint8_t b = set_.brightness;
  if (set_.nightDim && quietNow(now)) b = (uint8_t)tbMax(12, b / 4);
  if (screen_ == Screen::Torch) b = 255;
  if (screen_ == Screen::ScreenOff) b = 0;
  if (b != appliedBright_ || (bool)set_.rotate180 != appliedRotate_) {
    appliedBright_ = b; appliedRotate_ = set_.rotate180;
    plat_.applyDisplay(b, set_.rotate180);
  }
}

// ── jogos ───────────────────────────────────────────────────────────────────
void App::startGame(GameId g, ms_t now) {
  Progress& Pr = P();
  gameId_ = g;
  switch (g) {
    case GameId::Jump: game_ = &jump_; jump_.best = Pr.jumpBest; break;
    case GameId::Reflex: game_ = &reflex_; reflex_.best = Pr.reflexBestMs; break;
    case GameId::Fish: game_ = &fish_; fish_.best = Pr.fishBest; break;
    case GameId::Dive: game_ = &dive_; dive_.best = Pr.diveBest; break;
    default: game_ = &timing_; timing_.best = Pr.timingBestPct100; timing_.bestAux = Pr.timingBestErrMs; break;
  }
  game_->newRecord = false;
  game_->begin(now, &rng_, set_.jumpEasy);
  screen_ = Screen::Game; screenAt_ = now; forceFrame_ = true;
  canvas_.invalidateAll();
  setInputModeFor(screen_);
  event(gameTitle(g));
}

void App::onGameOutcome(const GameOutcome& o, ms_t now) {
  if (!o.finished || !game_) return;
  game_->newRecord = false;
  if (!o.valid) return;
  Progress& Pr = P();
  bool rec = false;
  Pr.gamesPlayed++;
  Pr.mood = (int16_t)tbMin(100, Pr.mood + 2);
  switch (gameId_) {
    case GameId::Jump:
      if (o.score > Pr.jumpBest) { Pr.jumpBest = (uint16_t)o.score; rec = true; }
      if (o.score >= 10) unlock(UNL_VITORIOSO, now);
      game_->best = Pr.jumpBest; break;
    case GameId::Reflex:
      Pr.reflexValid++;
      if (Pr.reflexBestMs == 0 || o.score < Pr.reflexBestMs) { Pr.reflexBestMs = (uint16_t)tbMin<int32_t>(o.score, 65535); rec = true; }
      if (Pr.reflexValid >= 5) unlock(UNL_ESPANTADO, now);
      game_->best = Pr.reflexBestMs; break;
    case GameId::Fish:
      if (o.score > Pr.fishBest) { Pr.fishBest = (uint16_t)o.score; rec = o.score > 0; }
      if (o.score >= 5) unlock(UNL_SATISFEITO, now);
      game_->best = Pr.fishBest; break;
    case GameId::Dive:
      if (o.score > Pr.diveBest) { Pr.diveBest = (uint16_t)o.score; rec = o.score > 0; }
      if (o.score >= 20) unlock(UNL_MERGULHADOR, now);
      game_->best = Pr.diveBest; break;
    default:
      if (o.score > Pr.timingBestPct100 || (o.score == Pr.timingBestPct100 && o.score > 0 && o.aux < Pr.timingBestErrMs)) {
        Pr.timingBestPct100 = (uint16_t)o.score; Pr.timingBestErrMs = (uint16_t)tbMin<int32_t>(o.aux, 65535);
        Pr.timingBestTargetCs = (uint16_t)o.aux2; rec = true;
      }
      if (o.score >= 9800) unlock(UNL_PRECISAO, now);
      game_->best = Pr.timingBestPct100; break;
  }
  game_->newRecord = rec;
  if (rec) out_.play(Pattern::Record, now);
  markProgress(now);
}

// ── respiração e sorteio ────────────────────────────────────────────────────
void App::startBreath(ms_t now) { breathOn_ = true; breathStart_ = now; breathPhaseLast_ = -1; event("respiração"); }

void App::rollDice(ms_t now) {
  if (rollUntil_ > now) return;
  rollUntil_ = now + 750;
  switch (diceMode_) {
    case 0: diceResult_ = rng_.range(1, 6); break;
    case 1: case 2: diceResult_ = rng_.range(0, 1); break;
    default: diceResult_ = rng_.range(1, 100); break;
  }
  out_.play(Pattern::Babble, now, rng_.next());
}

// ── listas e grades ─────────────────────────────────────────────────────────
void App::buildList(ms_t now, bool keepScroll) {
  (void)now;
  itemN_ = 0;
  using ui::RowKind;
  auto add = [&](const char* label, const char* sub, Icon ic, Color col, int id, RowKind kind = RowKind::Link, bool on = false) {
    if (itemN_ >= kListMax) return;
    tbStrlcpy(itemLabels_[itemN_], label, sizeof itemLabels_[0]);
    itemSubs_[itemN_][0] = 0;
    if (sub) tbStrlcpy(itemSubs_[itemN_], sub, sizeof itemSubs_[0]);
    items_[itemN_] = ui::ListItem{itemLabels_[itemN_], sub ? itemSubs_[itemN_] : nullptr, ic, col, id, kind, on};
    itemN_++;
  };
  char b[40];
  int keep = list_.scrollPx();
  static const char* const ONOFF[] = {"desligado", "ligado"};
  (void)ONOFF;
  switch (screen_) {
    case Screen::Launcher: {
      // menu principal: 3×3, na ordem pedida
      add("Jogos", nullptr, Icon::Games, ui::C_GAMES, L_GAMES);
      add("Pomodoro", nullptr, Icon::Tomato, ui::C_POMO, L_POMO);
      add("Timer", nullptr, Icon::Timer, ui::C_TIMER, L_TIMERS);
      add("Aparência", nullptr, Icon::Face, exprColorRgb(set_.eyeColor, false), L_LOOK);
      StrBuf v(b, sizeof b); if (caps_.speaker) v.num(set_.volume).put('%');
      add("Volume", caps_.speaker ? b : nullptr, set_.silent ? Icon::Silent : Icon::Speaker, ui::C_VOL, L_VOLUME);
      char c[8]; StrBuf br(c, sizeof c); br.num((set_.brightness * 100 + 127) / 255).put('%');
      add("Brilho", c, Icon::Brightness, ui::C_BRIGHT, L_BRIGHT);
      add("Wi-Fi", nullptr, Icon::Wifi, ui::C_WIFI, L_WIFI);
      add("Sistema", nullptr, Icon::Gear, ui::C_SET, L_SYSTEM);
      add("Mais apps", nullptr, Icon::Plus, ui::C_INFO, L_MORE);
      break;
    }
    case Screen::MoreApps:
      add("Cronômetro", nullptr, Icon::Stopwatch, ui::C_SW, L_SW);
      add("Alarmes", nullptr, Icon::Alarm, ui::C_ALARM, L_ALARMS);
      add("Relógio", nullptr, Icon::Clock, ui::C_CLOCK, L_CLOCK);
      add("Clima", nullptr, Icon::Weather, ui::C_WEATHER, L_WEATHER);
      add("Respirar", nullptr, Icon::Breath, ui::C_BREATH, L_BREATH);
      add("Sorteio", nullptr, Icon::Dice, ui::C_DICE, L_DICE);
      add("Expressões", nullptr, Icon::Sparkle, ui::C_EXPR, L_EXPR);
      add("Meu Tuba", nullptr, Icon::Heart, ui::C_STATS, L_STATS);
      if (caps_.imu) add("Nível", nullptr, Icon::Level, ui::C_LEVEL, L_LEVEL);
      else add("Sobre", nullptr, Icon::Info, ui::C_INFO, L_INFO);
      break;
    case Screen::GamesMenu: {
      const Progress& Pr = P();
      static const Icon GI[5] = {Icon::Shark, Icon::Bolt, Icon::Fish, Icon::Stopwatch, Icon::Level};
      static const Color GC[5] = {0x5CC8FF, 0xFFD54A, 0xFF9F43, 0x4BF0A6, 0x3EE6D0};
      for (int g = 0; g < (int)GameId::Count; g++) {
        StrBuf s(b, sizeof b); s.put("Recorde: ");
        switch ((GameId)g) {
          case GameId::Jump: s.num(Pr.jumpBest); break;
          case GameId::Reflex: if (Pr.reflexBestMs) s.num(Pr.reflexBestMs).put(" ms"); else s.put("--"); break;
          case GameId::Fish: s.num(Pr.fishBest).put(" peixes"); break;
          case GameId::Timing: if (Pr.timingBestPct100) s.fixed2(Pr.timingBestPct100).put('%'); else s.put("--"); break;
          default: s.num(Pr.diveBest).put(" peixes"); break;
        }
        add(gameTitle((GameId)g), b, GI[g], GC[g], L_GAME0 + g, RowKind::Action);
      }
      break;
    }
    case Screen::TimerSet:
      for (int i = 0; i < TIMER_PRESET_COUNT; i++) {
        StrBuf s(b, sizeof b); formatDuration(s, (ms_t)set_.timerPresetS[i] * 1000, false);
        add(b, i == TIMER_PRESET_COUNT - 1 ? "personalizado no portal" : nullptr, Icon::Timer, ui::C_TIMER, L_PRESET0 + i, RowKind::Action);
      }
      break;
    case Screen::Appearance: {
      add("Tema", set_.theme ? "Claro" : "Escuro", set_.theme ? Icon::Sun : Icon::Moon, ui::C_LOOK, S_THEME, RowKind::Value);
      add("Cor das expressões", EXPR_COLORS[set_.eyeColor].name, Icon::Palette, exprColorRgb(set_.eyeColor, false), S_EYE, RowKind::Link);
      int n = set_.showClock + set_.showWeather + (caps_.battery ? set_.showBattery : 0);
      StrBuf o(b, sizeof b); if (!n) o.put("rosto limpo"); else o.num(n).put(n == 1 ? " item" : " itens");
      add("Informações na tela", b, Icon::Info, ui::C_INFO, S_OVERLAYS, RowKind::Link);
      add("Brilho dos olhos", nullptr, Icon::Sparkle, ui::C_EXPR, S_GLOW, RowKind::Toggle, set_.eyeGlow);
      static const char* const WF[] = {"Digital", "Analógico", "Tuba"};
      add("Relógio", WF[set_.watchFace], Icon::Clock, ui::C_CLOCK, S_FACE, RowKind::Value);
      break;
    }
    case Screen::Overlays:
      add("Mostrar hora", nullptr, Icon::Clock, ui::C_CLOCK, S_SHOW_CLOCK, RowKind::Toggle, set_.showClock);
      add("Mostrar clima", nullptr, Icon::PartCloud, ui::C_WEATHER, S_SHOW_WEATHER, RowKind::Toggle, set_.showWeather);
      if (caps_.battery) add("Mostrar bateria", nullptr, Icon::Bolt, ui::C_VOL, S_SHOW_BATT, RowKind::Toggle, set_.showBattery);
      break;
    case Screen::Wifi: {
      NetStatus ns; plat_.netStatus(ns);
      const char* home = !ns.staConfigured ? "não configurada" : ns.sta == StaState::Connected ? ns.staSsid : ns.sta == StaState::Connecting ? "conectando..." : "sem conexão";
      add("Rede de casa", home, Icon::Wifi, ui::C_WIFI, S_WIFI_HOME, RowKind::Value);
      add("Configurar pelo celular", "QR Code + portal", Icon::Info, ui::C_INFO, S_WIFI_SETUP, RowKind::Link);
      add("Rede do Tuba", nullptr, Icon::Wifi, ui::C_FACE, S_WIFI_AP, RowKind::Toggle, ns.apActive);
      if (ns.staConfigured) add("Esquecer rede de casa", nullptr, Icon::Cross, ui::BAD, S_WIFI_FORGET, RowKind::Action);
      break;
    }
    case Screen::System: {
      add("Personalidade", set_.personality ? "Brincalhão" : "Tranquilo", Icon::Heart, ui::C_STATS, S_PERS, RowKind::Value);
      static const char* const AN[] = {"Calma", "Normal", "Animada"};
      add("Animação", AN[set_.animLevel], Icon::Sparkle, ui::C_EXPR, S_ANIM, RowKind::Value);
      StrBuf o(b, sizeof b); if (!set_.screenOffS) o.put("nunca"); else formatDuration(o, (ms_t)set_.screenOffS * 1000, false);
      add("Apagar a tela", b, Icon::Moon, ui::C_SET, S_SCREENOFF, RowKind::Value);
      add("Formato 24 h", nullptr, Icon::Clock, ui::C_CLOCK, S_24H, RowKind::Toggle, set_.clock24h);
      add("Silencioso", nullptr, Icon::Silent, ui::C_VOL, S_SILENT, RowKind::Toggle, set_.silent);
      if (caps_.mic) add("Reagir a sons", nullptr, Icon::Note, ui::C_GAMES, S_MIC, RowKind::Toggle, set_.micReactions);
      if (caps_.imu) add("Reagir a movimento", nullptr, Icon::Level, ui::C_LEVEL, S_IMU, RowKind::Toggle, set_.imuReactions);
      add("Girar 180°", nullptr, Icon::Reset, ui::C_SET, S_ROTATE, RowKind::Toggle, set_.rotate180);
      add("Informações", TB_VERSION, Icon::Info, ui::C_INFO, S_INFO, RowKind::Link);
      add("Restaurar padrão de fábrica", nullptr, Icon::Reset, ui::BAD, S_FACTORY_RESET, RowKind::Action);
      break;
    }
    default: break;
  }
  list_.reset(itemN_, keepScroll ? keep : 0);
}

void App::cycleSetting(int id, ms_t now) {
  switch (id) {
    case S_BRIGHT: { static const uint8_t L[] = {64, 128, 191, 255}; int i = 0; while (i < 4 && L[i] <= set_.brightness) i++; set_.brightness = L[i % 4]; break; }
    case S_VOLUME: { static const uint8_t L[] = {0, 25, 50, 75, 100}; int i = 0; while (i < 5 && L[i] <= set_.volume) i++; set_.volume = L[i % 5]; break; }
    case S_EYE: gotoScreen(Screen::ColorPick, now); return;
    case S_OVERLAYS: gotoScreen(Screen::Overlays, now); return;
    case S_THEME: set_.theme = !set_.theme; break;
    case S_GLOW: set_.eyeGlow = !set_.eyeGlow; break;
    case S_FACE: set_.watchFace = (uint8_t)((set_.watchFace + 1) % WATCHFACE_COUNT); break;
    case S_SHOW_CLOCK: set_.showClock = !set_.showClock; break;
    case S_SHOW_WEATHER: set_.showWeather = !set_.showWeather; if (set_.showWeather && !set_.weatherEnabled) toast("Ative o clima no portal", now, 2500); break;
    case S_SHOW_BATT: if (!caps_.battery) return; set_.showBattery = !set_.showBattery; break;
    case S_PERS: set_.personality = set_.personality ? 0 : 1; break;
    case S_ANIM: set_.animLevel = (uint8_t)((set_.animLevel + 1) % 3); break;
    case S_24H: set_.clock24h = !set_.clock24h; break;
    case S_SCREENOFF: { static const uint16_t L[] = {0, 60, 300, 900}; int i = 0; while (i < 4 && L[i] <= set_.screenOffS) i++; set_.screenOffS = L[i % 4]; break; }
    case S_IMU: set_.imuReactions = !set_.imuReactions; break;
    case S_MIC: set_.micReactions = !set_.micReactions; break;
    case S_VIB: set_.vibration = !set_.vibration; break;
    case S_SILENT: set_.silent = !set_.silent; break;
    case S_ROTATE: set_.rotate180 = !set_.rotate180; break;
    case S_NIGHTDIM: set_.nightDim = !set_.nightDim; break;
    case S_NET: case S_WIFI_SETUP: openConfig(now); return;
    case S_WIFI_HOME: openConfig(now); return;
    case S_WIFI_AP: {
      NetStatus ns; plat_.netStatus(ns);
      if (ns.apActive) { plat_.apClose(); toast("Rede do Tuba fechada", now); event("rede fechada"); }
      else { plat_.apOpen(); toast("Rede do Tuba aberta", now); event("rede aberta"); }
      out_.play(Pattern::Tap, now);
      buildList(now, true);
      return;
    }
    case S_WIFI_FORGET: confirm(C_WIFI_FORGET, now); return;
    case S_FACTORY_RESET: confirm(C_FACTORY_RESET, now); return;
    case S_INFO: infoPage_ = 0; returnScreen_ = Screen::System; gotoScreen(Screen::Info, now); return;
    default: return;
  }
  sanitizeSettings(set_);
  applyPrefs(now);
  applyTheme();
  markSettings(now);
  out_.play(id == S_VOLUME ? Pattern::Boop : Pattern::Tap, now);
  buildList(now, true);
}

void App::confirm(int action, ms_t now) {
  confirmAction_ = action;
  returnScreen_ = screen_;
  gotoScreen(Screen::Confirm, now);
}

// ── gestos e botões ─────────────────────────────────────────────────────────
void App::onListPick(int id, ms_t now) {
  out_.play(Pattern::Tap, now);
  transDir_ = -1;
  if (id >= L_GAME0 && id < L_GAME0 + (int)GameId::Count) { startGame((GameId)(id - L_GAME0), now); return; }
  if (id >= L_PRESET0 && id < L_PRESET0 + TIMER_PRESET_COUNT) {
    Countdown& t = timers_[timerSel_];
    t.cancel(); t.set(set_.timerPresetS[id - L_PRESET0] * 1000);
    writeCheckpoint(now);
    gotoScreen(Screen::Timers, now);
    return;
  }
  if (id >= S_BRIGHT && id < S_LAST) { cycleSetting(id, now); return; }
  switch (id) {
    case L_CLOCK: goHome(now, Screen::Clock); break;
    case L_POMO: gotoScreen(Screen::Pomodoro, now); break;
    case L_TIMERS: gotoScreen(Screen::Timers, now); break;
    case L_SW: gotoScreen(Screen::Stopwatch, now); break;
    case L_ALARMS: gotoScreen(Screen::Alarms, now); break;
    case L_GAMES: gotoScreen(Screen::GamesMenu, now); break;
    case L_EXPR: gotoScreen(Screen::Expressions, now); break;
    case L_BREATH: breathOn_ = false; gotoScreen(Screen::Breath, now); break;
    case L_LEVEL: gotoScreen(Screen::Level, now); break;
    case L_DICE: gotoScreen(Screen::Dice, now); break;
    case L_WEATHER: gotoScreen(Screen::Weather, now); break;
    case L_STATS: gotoScreen(Screen::Stats, now); break;
    case L_SYSTEM: gotoScreen(Screen::System, now); break;
    case L_INFO: infoPage_ = 0; returnScreen_ = Screen::MoreApps; gotoScreen(Screen::Info, now); break;
    case L_LOOK: gotoScreen(Screen::Appearance, now); break;
    case L_VOLUME: gotoScreen(Screen::Volume, now); break;
    case L_BRIGHT: gotoScreen(Screen::Brightness, now); break;
    case L_WIFI: gotoScreen(Screen::Wifi, now); break;
    case L_MORE: gotoScreen(Screen::MoreApps, now); break;
  }
}

// Controles deslizantes (Volume, Brilho e painel rápido): x na tela -> valor.
void App::setSlider(int id, int x, bool commit, ms_t now) {
  int pm = tbClamp((int)(((float)x - ui::SLIDER_X0) * 1000.f / ui::SLIDER_W), 0, 1000);
  if (id == B_SL_BRIGHT || id == B_Q_BRIGHT) set_.brightness = (uint8_t)tbClamp(10 + pm * 245 / 1000, 10, 255);
  else set_.volume = (uint8_t)((pm + 5) / 10);
  applyPrefs(now);
  if (commit) {
    markSettings(now);
    if (id == B_SL_VOL || id == B_Q_VOL) out_.play(Pattern::Boop, now);
  }
}

static int galleryCount() { return (int)Expr::Dizzy; }  // expressões selecionáveis/colecionáveis

void App::onButton(int id, ms_t now) {
  if (id == B_BACK) { out_.play(Pattern::Menu, now); goBack(now); return; }
  forceFrame_ = true;
  switch (screen_) {
    case Screen::Face:
      dockUntil_ = 0;
      out_.play(Pattern::Tap, now);
      transDir_ = 0;
      if (id == B_DOCK_MENU) gotoScreen(Screen::Launcher, now);
      else if (id == B_DOCK_POMO) gotoScreen(Screen::Pomodoro, now);
      else if (id == B_DOCK_GAMES) gotoScreen(Screen::GamesMenu, now);
      break;
    case Screen::ColorPick:
      if (id >= B_COLOR0 && id < B_COLOR0 + EXPR_COLOR_COUNT) {
        set_.eyeColor = (uint8_t)(id - B_COLOR0);
        applyTheme(); markSettings(now);
        face_.setExpression(Expr::Happy, now); exprUntil_ = 0;
        out_.play(Pattern::Boop, now);
        event("cor das expressões");
      }
      break;
    case Screen::Volume: case Screen::Brightness: {
      bool vol = screen_ == Screen::Volume;
      if (id == B_SL_MINUS || id == B_SL_PLUS) {
        int d = id == B_SL_PLUS ? 1 : -1;
        if (vol) set_.volume = (uint8_t)tbClamp((int)set_.volume + 10 * d, 0, 100);
        else set_.brightness = (uint8_t)tbClamp((int)set_.brightness + 25 * d, 10, 255);
        applyPrefs(now); markSettings(now);
        out_.play(vol ? Pattern::Boop : Pattern::Tap, now);
      } else if (id == B_SL_TEST) {
        if (vol) { set_.silent = !set_.silent; applyPrefs(now); markSettings(now); out_.play(Pattern::Boop, now); }
        else { set_.nightDim = !set_.nightDim; markSettings(now); updateBrightness(now); out_.play(Pattern::Tap, now); }
      }
      break;
    }
    case Screen::Pomodoro:
      if (id == B_PLAY) { pomo_.toggle(now); writeCheckpoint(now); out_.play(Pattern::Tap, now); event("pomodoro"); }
      else if (id == B_SKIP) { pomo_.skip(now); writeCheckpoint(now); out_.play(Pattern::Tap, now); event("etapa pulada"); }
      else if (id == B_RESET && pomo_.active()) { confirmAction_ = C_CANCEL_POMO; returnScreen_ = screen_; gotoScreen(Screen::Confirm, now); }
      break;
    case Screen::Timers: {
      Countdown& t = timers_[timerSel_];
      if (id >= B_TIMER_SEL0 && id <= B_TIMER_SEL2) { timerSel_ = id - B_TIMER_SEL0; out_.play(Pattern::Menu, now); }
      else if (id == B_PLAY) {
        if (t.state() == RunState::Idle || t.state() == RunState::Done) { t.acknowledge(); t.start(now); }
        else if (t.state() == RunState::Running) t.pause(now);
        else if (t.state() == RunState::Paused) t.resume(now);
        writeCheckpoint(now); out_.play(Pattern::Tap, now);
      } else if (id == B_TIMER_DUR) { gotoScreen(Screen::TimerSet, now); list_.scrollTo(1); }
      else if (id == B_TIMER_REPEAT) { t.cancel(); t.start(now); writeCheckpoint(now); out_.play(Pattern::Tap, now); }
      else if (id == B_TIMER_CANCEL) { t.cancel(); writeCheckpoint(now); out_.play(Pattern::Tap, now); }
      break;
    }
    case Screen::Stopwatch:
      if (id == B_PLAY) { sw_.toggle(now); writeCheckpoint(now); out_.play(Pattern::Tap, now); }
      else if (id == B_LAP && sw_.state() == RunState::Running) {
        if (lapN_ == kLaps) { for (int i = 1; i < kLaps; i++) laps_[i - 1] = laps_[i]; lapN_--; }
        laps_[lapN_++] = sw_.elapsedMs(now); out_.play(Pattern::Tap, now);
      } else if (id == B_RESET && sw_.elapsedMs(now) > 0) {
        if (sw_.state() == RunState::Running) { sw_.pause(now); writeCheckpoint(now); }
        confirmAction_ = C_SW_RESET; returnScreen_ = screen_; gotoScreen(Screen::Confirm, now);
      }
      break;
    case Screen::Alarms:
      if (id >= B_ALARM_TOGGLE0 && id <= B_ALARM_TOGGLE2) {
        Alarm& a = set_.alarms[id - B_ALARM_TOGGLE0];
        a.enabled = !a.enabled; markSettings(now); out_.play(Pattern::Tap, now);
        if (a.enabled && !clock_.valid()) toast("Ajuste a hora primeiro", now, 2500);
      } else if (id >= B_ALARM_OPEN0 && id <= B_ALARM_OPEN2) { editAlarm_ = id - B_ALARM_OPEN0; gotoScreen(Screen::AlarmEdit, now); }
      break;
    case Screen::AlarmEdit: {
      Alarm& a = set_.alarms[editAlarm_];
      if (id == B_AL_H_MINUS) a.hour = (uint8_t)((a.hour + 23) % 24);
      else if (id == B_AL_H_PLUS) a.hour = (uint8_t)((a.hour + 1) % 24);
      else if (id == B_AL_M_MINUS) a.minute = (uint8_t)((a.minute + 55) % 60 / 5 * 5);
      else if (id == B_AL_M_PLUS) a.minute = (uint8_t)((a.minute / 5 * 5 + 5) % 60);
      else if (id >= B_AL_DAY0 && id < B_AL_DAY0 + 7) a.days ^= (uint8_t)(1u << (id - B_AL_DAY0));
      else if (id == B_AL_ENABLE) a.enabled = !a.enabled;
      else if (id == B_AL_DONE) { a.enabled = 1; markSettings(now); toast("Alarme salvo", now); gotoScreen(Screen::Alarms, now); return; }
      markSettings(now); out_.play(Pattern::Menu, now);
      break;
    }
    case Screen::Expressions: {
      int n = galleryCount();
      if (id == B_EXPR_PREV) { exprSel_ = (exprSel_ + n - 1) % n; if ((Expr)exprSel_ == Expr::Blink) exprSel_ = (exprSel_ + n - 1) % n; transDir_ = 1; }
      else if (id == B_EXPR_NEXT) { exprSel_ = (exprSel_ + 1) % n; if ((Expr)exprSel_ == Expr::Blink) exprSel_ = (exprSel_ + 1) % n; transDir_ = -1; }
      else if (id == B_EXPR_USE) {
        Expr e = (Expr)exprSel_;
        int uid;
        if (exprIsUnlockable(e, &uid) && !((P().unlocked >> uid) & 1u)) { toast("Ainda bloqueada", now); break; }
        baseExpr_ = e; exprUntil_ = 0; nextIdleExpr_ = now + 120000;
        out_.play(Pattern::Love, now);
        goHome(now, Screen::Face);
        return;
      }
      screenAt_ = now;
      break;
    }
    case Screen::Breath:
      if (id == B_BREATH_PLAY) { if (breathOn_) breathOn_ = false; else startBreath(now); out_.play(Pattern::Tap, now); }
      else if (id == B_BREATH_MODE && !breathOn_) { breathMode_ = (uint8_t)((breathMode_ + 1) % 3); out_.play(Pattern::Menu, now); }
      break;
    case Screen::Dice:
      if (id == B_DICE_MODE) { diceMode_ = (uint8_t)((diceMode_ + 1) % 4); diceResult_ = 0; out_.play(Pattern::Menu, now); }
      else if (id == B_DICE_ROLL) rollDice(now);
      break;
    case Screen::Weather:
      if (id == B_WEATHER_REFRESH) { weatherTryAt_ = 0; toast("Atualizando...", now); }
      break;
    case Screen::Quick:
      if (id == B_Q_SILENT) { set_.silent = !set_.silent; applyPrefs(now); markSettings(now); toast(set_.silent ? "Silencioso: ligado" : "Silencioso: desligado", now); }
      else if (id == B_Q_THEME) { set_.theme = !set_.theme; applyTheme(); markSettings(now); toast(set_.theme ? "Tema claro" : "Tema escuro", now); }
      else if (id == B_Q_MIC) { set_.micReactions = !set_.micReactions; applyPrefs(now); markSettings(now); toast(set_.micReactions ? "Reagir a sons: ligado" : "Microfone ignorado", now); }
      else if (id == B_Q_TORCH) { returnScreen_ = homePage_; gotoScreen(Screen::Torch, now); updateBrightness(now); }
      else if (id == B_Q_NET) gotoScreen(Screen::Wifi, now);
      break;
    case Screen::Info:
      if (id == B_PAGE_NEXT) { infoPage_ = (infoPage_ + 1) % 6; transDir_ = -1; screenAt_ = now; }
      break;
    case Screen::Config: case Screen::FirstUse:
      if (id == B_PAGE_NEXT) { configPage_ = (configPage_ + 1) % 3; transDir_ = -1; screenAt_ = now; }
      else if (id == B_FIRST_OFFLINE) { set_.setupDone = 1; markSettings(now); plat_.apClose(); event("usar offline"); goHome(now); }
      else if (id == B_CFG_CLOSE) { plat_.apClose(); toast("Rede do Tuba fechada", now); event("rede fechada"); gotoScreen(Screen::Wifi, now); }
      break;
    case Screen::Confirm:
      if (id == B_OK) {
        if (confirmAction_ == C_FACTORY_RESET) { confirmAction_ = C_NONE; factoryReset(now); return; }
        if (confirmAction_ == C_WIFI_FORGET) { forgetWifi(); toast("Rede de casa esquecida", now); }
        if (confirmAction_ == C_CANCEL_POMO) { pomo_.cancel(); event("pomodoro cancelado"); }
        if (confirmAction_ == C_SW_RESET) { sw_.reset(); lapN_ = 0; event("cronômetro zerado"); }
        writeCheckpoint(now);
      }
      if (id == B_OK || id == B_CANCEL) { confirmAction_ = C_NONE; gotoScreen(returnScreen_, now); }
      break;
    case Screen::RecoveryConfirm:
      if (id == B_REC_YES) openConfig(now);
      else if (id == B_REC_NO) gotoScreen(returnScreen_ == Screen::RecoveryConfirm ? homePage_ : returnScreen_, now);
      break;
    case Screen::Resume:
      if (id == B_RESUME_YES) resumeFromCheckpoint(now);
      else if (id == B_RESUME_NO) { tbMemset(&ckpt_, 0, sizeof ckpt_); ckptDirty_ = true; lastChange_ = now; event("sessão descartada"); goHome(now); }
      break;
    default: break;
  }
}

void App::onFaceTouch(const TouchEvent& e, ms_t now) {
  switch (e.type) {
    case TouchEv::Tap: {
      // "boop": o Tuba reage ao toque e olha para onde foi tocado
      Progress& Pr = P();
      Pr.boops++;
      if (Pr.boops % 20 == 0) markProgress(now);
      face_.bounce(now);
      dockUntil_ = now + 4000;   // controles aparecem e somem sozinhos
      lookX_ = (e.x - SCR_CX) * 1000 / 100; lookY_ = (e.y - (int)FACE_CY) * 1000 / 100; lookUntil_ = now + 900;
      bool playful = set_.personality == (uint8_t)Personality::Brincalhao;
      int r = rng_.range(0, 99);
      if (r < (playful ? 40 : 25)) react(Expr::Hit, now, 900, Pattern::Giggle);
      else if (r < (playful ? 65 : 55)) react(rng_.range(0, 1) ? Expr::WinkL : Expr::WinkR, now, 800, Pattern::Boop);
      else if (r < 80) react(Expr::Surprised, now, 700, Pattern::Boop);
      else react(Expr::Curious, now, 900, Pattern::Babble);
      break;
    }
    case TouchEv::DoubleTap: {
      Expr list[40]; int n = availableExprs(list, 40);
      int idx = 0;
      for (int i = 0; i < n; i++) if (list[i] == baseExpr_) { idx = i; break; }
      baseExpr_ = list[(idx + 1) % n];
      exprUntil_ = 0;
      face_.setExpression(baseExpr_, now);
      out_.play(Pattern::Double, now);
      break;
    }
    case TouchEv::PetStroke:
      if (!out_.soundActive(now) || out_.current() != Pattern::Purr) out_.play(Pattern::Purr, now);
      break;
    case TouchEv::Pet:
      react(Expr::Pet, now, 2500);
      addPet(now);
      event("carinho");
      break;
    case TouchEv::LongPress:
      out_.play(Pattern::Purr, now);
      break;
    case TouchEv::LongRelease:
      if (e.hold >= 2000) { react(Expr::Love, now, 3000, Pattern::Love); addPet(now); event("amor"); }
      else { react(Expr::Pet, now, 2500, Pattern::Purr); addPet(now); event("carinho"); }
      break;
    default: break;
  }
}

void App::openConfig(ms_t now) {
  plat_.apOpen();  // reabre a rede própria mesmo sem roteador / senha antiga
  configPage_ = 0;
  event("configuração aberta");
  gotoScreen(set_.setupDone ? Screen::Config : Screen::FirstUse, now);
}

void App::wake(ms_t now) {
  Screen back = returnScreen_;
  screen_ = (back == Screen::ScreenOff || back == Screen::Torch) ? homePage_ : back;
  if (screen_ == Screen::Game && !game_) screen_ = Screen::GamesMenu;
  screenAt_ = now; forceFrame_ = true; wakeGuard_ = now + 600;
  canvas_.invalidateAll();
  noteInteraction(now);
  setInputModeFor(screen_);
  updateBrightness(now);
}

void App::sleepScreen(ms_t now) {
  if (screen_ == Screen::ScreenOff || screen_ == Screen::Ota) return;
  if (screen_ == Screen::Game && game_ && game_->phase() == GamePhase::Playing && !game_->paused()) game_->pause(now);
  returnScreen_ = screen_; screen_ = Screen::ScreenOff; screenAt_ = now; forceFrame_ = true;
  event("visor em descanso");
  updateBrightness(now);
}

void App::touchSample(bool down, int x, int y, ms_t t) {
  if (set_.rotate180) { x = SCR_W - 1 - x; y = SCR_H - 1 - y; }
  input_.sample(down, tbClamp(x, 0, SCR_W - 1), tbClamp(y, 0, SCR_H - 1), t);
}

int App::hitTest(int x, int y) const {
  for (int i = hitN_ - 1; i >= 0; i--) {
    const Hit& h = hits_[i];
    if (h.circle) {
      int cx = (h.x0 + h.x1) / 2, cy = (h.y0 + h.y1) / 2, r = (h.x1 - h.x0) / 2;
      int dx = x - cx, dy = y - cy;
      if (dx * dx + dy * dy <= r * r) return h.id;
    } else if (x >= h.x0 && x < h.x1 && y >= h.y0 && y < h.y1) return h.id;
  }
  return -1;
}

bool App::buttonPos(int id, int& x, int& y) const {
  for (int i = 0; i < hitN_; i++) if (hits_[i].id == id) { x = (hits_[i].x0 + hits_[i].x1) / 2; y = (hits_[i].y0 + hits_[i].y1) / 2; return true; }
  return false;
}
int App::listIndexOf(int id) const { for (int i = 0; i < itemN_; i++) if (items_[i].id == id) return i; return -1; }
bool App::listItemPos(int id, int& x, int& y) {
  int i = listIndexOf(id);
  if (i < 0) return false;
  if (gridScreen(screen_)) { x = (int)ui::GridGeom::cx(i); y = (int)ui::GridGeom::cy(i); return true; }
  list_.scrollTo(i);
  list_.rowCenter(i, x, y);
  return true;
}

void App::handleTouch(const TouchEvent& e, ms_t now) {
  if (e.type == TouchEv::StuckDetected) { event("toque preso?"); toast("Toque contínuo: limpe a tela", now, 3000); return; }
  if (e.type == TouchEv::Down) noteInteraction(now);
  if (screen_ == Screen::Ota) return;
  if (screen_ == Screen::ScreenOff) { if (e.type == TouchEv::RecoveryArmed) wake(now); else return; }
  // logo depois de acordar, toques rápidos (ex.: duplo toque para acordar) não viram ação
  if (now < wakeGuard_ && (e.type == TouchEv::Tap || e.type == TouchEv::DoubleTap)) return;
  forceFrame_ = true;
  if (e.type == TouchEv::RecoveryArmed) {
    if (screen_ != Screen::RecoveryConfirm) returnScreen_ = screen_ == Screen::Game ? Screen::GamesMenu : screen_;
    if (screen_ == Screen::Game) game_ = nullptr;
    screen_ = Screen::RecoveryConfirm; screenAt_ = now; recoveryUntil_ = now + 6000;
    canvas_.invalidateAll();
    setInputModeFor(screen_);
    return;
  }
  if (alertN_ && input_.mode() != TouchMode::Game) { onAlertTouch(e, now); return; }
  if (input_.mode() == TouchMode::Game) {
    if (!game_) return;
    if (e.type == TouchEv::Down || e.type == TouchEv::Move) game_->setControl(motion_.tiltX(), e.x);
    else if (e.type == TouchEv::Up) game_->setControl(motion_.tiltX(), -1);
    if (e.type == TouchEv::GamePress) {
      if (game_->paused()) { game_->resume(now); alertPausedGame_ = false; return; }
      game_->press(e.t, now);
    } else if (e.type == TouchEv::GameHoldExit) {
      game_->takeOutcome();  // sair/abandonar: nenhuma pontuação pela ação de saída
      game_ = nullptr;
      gotoScreen(Screen::GamesMenu, now);
      event("saiu do jogo");
    }
    return;
  }
  // ── pressionar / arrastar ──
  bool isList = listScreen(screen_), isGrid = gridScreen(screen_);
  auto isSlider = [](int h) { return h == B_Q_BRIGHT || h == B_Q_VOL || h == B_SL_VOL || h == B_SL_BRIGHT; };
  if (e.type == TouchEv::Down) {
    suppressSwipe_ = false;
    pressedHit_ = hitTest(e.x, e.y);
    if (isList && pressedHit_ < 0) { list_.onDown(e.y, now); listTouch_ = true; pressedItem_ = list_.hit(e.x, e.y); }
    if (isGrid && pressedHit_ < 0) pressedItem_ = ui::gridHit(itemN_, e.x, e.y);
    quickArm_ = isSlider(pressedHit_) ? pressedHit_ : -1;
  }
  // controle deslizante só "pega" com movimento horizontal (deslizar na vertical continua sendo gesto)
  if (e.type == TouchEv::Move && quickArm_ >= 0 && quickDrag_ < 0 && tbAbs(e.x - e.x0) > 8 && tbAbs(e.x - e.x0) > tbAbs(e.y - e.y0)) quickDrag_ = quickArm_;
  if (e.type == TouchEv::Move && quickDrag_ >= 0) setSlider(quickDrag_, e.x, false, now);
  if (e.type == TouchEv::Tap && quickArm_ >= 0) { setSlider(quickArm_, e.x, true, now); quickArm_ = -1; pressedHit_ = -1; return; }
  if (e.type == TouchEv::Move && isList && listTouch_) { list_.onMove(e.y, now); if (list_.dragged()) pressedItem_ = -1; }
  if (e.type == TouchEv::Move && isGrid && pressedItem_ >= 0 && ui::gridHit(itemN_, e.x, e.y) != pressedItem_) pressedItem_ = -1;
  if (e.type == TouchEv::Move && pressedHit_ >= 0 && hitTest(e.x, e.y) != pressedHit_) pressedHit_ = -1;
  if (e.type == TouchEv::Up) {
    if (isList && listTouch_) list_.onUp(e.vy);
    listTouch_ = false;
    if (quickDrag_ >= 0) { suppressSwipe_ = true; setSlider(quickDrag_, e.x, true, now); quickDrag_ = -1; quickArm_ = -1; return; }
    return;
  }
  if (e.type == TouchEv::Down || e.type == TouchEv::Move) {
    if (screen_ == Screen::Face) onFaceTouch(e, now);
    return;
  }
  // ── toque classificado ──
  if (e.type == TouchEv::Tap || e.type == TouchEv::LongRelease) {
    int h = hitTest(e.x, e.y);
    pressedHit_ = -1;
    if (h >= 0 && !isSlider(h)) { onButton(h, now); return; }
    if (isList) {
      if (e.type == TouchEv::Tap && !list_.dragged()) { int i = list_.hit(e.x, e.y); if (i >= 0) onListPick(items_[i].id, now); }
      pressedItem_ = -1;
      return;
    }
    if (isGrid) {
      int i = e.type == TouchEv::Tap ? ui::gridHit(itemN_, e.x, e.y) : -1;
      pressedItem_ = -1;
      if (i >= 0) { onListPick(items_[i].id, now); return; }
    }
  }
  // ── deslizes e toques por tela ──
  if (suppressSwipe_ && (e.type == TouchEv::SwipeLeft || e.type == TouchEv::SwipeRight || e.type == TouchEv::SwipeUp || e.type == TouchEv::SwipeDown)) { suppressSwipe_ = false; return; }
  bool home = homeScreen(screen_);
  if (home && (e.type == TouchEv::SwipeLeft || e.type == TouchEv::SwipeRight)) {
    static const Screen ORDER[3] = {Screen::Face, Screen::Clock, Screen::Today};
    int i = screen_ == Screen::Face ? 0 : screen_ == Screen::Clock ? 1 : 2;
    int j = i + (e.type == TouchEv::SwipeLeft ? 1 : -1);
    if (j >= 0 && j <= 2) { transDir_ = e.type == TouchEv::SwipeLeft ? -1 : 1; out_.play(Pattern::Swipe, now); gotoScreen(ORDER[j], now); }
    return;
  }
  if (home && e.type == TouchEv::SwipeUp) { out_.play(Pattern::Swipe, now); transDir_ = 0; gotoScreen(Screen::Launcher, now); return; }
  if (home && e.type == TouchEv::SwipeDown) { out_.play(Pattern::Swipe, now); gotoScreen(Screen::Quick, now); return; }
  if (screen_ == Screen::Quick && e.type == TouchEv::SwipeUp) { goBack(now); return; }
  if (screen_ == Screen::Expressions && (e.type == TouchEv::SwipeLeft || e.type == TouchEv::SwipeRight)) {
    onButton(e.type == TouchEv::SwipeLeft ? B_EXPR_NEXT : B_EXPR_PREV, now); return;
  }
  if ((screen_ == Screen::Info || screen_ == Screen::Config || screen_ == Screen::FirstUse) && e.type == TouchEv::SwipeLeft) { onButton(B_PAGE_NEXT, now); return; }
  if (e.type == TouchEv::SwipeRight && !home && screen_ != Screen::FirstUse && screen_ != Screen::Confirm && screen_ != Screen::RecoveryConfirm &&
      screen_ != Screen::Resume && screen_ != Screen::Intro) {
    out_.play(Pattern::Swipe, now); goBack(now); return;
  }
  switch (screen_) {
    case Screen::Intro: if (e.type == TouchEv::Tap) goHome(now); break;
    case Screen::Face: onFaceTouch(e, now); break;
    case Screen::Clock: if (e.type == TouchEv::LongRelease) { set_.watchFace = (uint8_t)((set_.watchFace + 1) % WATCHFACE_COUNT); markSettings(now); } break;
    case Screen::Breath: if (e.type == TouchEv::Tap) onButton(B_BREATH_PLAY, now); break;
    case Screen::Dice: if (e.type == TouchEv::Tap) rollDice(now); break;
    case Screen::Torch: if (e.type == TouchEv::Tap) goBack(now); break;
    case Screen::Today: case Screen::Stats: case Screen::Weather: break;
    default: break;
  }
}

void App::handleMotion(MotionEv e, ms_t now) {
  if (!set_.imuReactions && e != MotionEv::FaceDown && e != MotionEv::FaceUp) return;
  if (screen_ == Screen::Game || screen_ == Screen::Ota) return;
  switch (e) {
    case MotionEv::Shake:
      P().shakes++; markProgress(now);
      if (screen_ == Screen::Dice) { rollDice(now); break; }
      if (screen_ == Screen::ScreenOff) wake(now);
      if (screen_ == Screen::Face || homeScreen(screen_)) { if (screen_ != Screen::Face) gotoScreen(Screen::Face, now); react(Expr::Dizzy, now, 2600, Pattern::Dizzy); }
      event("sacudido");
      break;
    case MotionEv::PickUp:
      if (screen_ == Screen::ScreenOff) { if (set_.wakeOnPickup) wake(now); else break; }
      if (screen_ == Screen::Face) react(Expr::Startled, now, 700, Pattern::Babble);
      noteInteraction(now);
      break;
    case MotionEv::FaceDown:
      if (alertN_ && set_.faceDownSnooze) {
        bool snz = alerts_[0].type == AlertType::Alarm || alerts_[0].type == AlertType::Reminder;
        ackAlert(now, snz);
        toast(snz ? "Adiado" : "Ok", now);
      }
      sleepScreen(now);
      break;
    case MotionEv::FaceUp:
      if (screen_ == Screen::ScreenOff) wake(now);
      break;
    case MotionEv::UpsideDown:
      if (screen_ == Screen::Face) react(Expr::Dizzy, now, 2000, Pattern::Startled);
      break;
    case MotionEv::OnBack:
      if (screen_ == Screen::Face) react(Expr::Rest, now, 2500, Pattern::Yawn);
      break;
    default: break;
  }
}

void App::handleSound(SoundEv e, ms_t now) {
  if (!set_.micReactions || screen_ != Screen::Face) return;
  switch (e) {
    case SoundEv::Clap: react(Expr::Startled, now, 900, Pattern::Startled); event("palma"); break;
    case SoundEv::Loud: react(Expr::Squint, now, 2500); event("barulho alto"); break;
    case SoundEv::MusicStart: event("música: dançando"); break;
    case SoundEv::MusicStop: event("música parou"); break;
    default: break;
  }
}

void App::powerUpdate(const PowerState& p, ms_t now) {
  if (!caps_.battery) { power_ = PowerState(); return; }  // TubaBot Lite: sem bateria
  bool wasCharging = power_.charging;
  power_ = p;
  if (p.charging && !wasCharging && power_.known && screen_ != Screen::Game) {
    if (screen_ == Screen::Face) react(Expr::Charging, now, 3000);
    out_.play(Pattern::Charge, now);
    event("carregando");
  }
  if (p.battery && p.percent >= 0 && p.percent <= 10 && !p.charging) {
    if (!lowBatWarned_) {
      lowBatWarned_ = true;
      toast("Bateria fraca", now, 3000);
      if (screen_ == Screen::Face) react(Expr::Tired, now, 3000);
      out_.play(Pattern::LowBattery, now);
      event("bateria fraca");
    }
  } else if (p.percent > 15 || p.charging) lowBatWarned_ = false;
}

void App::hwButton(HwButton b, bool longPress, ms_t now) {
  noteInteraction(now);
  if (b == HwButton::Boot && longPress) { openConfig(now); return; }  // recuperação independente do toque
  if (screen_ == Screen::ScreenOff) { wake(now); return; }
  if (alertN_) { ackAlert(now, false); return; }
  if (b == HwButton::Power || screen_ == Screen::Face) sleepScreen(now);  // ES3C28P: BOOT no rosto apaga a tela
  else goHome(now);
}

// ── laço principal ──────────────────────────────────────────────────────────
uint32_t App::frameIntervalMs(ms_t now) const {
  if (screen_ == Screen::ScreenOff) return 250;
  if (screen_ == Screen::Torch) return 1000;
  if (screen_ == Screen::Intro || screen_ == Screen::Face || screen_ == Screen::ColorPick) return 33;
  if (screen_ == Screen::Game && game_ && (game_->needsFastFrames() || game_->phase() == GamePhase::Playing)) return 33;
  if (alertN_) return 50;
  if (input_.pressed() || list_.moving() || now - screenAt_ < 400 || toastUntil_ > now) return 33;
  if (screen_ == Screen::Stopwatch && sw_.state() == RunState::Running) return 50;
  if (screen_ == Screen::Breath || screen_ == Screen::Level || screen_ == Screen::Dice || screen_ == Screen::Expressions) return 40;
  if (screen_ == Screen::Clock) return set_.watchFace == 2 ? 50 : 250;
  if (screen_ == Screen::Game) return 100;
  return 200;
}

void App::framePresented(ms_t t) { if (screen_ == Screen::Game && game_) game_->framePresented(t); }

bool App::tick(ms_t now) {
  TouchEvent ev[16];
  // tela apagada: o primeiro toque só acorda (gesto consumido; recuperação continua valendo)
  if (screen_ == Screen::ScreenOff && input_.pressed()) { input_.consumeCurrent(true); wake(now); }
  int n = input_.poll(now, ev, 16);
  for (int i = 0; i < n; i++) handleTouch(ev[i], now);
  if (screen_ == Screen::ScreenOff && input_.pressed()) { input_.consumeCurrent(true); wake(now); }
  MotionEv me[8];
  n = motion_.poll(me, 8);
  for (int i = 0; i < n; i++) handleMotion(me[i], now);
  SoundEv se[8];
  n = sound_.poll(se, 8);
  for (int i = 0; i < n; i++) handleSound(se[i], now);

  if (screen_ == Screen::RecoveryConfirm && now >= recoveryUntil_) gotoScreen(returnScreen_ == Screen::RecoveryConfirm ? homePage_ : returnScreen_, now);

  updateTimeServices(now);

  if (screen_ == Screen::Intro && now - bootAt_ >= (set_.shortIntro ? INTRO_SHORT_MS : INTRO_FULL_MS)) {
    if (!set_.setupDone) { plat_.apOpen(); configPage_ = 0; gotoScreen(Screen::FirstUse, now); }
    else if (checkpointPending()) gotoScreen(Screen::Resume, now);
    else { goHome(now); dockUntil_ = now + 3500; out_.play(Pattern::Babble, now, rng_.next()); }
  }
  if (screen_ == Screen::FirstUse && set_.setupDone) goHome(now);  // concluído pelo portal

  if (screen_ == Screen::Game && game_) {
    if (!input_.pressed()) game_->setControl(motion_.tiltX(), -1);
    game_->update(now);
    GameOutcome o = game_->takeOutcome();
    if (o.finished) onGameOutcome(o, now);
    int sfx = game_ ? game_->takeSfx() : 0;
    if (sfx == 1 && out_.current() != Pattern::Record) out_.play(Pattern::GamePoint, now);
    if (sfx == 2 && out_.current() != Pattern::Record) out_.play(Pattern::GameOver, now);
    if (sfx == 3) out_.play(Pattern::Startled, now);
  }
  // respiração: fases e conclusão da sessão
  if (screen_ == Screen::Breath && breathOn_) {
    static const uint16_t PH[3][4] = {{4000, 4000, 4000, 4000}, {4000, 7000, 8000, 0}, {5000, 0, 5000, 0}};
    static const uint8_t CYC[3] = {4, 4, 6};
    uint32_t cyc = 0; for (int k = 0; k < 4; k++) cyc += PH[breathMode_][k];
    ms_t el = now - breathStart_;
    if (el >= (ms_t)cyc * CYC[breathMode_]) {
      breathOn_ = false;
      Progress& Pr = P(); Pr.breathSessions++; markProgress(now);
      toast("Sessão concluída", now, 2500);
      out_.play(Pattern::Unlock, now);
      if (Pr.breathSessions >= 3) unlock(UNL_ZEN, now);
    } else {
      uint32_t inC = (uint32_t)(el % cyc); int ph = 0;
      while (ph < 3 && inC >= PH[breathMode_][ph]) { inC -= PH[breathMode_][ph]; ph++; }
      int key = (int)(el / cyc) * 4 + ph;
      if (key != breathPhaseLast_) { breathPhaseLast_ = key; if (PH[breathMode_][ph]) out_.play(Pattern::Breath, now); }
    }
  }
  if (rollUntil_ && now >= rollUntil_) { rollUntil_ = 0; out_.play(Pattern::Tap, now); forceFrame_ = true; }
  if (listScreen(screen_)) list_.tick(now);
  if (screen_ == Screen::Face && dockUntil_ && now >= dockUntil_ + 300) { dockUntil_ = 0; forceFrame_ = true; }

  updatePersonality(now);
  const PersonalityTuning& tune = set_.personality ? TUNE_PLAY : TUNE_CALM;
  PersonalityTuning t = tune;
  if (pomo_.phase() == PomoPhase::Focus && pomo_.state() == RunState::Running) t.mischiefPct = 0;  // foco reduz distrações
  face_.update(now, rng_, t, set_.animLevel);

  // descanso do visor (timers continuam)
  bool canSleep = screen_ != Screen::Intro && screen_ != Screen::Game && screen_ != Screen::Ota && screen_ != Screen::ScreenOff &&
                  screen_ != Screen::FirstUse && screen_ != Screen::Confirm && screen_ != Screen::RecoveryConfirm && screen_ != Screen::Resume &&
                  screen_ != Screen::Torch && !(screen_ == Screen::Breath && breathOn_) && !(screen_ == Screen::Stopwatch && sw_.state() == RunState::Running);
  if (set_.screenOffS && canSleep && !alertN_ && !input_.pressed() && now - lastInteraction_ >= (ms_t)set_.screenOffS * 1000) sleepScreen(now);
  // sem interação, tudo volta ao rosto em tela cheia (timers/Pomodoro seguem rodando)
  ms_t idleLimit = 0;
  switch (screen_) {
    case Screen::Face: case Screen::Intro: case Screen::FirstUse: case Screen::Game: case Screen::Ota: case Screen::ScreenOff:
    case Screen::Torch: case Screen::Resume: case Screen::RecoveryConfirm:
      break;
    case Screen::Config: idleLimit = 180000; break;   // tempo para ler o QR e configurar
    case Screen::Breath: idleLimit = breathOn_ ? 0 : 30000; break;
    case Screen::Stopwatch: idleLimit = sw_.state() == RunState::Running ? 0 : 45000; break;
    case Screen::Launcher: case Screen::MoreApps: case Screen::Quick: case Screen::GamesMenu: case Screen::Appearance:
    case Screen::ColorPick: case Screen::Overlays: case Screen::Volume: case Screen::Brightness: case Screen::Wifi:
    case Screen::System: case Screen::Confirm: case Screen::TimerSet: case Screen::Info:
      idleLimit = 20000; break;
    default: idleLimit = 45000; break;  // apps (Pomodoro, Timer, relógio...)
  }
  if (idleLimit && !alertN_ && !input_.pressed() && now - lastInteraction_ >= idleLimit) {
    if (screen_ == Screen::Confirm) confirmAction_ = C_NONE;
    transDir_ = 0;
    goHome(now, Screen::Face);
  }
  applyTheme();

  updateBrightness(now);
  out_.update(now);
  flush(now, false);

  if (forceFrame_ || now - lastRender_ >= frameIntervalMs(now)) {
    forceFrame_ = false;
    lastRender_ = now;
    hitN_ = 0;
    canvas_.beginFrame(ui::BG);
    render(now);
    canvas_.setOrigin(0, 0);
    canvas_.setGlobalAlpha(255);
    dirty_ = canvas_.endFrame();
    return !dirty_.empty();
  }
  return false;
}

// ── ações administrativas ───────────────────────────────────────────────────
void App::forgetWifi() { plat_.wifiForget(); event("rede doméstica esquecida"); }

void App::resetPreferences(ms_t now) {
  Settings keep = set_;
  defaultSettings(set_);
  applyFactoryDefaults(fac_, set_);
  set_.setupDone = keep.setupDone;  // não volta ao primeiro uso só por redefinir preferências
  applyPrefs(now); markSettings(now); flush(now, true);
  event("preferências redefinidas");
}

void App::eraseAll(ms_t now) {
  plat_.storageErase(BlobKind::Settings); plat_.storageErase(BlobKind::Progress); plat_.storageErase(BlobKind::Checkpoint);
  plat_.wifiForget();
  defaultSettings(set_); applyFactoryDefaults(fac_, set_); defaultProgress(prog_); tbMemset(&ckpt_, 0, sizeof ckpt_);
  setDirty_ = progDirty_ = ckptDirty_ = false;
  applyPrefs(now);
  event("tudo apagado");
  plat_.requestReboot();
}

// "Restaurar padrão de fábrica": apaga preferências, progresso, sessão e Wi-Fi.
// O perfil de fábrica (modelo, cor, padrões) fica intacto, então a unidade
// reinicia exatamente como saiu da bancada.
void App::factoryReset(ms_t now) {
  event("restaurar padrão de fábrica");
  eraseAll(now);
  applyFactoryDefaults(fac_, set_);
  applyPrefs(now);
  applyTheme();
}

bool App::otaAllowed(const char** reason) const {
  const char* r = nullptr;
  if (pomo_.state() == RunState::Running) r = "Pomodoro em andamento";
  for (int i = 0; i < kTimers && !r; i++) if (timers_[i].state() == RunState::Running) r = "Timer em andamento";
  if (!r && sw_.state() == RunState::Running) r = "Cronômetro em andamento";
  if (!r && screen_ == Screen::Game) r = "Jogo em andamento";
  if (!r && alertN_) r = "Aviso na tela";
  if (reason) *reason = r;
  return r == nullptr;
}

void App::enterOta(ms_t now) {
  flush(now, true);  // salva tudo antes de gravar
  otaPermille_ = 0;
  gotoScreen(Screen::Ota, now);
  event("atualização iniciada");
}

void App::otaFailed(ms_t now) { toast("Falha na atualização", now, 4000); event("atualização falhou"); goHome(now); }

}  // namespace tb
