#include "app.h"

namespace tb {

enum MenuId { M_FACE, M_CLOCK, M_POMO, M_TIMERS, M_SW, M_GAMES, M_SILENT, M_EXPR, M_CONFIG, M_INFO, M_COUNT };
// rótulos do menu: ver MENU_LABELS_D em app_draw.cpp (mesma ordem de MenuId)

enum ActionId {
  A_BACK = 1, A_SKIP, A_CANCEL_POMO, A_NEXT_TIMER, A_TIMER_DURATION, A_TIMER_REPEAT, A_TIMER_CANCEL,
  A_SW_RESET, A_PRESET0 = 100,
};
enum ConfirmId { C_NONE, C_CANCEL_POMO, C_SW_RESET };

static const PersonalityTuning TUNE_CALM = {3000, 6500, 4000, 9000, 6, 3, 10, 3};
static const PersonalityTuning TUNE_PLAY = {2000, 4500, 1800, 4500, 9, 4, 25, 18};

const char* screenName(Screen s) {
  static const char* const N[] = {"intro", "first_use", "face", "menu", "clock", "pomodoro", "timers", "stopwatch",
    "games", "game", "expressions", "config", "info", "actions", "confirm", "recovery", "resume", "screen_off", "ota"};
  return N[(int)s];
}

static const uint16_t INTRO_FULL_MS = 2800, INTRO_SHORT_MS = 900;

// ── inicialização e persistência ────────────────────────────────────────────
void App::loadAll(ms_t now) {
  (void)now;
  uint8_t buf[512];
  size_t len = 0;
  defaultSettings(set_);
  DecodeResult r = DecodeResult::Empty;
  if (plat_.storageLoad(BlobKind::Settings, buf, sizeof buf, &len)) r = decodeBlob(BlobKind::Settings, SETTINGS_VERSION, buf, len, &set_, sizeof set_);
  if (r != DecodeResult::Ok && r != DecodeResult::Empty && r != DecodeResult::Migrated) { defaultSettings(set_); recovered_ = true; setDirty_ = true; }
  if (r == DecodeResult::Migrated) setDirty_ = true;
  sanitizeSettings(set_);

  defaultProgress(prog_);
  r = DecodeResult::Empty; len = 0;
  if (plat_.storageLoad(BlobKind::Progress, buf, sizeof buf, &len)) r = decodeBlob(BlobKind::Progress, PROGRESS_VERSION, buf, len, &prog_, sizeof prog_);
  if (r != DecodeResult::Ok && r != DecodeResult::Empty && r != DecodeResult::Migrated) { defaultProgress(prog_); recovered_ = true; progDirty_ = true; }
  prog_.mood = tbClamp<int16_t>(prog_.mood, -100, 100);
  prog_.bond = tbMin<uint16_t>(prog_.bond, 1000);
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
  uint8_t buf[512];
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
  (void)now;
  AlertPrefs ap; ap.sound = set_.sound; ap.silent = set_.silent; ap.vibration = set_.vibration;
  out_.configure(model_, ap);  // política central; Lite ignora tudo
  pomo_.cfg.focusMin = set_.pomoFocusMin; pomo_.cfg.shortMin = set_.pomoShortMin; pomo_.cfg.longMin = set_.pomoLongMin;
  pomo_.cfg.cyclesBeforeLong = set_.pomoCyclesBeforeLong; pomo_.cfg.autoStart = set_.pomoAutoStart;
  if (pomo_.state() == RunState::Idle) pomo_.reset();
  for (int i = 0; i < kTimers; i++)
    if (timers_[i].state() == RunState::Idle && timers_[i].durationMs() == 0) timers_[i].set(set_.timerPresetS[1 + i] * 1000);
  plat_.applyDisplay(set_.contrast, set_.rotate180);
  plat_.setTimezone(TZ_TABLE[set_.tzIndex].posix);
}

void App::begin(Model model, ms_t now, uint32_t seed, bool devBuild) {
  model_ = model; devBuild_ = devBuild;
  rng_.seed(seed);
  loadAll(now);       // 1º: preferências — antes de QUALQUER saída
  applyPrefs(now);
  pomo_.reset();
  for (int i = 0; i < kTimers; i++) timers_[i].set(set_.timerPresetS[1 + i] * 1000);
  // token de sessão e PIN de pareamento (por inicialização)
  StrBuf t(token_, sizeof token_); t.hex(rng_.next(), 8).hex(rng_.next(), 8);
  StrBuf p(pin_, sizeof pin_); p.numPad(rng_.range(0, 999999), 6);
  face_.reset(now);
  baseExpr_ = (Expr)set_.favoriteExpr;
  face_.setExpression(baseExpr_, now, true);
  bootAt_ = screenAt_ = lastInteraction_ = lastMinuteTick_ = now;
  nextIdleExpr_ = now + 60000;
  for (int i = 0; i < REMINDER_COUNT; i++) reminderDue_[i] = now + (ms_t)set_.reminders[i].intervalMin * 60000;
  screen_ = Screen::Intro;
  out_.play(Pattern::Boot, now);  // já obedece silencioso/vibração salvos; Lite: só animação
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

void App::setInputModeFor(Screen s, ms_t now) {
  input_.setMode(alertN_ == 0 && s == Screen::Game ? InputMode::Game : InputMode::Normal, now);
}

void App::gotoScreen(Screen s, ms_t now) {
  if (s != Screen::Actions && s != Screen::Confirm && s != Screen::RecoveryConfirm && s != Screen::ScreenOff) {
    if (screen_ == Screen::Game && s != Screen::Game && game_) { game_ = nullptr; }
  }
  screen_ = s; screenAt_ = now; forceFrame_ = true;
  setInputModeFor(s, now);
  if (s == Screen::Face) face_.setExpression(baseExpr_, now);
}

int App::availableExprs(Expr* out, int max) const {
  static const Expr base[] = {Expr::Happy, Expr::Neutral, Expr::Surprised, Expr::Love, Expr::Sad, Expr::Sleepy, Expr::Angry,
                              Expr::WinkL, Expr::WinkR, Expr::Pet, Expr::Dead};
  int n = 0;
  for (Expr e : base) if (n < max) out[n++] = e;
  const Progress& P = testMode_ ? progTest_ : prog_;
  for (int i = 0; i < UNL_COUNT; i++) if ((P.unlocked >> i) & 1u) if (n < max) out[n++] = exprForUnlock(i);
  return n;
}

void App::unlock(UnlockId id, ms_t now) {
  Progress& P = testMode_ ? progTest_ : prog_;
  if ((P.unlocked >> id) & 1u) return;
  P.unlocked |= 1u << id;
  if (!testMode_) markProgress(now);
  char b[40]; StrBuf s(b, sizeof b); s.put("Nova: ").put(exprName(exprForUnlock(id))).put("!");
  toast(b, now, 2500);
  out_.play(Pattern::Unlock, now);
  event(b);
}

void App::addPet(ms_t now) {
  // intervalo mínimo: ruído/toques repetidos não multiplicam o contador
  if (lastPetAt_ && now - lastPetAt_ < 2000) return;
  lastPetAt_ = now;
  Progress& P = testMode_ ? progTest_ : prog_;
  P.pets++;
  P.mood = (int16_t)tbMin(100, P.mood + 5);
  P.bond = (uint16_t)tbMin(1000, P.bond + 3);
  if (!testMode_) markProgress(now);
  unlock(UNL_TIMIDO, now);
}

// ── alertas ─────────────────────────────────────────────────────────────────
void App::startAlertOutput(ms_t now) {
  if (!alertN_) return;
  switch (alerts_[0].type) {
    case AlertType::Timer: out_.play(Pattern::TimerDone, now); break;
    case AlertType::PomoFocusDone: case AlertType::PomoBreakDone: out_.play(Pattern::PomodoroDone, now); break;
    case AlertType::Reminder: out_.play(Pattern::Reminder, now); break;
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
    setInputModeFor(screen_, now);
  }
  static const char* const names[] = {"timer concluído", "foco concluído", "pausa concluída", "lembrete"};
  event(names[(int)t]);
  forceFrame_ = true;
}

void App::ackAlert(ms_t now, bool snooze) {
  if (!alertN_) return;
  Alert a = alerts_[0];
  if (a.type == AlertType::Timer) timers_[a.index].acknowledge();
  if (a.type == AlertType::Reminder) reminderDue_[a.index] = now + (snooze ? 10ull * 60000 : (ms_t)set_.reminders[a.index].intervalMin * 60000);
  for (int i = 0; i + 1 < alertN_; i++) alerts_[i] = alerts_[i + 1];
  alertN_--;
  out_.stopAll();
  if (alertN_) startAlertOutput(now);
  setInputModeFor(screen_, now);
  event(snooze ? "lembrete adiado" : "alerta reconhecido");
  forceFrame_ = true;
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

bool App::reminderBlocked(ms_t now) const {
  if (!set_.quietEnabled || !clock_.valid()) return false;
  int h = clock_.local(now, TZ_TABLE[set_.tzIndex].offsetMin).hour;
  int a = set_.quietStartH, b = set_.quietEndH;
  return a <= b ? (h >= a && h < b) : (h >= a || h < b);
}

void App::updateTimeServices(ms_t now) {
  Pomodoro::Event pe = pomo_.poll(now);
  if (pe != Pomodoro::Event::None) {
    if (pe == Pomodoro::Event::FocusDone) {
      Progress& P = testMode_ ? progTest_ : prog_;
      P.pomodorosDone++;
      P.mood = (int16_t)tbMin(100, P.mood + 4);
      if (!testMode_) markProgress(now);
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
      if (reminderBlocked(now)) { event("lembrete pulado (descanso)"); continue; }
      if (focusRunning) { reminderPending_[i] = true; continue; }  // respeita o foco
      pushAlert(AlertType::Reminder, (uint8_t)i, now);
    }
    if (reminderPending_[i] && !focusRunning) { reminderPending_[i] = false; pushAlert(AlertType::Reminder, (uint8_t)i, now); }
  }
}

void App::setTime(int64_t epoch, ms_t now) {
  clock_.set(epoch, now);
  event("hora ajustada");
}

// ── personalidade ───────────────────────────────────────────────────────────
Expr App::idleExpression(ms_t now) {
  Progress& P = testMode_ ? progTest_ : prog_;
  if (pomo_.state() == RunState::Running) return pomo_.phase() == PomoPhase::Focus ? Expr::Focus : Expr::Rest;
  if (energy_ < 25) return Expr::Sleepy;
  if (clock_.valid()) {
    int h = clock_.local(now, TZ_TABLE[set_.tzIndex].offsetMin).hour;
    if (h >= 23 || h < 6) return Expr::Sleepy;
  }
  if (P.mood < -20) return Expr::Neutral;
  Expr fav = (Expr)set_.favoriteExpr;
  int r = rng_.range(0, 99);
  bool playful = set_.personality == (uint8_t)Personality::Brincalhao;
  if (r < (playful ? 50 : 60)) return fav;
  if (r < (playful ? 80 : 85)) return Expr::Happy;
  if (playful && r < 90) return Expr::Surprised;
  return Expr::Neutral;
}

void App::updatePersonality(ms_t now) {
  if (now - lastMinuteTick_ >= 60000) {
    lastMinuteTick_ += 60000;
    if (now - lastInteraction_ > 120000) energy_ = tbMax(5, energy_ - 1);
    Progress& P = testMode_ ? progTest_ : prog_;
    if (P.mood > 20) P.mood--; else if (P.mood < 20) P.mood++;  // deriva lenta, em memória
  }
  if (screen_ != Screen::Face) return;
  HoldStage st = input_.holdStage(now);
  face_.setPetting(st == HoldStage::Medium);
  if (st == HoldStage::Long) { face_.setExpression(Expr::Love, now); exprUntil_ = now + 400; }
  if (exprUntil_ && now >= exprUntil_) { exprUntil_ = 0; face_.setExpression(baseExpr_, now); }
  if (!exprUntil_ && st == HoldStage::None && now >= nextIdleExpr_) {
    int f = set_.animLevel == 0 ? 160 : (set_.animLevel == 2 ? 70 : 100);
    nextIdleExpr_ = now + (set_.personality ? 45000ull : 90000ull) * f / 100;
    baseExpr_ = idleExpression(now);
    face_.setExpression(baseExpr_, now);
  }
}

// ── jogos ───────────────────────────────────────────────────────────────────
void App::startGame(GameId g, ms_t now) {
  Progress& P = testMode_ ? progTest_ : prog_;
  gameId_ = g;
  switch (g) {
    case GameId::Jump: game_ = &jump_; jump_.best = P.jumpBest; break;
    case GameId::Reflex: game_ = &reflex_; reflex_.best = P.reflexBestMs; break;
    case GameId::Fish: game_ = &fish_; fish_.best = P.fishBest; break;
    default: game_ = &timing_; timing_.best = P.timingBestPct100; timing_.bestAux = P.timingBestErrMs; break;
  }
  game_->newRecord = false;
  game_->begin(now, &rng_, set_.jumpEasy);
  screen_ = Screen::Game; screenAt_ = now; forceFrame_ = true;
  setInputModeFor(screen_, now);
  event(gameTitle(g));
}

void App::onGameOutcome(const GameOutcome& o, ms_t now) {
  if (!o.finished || !game_) return;
  game_->newRecord = false;
  if (!o.valid) return;
  Progress& P = testMode_ ? progTest_ : prog_;
  bool rec = false;
  P.gamesPlayed++;
  P.mood = (int16_t)tbMin(100, P.mood + 2);
  switch (gameId_) {
    case GameId::Jump:
      if (o.score > P.jumpBest) { P.jumpBest = (uint16_t)o.score; rec = true; }
      if (o.score >= 10) unlock(UNL_VITORIOSO, now);
      game_->best = P.jumpBest; break;
    case GameId::Reflex:
      P.reflexValid++;
      if (P.reflexBestMs == 0 || o.score < P.reflexBestMs) { P.reflexBestMs = (uint16_t)tbMin<int32_t>(o.score, 65535); rec = true; }
      if (P.reflexValid >= 5) unlock(UNL_ESPANTADO, now);
      game_->best = P.reflexBestMs; break;
    case GameId::Fish:
      if (o.score > P.fishBest) { P.fishBest = (uint16_t)o.score; rec = o.score > 0; }
      if (o.score >= 5) unlock(UNL_SATISFEITO, now);
      game_->best = P.fishBest; break;
    default:
      if (o.score > P.timingBestPct100 || (o.score == P.timingBestPct100 && o.score > 0 && o.aux < P.timingBestErrMs)) {
        P.timingBestPct100 = (uint16_t)o.score; P.timingBestErrMs = (uint16_t)tbMin<int32_t>(o.aux, 65535);
        P.timingBestTargetCs = (uint16_t)o.aux2; rec = true;
      }
      if (o.score >= 9800) unlock(UNL_PRECISAO, now);
      game_->best = P.timingBestPct100; break;
  }
  game_->newRecord = rec;
  if (rec) out_.play(Pattern::Record, now);
  if (!testMode_) markProgress(now);
}

// ── gestos ──────────────────────────────────────────────────────────────────
void App::onFace(Gesture g, ms_t now) {
  switch (g) {
    case Gesture::Tap: {
      Expr list[32]; int n = availableExprs(list, 32);
      int idx = 0;
      for (int i = 0; i < n; i++) if (list[i] == baseExpr_) { idx = i; break; }
      baseExpr_ = list[(idx + 1) % n];
      exprUntil_ = 0;
      face_.setExpression(baseExpr_, now);
      out_.play(Pattern::Tap, now);
      break;
    }
    case Gesture::DoubleTap:
      out_.play(Pattern::Menu, now);
      gotoScreen(Screen::Menu, now);
      break;
    case Gesture::Medium:  // carinho
      face_.setExpression(Expr::Pet, now); exprUntil_ = now + 2500;
      out_.play(Pattern::Purr, now);
      addPet(now);
      event("carinho");
      break;
    case Gesture::Long:    // amor / corações
      face_.setExpression(Expr::Love, now); exprUntil_ = now + 3000;
      out_.play(Pattern::Love, now);
      addPet(now);
      event("amor");
      break;
    default: break;
  }
}

void App::onMenu(Gesture g, ms_t now) {
  if (g == Gesture::Tap) { menuSel_ = (menuSel_ + 1) % M_COUNT; out_.play(Pattern::Menu, now); }
  else if (g == Gesture::DoubleTap) gotoScreen(Screen::Face, now);
  else if (g == Gesture::Medium || g == Gesture::Long) activateMenuItem(menuSel_, now);
}

void App::activateMenuItem(int id, ms_t now) {
  out_.play(Pattern::Tap, now);
  switch (id) {
    case M_FACE: gotoScreen(Screen::Face, now); break;
    case M_CLOCK: gotoScreen(Screen::Clock, now); break;
    case M_POMO: gotoScreen(Screen::Pomodoro, now); break;
    case M_TIMERS: gotoScreen(Screen::Timers, now); break;
    case M_SW: gotoScreen(Screen::Stopwatch, now); break;
    case M_GAMES: gameSel_ = 0; gotoScreen(Screen::GamesMenu, now); break;
    case M_SILENT:
      if (model_ == Model::Lite) toast("Lite: só avisos na tela", now);
      else {
        set_.silent = !set_.silent; applyPrefs(now); markSettings(now);
        toast(set_.silent ? "Silencioso: ligado" : "Silencioso: desligado", now);
      }
      break;
    case M_EXPR: exprSel_ = 0; gotoScreen(Screen::Expressions, now); break;
    case M_CONFIG: openConfig(now); break;
    case M_INFO: infoPage_ = 0; gotoScreen(Screen::Info, now); break;
  }
}

void App::openActions(ms_t now) {
  listN_ = 0; listSel_ = 0;
  auto add = [&](const char* l, int id) { if (listN_ < kListMax) { tbStrlcpy(listLabels_[listN_], l, sizeof listLabels_[0]); list_[listN_].label = listLabels_[listN_]; list_[listN_].id = id; listN_++; } };
  if (screen_ == Screen::Pomodoro) { add("Pular etapa", A_SKIP); add("Cancelar", A_CANCEL_POMO); add("Voltar", A_BACK); }
  else if (screen_ == Screen::Timers) {
    add("Próximo timer", A_NEXT_TIMER); add("Mudar duração", A_TIMER_DURATION);
    add("Repetir", A_TIMER_REPEAT); add("Cancelar", A_TIMER_CANCEL); add("Voltar", A_BACK);
  } else if (screen_ == Screen::Stopwatch) { add("Zerar", A_SW_RESET); add("Voltar", A_BACK); }
  else return;
  returnScreen_ = screen_;
  screen_ = Screen::Actions; screenAt_ = now; forceFrame_ = true;
}

void App::activateAction(int id, ms_t now) {
  Screen back = returnScreen_;
  auto done = [&]() { screen_ = back; screenAt_ = now; forceFrame_ = true; };
  if (id >= A_PRESET0 && id < A_PRESET0 + TIMER_PRESET_COUNT) {
    Countdown& t = timers_[timerSel_];
    t.cancel(); t.set(set_.timerPresetS[id - A_PRESET0] * 1000);
    writeCheckpoint(now); done(); return;
  }
  switch (id) {
    case A_BACK: done(); break;
    case A_SKIP: pomo_.skip(now); writeCheckpoint(now); event("etapa pulada"); done(); break;
    case A_CANCEL_POMO: confirmAction_ = C_CANCEL_POMO; screen_ = Screen::Confirm; forceFrame_ = true; break;
    case A_NEXT_TIMER: timerSel_ = (timerSel_ + 1) % kTimers; done(); break;
    case A_TIMER_DURATION: {
      listN_ = 0; listSel_ = 0;
      for (int i = 0; i < TIMER_PRESET_COUNT; i++) {
        StrBuf s(listLabels_[i], sizeof listLabels_[0]);
        formatDuration(s, (ms_t)set_.timerPresetS[i] * 1000, false);
        if (i == TIMER_PRESET_COUNT - 1) s.put(" (pers.)");
        list_[i].label = listLabels_[i]; list_[i].id = A_PRESET0 + i; listN_++;
      }
      forceFrame_ = true;
      break;
    }
    case A_TIMER_REPEAT: { Countdown& t = timers_[timerSel_]; t.cancel(); t.start(now); writeCheckpoint(now); done(); break; }
    case A_TIMER_CANCEL: timers_[timerSel_].cancel(); writeCheckpoint(now); done(); break;
    case A_SW_RESET:
      if (sw_.elapsedMs(now) > 0) { confirmAction_ = C_SW_RESET; screen_ = Screen::Confirm; forceFrame_ = true; }
      else done();
      break;
    default: done();
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
  screen_ = back == Screen::ScreenOff ? Screen::Face : back;
  screenAt_ = now; forceFrame_ = true;
  noteInteraction(now);
  setInputModeFor(screen_, now);
}

void App::handleGesture(const GestureEvent& e, ms_t now) {
  Gesture g = e.g;
  if (g == Gesture::None) return;
  if (g == Gesture::StuckDetected) { event("sensor preso?"); toast("Sensor preso? Solte", now, 3000); return; }
  noteInteraction(now);
  if (screen_ == Screen::Ota) return;
  if (screen_ == Screen::ScreenOff) {
    if (g == Gesture::RecoveryArmed) { wake(now); } else return;
  }
  if (g == Gesture::RecoveryArmed) {
    if (screen_ != Screen::RecoveryConfirm) returnScreen_ = screen_ == Screen::Game ? Screen::GamesMenu : screen_;
    if (screen_ == Screen::Game) game_ = nullptr;
    screen_ = Screen::RecoveryConfirm; screenAt_ = now; recoveryUntil_ = now + 6000; forceFrame_ = true;
    setInputModeFor(screen_, now);
    return;
  }
  // Alerta tem prioridade sobre a interação da tela
  if (alertN_ && input_.mode() == InputMode::Normal) {
    bool isReminder = alerts_[0].type == AlertType::Reminder;
    if (g == Gesture::Tap || g == Gesture::DoubleTap) ackAlert(now, false);
    else if (g == Gesture::Medium && isReminder) ackAlert(now, true);
    else { ackAlert(now, false); if (screen_ != Screen::Game) gotoScreen(Screen::Menu, now); }
    return;
  }
  if (input_.mode() == InputMode::Game) {
    if (!game_) return;
    if (g == Gesture::GamePress) {
      if (game_->paused()) { game_->resume(now); alertPausedGame_ = false; forceFrame_ = true; return; }
      game_->press(e.t, now);
    } else if (g == Gesture::GameHoldExit) {
      // sair/abandonar: nenhuma pontuação pela ação de saída
      game_->takeOutcome();
      game_ = nullptr;
      gotoScreen(Screen::GamesMenu, now);
      event("saiu do jogo");
    }
    return;
  }
  switch (screen_) {
    case Screen::Intro: gotoScreen(Screen::Face, now); break;  // pular abertura
    case Screen::FirstUse:
      if (g == Gesture::Tap) { configPage_++; forceFrame_ = true; }
      else if (g == Gesture::Medium || g == Gesture::Long) {
        set_.setupDone = 1; markSettings(now); plat_.apClose(); event("usar offline");
        gotoScreen(Screen::Face, now);
      }
      break;
    case Screen::Face: onFace(g, now); break;
    case Screen::Menu: onMenu(g, now); break;
    case Screen::Clock:
      if (g == Gesture::Tap) { clockSeconds_ = !clockSeconds_; forceFrame_ = true; }
      else gotoScreen(Screen::Menu, now);
      break;
    case Screen::Pomodoro:
      if (g == Gesture::Tap) { pomo_.toggle(now); writeCheckpoint(now); out_.play(Pattern::Tap, now); event("pomodoro"); }
      else if (g == Gesture::DoubleTap) openActions(now);
      else gotoScreen(Screen::Menu, now);  // volta ao menu SEM cancelar a contagem
      break;
    case Screen::Timers:
      if (g == Gesture::Tap) {
        Countdown& t = timers_[timerSel_];
        if (t.state() == RunState::Idle) t.start(now);
        else if (t.state() == RunState::Running) t.pause(now);
        else if (t.state() == RunState::Paused) t.resume(now);
        writeCheckpoint(now); out_.play(Pattern::Tap, now);
      } else if (g == Gesture::DoubleTap) openActions(now);
      else gotoScreen(Screen::Menu, now);
      break;
    case Screen::Stopwatch:
      if (g == Gesture::Tap) { sw_.toggle(now); writeCheckpoint(now); out_.play(Pattern::Tap, now); }
      else if (g == Gesture::DoubleTap) openActions(now);
      else gotoScreen(Screen::Menu, now);
      break;
    case Screen::GamesMenu:
      if (g == Gesture::Tap) { gameSel_ = (gameSel_ + 1) % 5; forceFrame_ = true; out_.play(Pattern::Menu, now); }
      else if (g == Gesture::DoubleTap) gotoScreen(Screen::Menu, now);
      else { if (gameSel_ == 4) gotoScreen(Screen::Menu, now); else startGame((GameId)gameSel_, now); }
      break;
    case Screen::Expressions: {
      if (g == Gesture::Tap) { exprSel_ = (exprSel_ + 1) % ((int)Expr::Count); if ((Expr)exprSel_ == Expr::Blink) exprSel_++; forceFrame_ = true; }
      else if (g == Gesture::DoubleTap) gotoScreen(Screen::Menu, now);
      else {
        Expr e = (Expr)exprSel_;
        int uid;
        const Progress& P = testMode_ ? progTest_ : prog_;
        if (exprIsUnlockable(e, &uid) && !((P.unlocked >> uid) & 1u)) { toast("Ainda bloqueada", now); break; }
        baseExpr_ = e; exprUntil_ = 0; nextIdleExpr_ = now + 120000;
        gotoScreen(Screen::Face, now);
      }
      break;
    }
    case Screen::Config:
      if (g == Gesture::Tap) { configPage_++; forceFrame_ = true; }
      else if (g == Gesture::DoubleTap) gotoScreen(Screen::Menu, now);
      else { plat_.apClose(); toast("Rede do Tuba fechada", now); event("rede fechada"); gotoScreen(Screen::Menu, now); }
      break;
    case Screen::Info:
      if (g == Gesture::Tap) { infoPage_ = (infoPage_ + 1) % 6; forceFrame_ = true; }
      else if (g == Gesture::DoubleTap) gotoScreen(Screen::Menu, now);
      else if (infoPage_ == 1) openConfig(now);
      else gotoScreen(Screen::Menu, now);
      break;
    case Screen::Actions:
      if (g == Gesture::Tap) { listSel_ = (listSel_ + 1) % tbMax(1, listN_); forceFrame_ = true; }
      else if (g == Gesture::DoubleTap) { screen_ = returnScreen_; forceFrame_ = true; }
      else if (listN_) activateAction(list_[listSel_].id, now);
      break;
    case Screen::Confirm:
      if (g == Gesture::Medium || g == Gesture::Long) {
        if (confirmAction_ == C_CANCEL_POMO) { pomo_.cancel(); event("pomodoro cancelado"); }
        if (confirmAction_ == C_SW_RESET) { sw_.reset(); event("cronômetro zerado"); }
        writeCheckpoint(now);
      }
      confirmAction_ = C_NONE;
      screen_ = returnScreen_; forceFrame_ = true;
      break;
    case Screen::RecoveryConfirm:
      if (g == Gesture::Tap) openConfig(now);
      else gotoScreen(returnScreen_ == Screen::RecoveryConfirm ? Screen::Face : returnScreen_, now);
      break;
    case Screen::Resume:
      if (g == Gesture::Tap) resumeFromCheckpoint(now);
      else if (g == Gesture::Medium || g == Gesture::Long) {
        tbMemset(&ckpt_, 0, sizeof ckpt_); ckptDirty_ = true; lastChange_ = now;
        event("sessão descartada"); gotoScreen(Screen::Face, now);
      }
      break;
    default: break;
  }
}

// ── laço principal ──────────────────────────────────────────────────────────
uint32_t App::frameIntervalMs(ms_t now) const {
  if (screen_ == Screen::ScreenOff) return 250;
  if (screen_ == Screen::Intro) return 33;
  if (screen_ == Screen::Game && game_ && game_->needsFastFrames()) return 33;
  if (input_.pressed()) return 50;
  if (screen_ == Screen::Face && face_.animating(now)) return 33;
  if (screen_ == Screen::Stopwatch && sw_.state() == RunState::Running) return 50;
  return 100;
}

void App::framePresented(ms_t t) { if (screen_ == Screen::Game && game_) game_->framePresented(t); }

bool App::tick(ms_t now) {
  GestureEvent ev[8];
  // tela apagada: o primeiro toque só acorda (gesto consumido; recuperação continua valendo)
  if (screen_ == Screen::ScreenOff && input_.pressed()) { input_.consumeCurrent(true); wake(now); }
  int n = input_.poll(now, ev, 8);
  for (int i = 0; i < n; i++) handleGesture(ev[i], now);
  if (screen_ == Screen::ScreenOff && input_.pressed()) { input_.consumeCurrent(true); wake(now); }

  if (screen_ == Screen::RecoveryConfirm && now >= recoveryUntil_) gotoScreen(returnScreen_ == Screen::RecoveryConfirm ? Screen::Face : returnScreen_, now);

  updateTimeServices(now);

  if (screen_ == Screen::Intro && now - bootAt_ >= (set_.shortIntro ? INTRO_SHORT_MS : INTRO_FULL_MS)) {
    if (!set_.setupDone) { plat_.apOpen(); gotoScreen(Screen::FirstUse, now); }
    else if (checkpointPending()) gotoScreen(Screen::Resume, now);
    else gotoScreen(Screen::Face, now);
  }
  if (screen_ == Screen::FirstUse && set_.setupDone) gotoScreen(Screen::Face, now);  // concluído pelo portal

  if (screen_ == Screen::Game && game_) {
    game_->update(now);
    GameOutcome o = game_->takeOutcome();
    if (o.finished) onGameOutcome(o, now);
    int sfx = game_ ? game_->takeSfx() : 0;
    if (sfx == 1 && out_.current() != Pattern::Record) out_.play(Pattern::GamePoint, now);
    if (sfx == 2 && out_.current() != Pattern::Record) out_.play(Pattern::GameOver, now);
  }

  updatePersonality(now);
  const PersonalityTuning& tune = set_.personality ? TUNE_PLAY : TUNE_CALM;
  PersonalityTuning t = tune;
  if (pomo_.phase() == PomoPhase::Focus && pomo_.state() == RunState::Running) t.mischiefPct = 0;  // foco reduz distrações
  face_.update(now, rng_, t, set_.animLevel);

  // descanso do visor (timers continuam)
  bool canSleep = screen_ == Screen::Face || screen_ == Screen::Clock || screen_ == Screen::Pomodoro || screen_ == Screen::Timers ||
                  screen_ == Screen::Stopwatch || screen_ == Screen::Menu || screen_ == Screen::Info || screen_ == Screen::Config ||
                  screen_ == Screen::Expressions || screen_ == Screen::GamesMenu;
  if (set_.screenOffS && canSleep && !alertN_ && !input_.pressed() && now - lastInteraction_ >= (ms_t)set_.screenOffS * 1000) {
    returnScreen_ = screen_; screen_ = Screen::ScreenOff; screenAt_ = now; forceFrame_ = true;
    event("visor em descanso");
  }
  // listas voltam ao rosto após inatividade
  bool listy = screen_ == Screen::Menu || screen_ == Screen::Info || screen_ == Screen::Actions || screen_ == Screen::GamesMenu ||
               screen_ == Screen::Expressions || screen_ == Screen::Confirm;
  if (listy && !alertN_ && now - lastInteraction_ >= 30000) gotoScreen(Screen::Face, now);

  out_.update(now);
  flush(now, false);

  if (forceFrame_ || now - lastRender_ >= frameIntervalMs(now)) {
    forceFrame_ = false;
    lastRender_ = now;
    fb_.clear();
    render(now);
    bool changed = tbMemcmp(fb_.buf, prev_.buf, FB_BYTES) != 0;
    if (changed) tbMemcpy(prev_.buf, fb_.buf, FB_BYTES);
    return changed;
  }
  return false;
}

// ── ações administrativas ───────────────────────────────────────────────────
void App::forgetWifi() { plat_.wifiForget(); event("rede doméstica esquecida"); }

void App::resetPreferences(ms_t now) {
  Settings keep = set_;
  defaultSettings(set_);
  set_.setupDone = keep.setupDone;  // não volta ao primeiro uso só por redefinir preferências
  applyPrefs(now); markSettings(now); flush(now, true);
  event("preferências redefinidas");
}

void App::eraseAll(ms_t now) {
  plat_.storageErase(BlobKind::Settings); plat_.storageErase(BlobKind::Progress); plat_.storageErase(BlobKind::Checkpoint);
  plat_.wifiForget();
  defaultSettings(set_); defaultProgress(prog_); tbMemset(&ckpt_, 0, sizeof ckpt_);
  setDirty_ = progDirty_ = ckptDirty_ = false;
  applyPrefs(now);
  event("tudo apagado");
  plat_.requestReboot();
}

bool App::otaAllowed(const char** reason) const {
  const char* r = nullptr;
  if (pomo_.state() == RunState::Running) r = "Pomodoro em andamento";
  for (int i = 0; i < kTimers && !r; i++) if (timers_[i].state() == RunState::Running) r = "Timer em andamento";
  if (!r && sw_.state() == RunState::Running) r = "Cronômetro em andamento";
  if (!r && screen_ == Screen::Game) r = "Jogo em andamento";
  if (reason) *reason = r;
  return r == nullptr;
}

void App::enterOta(ms_t now) {
  flush(now, true);  // salva tudo antes de gravar
  otaPermille_ = 0;
  gotoScreen(Screen::Ota, now);
  event("atualização iniciada");
}

void App::otaFailed(ms_t now) { toast("Falha na atualização", now, 4000); event("atualização falhou"); gotoScreen(Screen::Face, now); }

}  // namespace tb
