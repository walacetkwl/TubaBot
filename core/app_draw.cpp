#include "app.h"

namespace tb {

static const char* const MENU_LABELS_D[] = {
  "Rosto", "Relógio", "Pomodoro", "Timers", "Cronômetro", "Jogos", "Silencioso", "Expressões", "Configurar", "Informações"};
static const char* const WEEKDAYS[] = {"dom", "seg", "ter", "qua", "qui", "sex", "sáb"};
static const char* const UNLOCK_CRITERIA[UNL_COUNT] = {
  "1º carinho", "1º Pomodoro completo", "10 obstáculos (Pula)", "5 peixes (Bote)", "5 reflexos válidos", "98% Tempo Certo"};

// seno inteiro (−100..100) com período de 64 passos
static int isin64(int i) {
  static const int8_t Q[17] = {0, 10, 20, 29, 38, 47, 56, 63, 71, 77, 83, 88, 92, 96, 98, 100, 100};
  i &= 63;
  if (i < 16) return Q[i];
  if (i < 32) return Q[32 - i];
  if (i < 48) return -Q[i - 32];
  return -Q[64 - i];
}

static void drawFin(Framebuffer& fb, int x, int base, int h) {
  // barbatana: borda frontal reta, ponta e borda traseira côncava
  fb.fillTriangle(x, base, x + 18, base, x + 13, base - h);
  fb.fillCircle(x + 21, base - h / 2 - 2, h / 3 + 1, false);
}

static void drawWave(Framebuffer& fb, int y, int phase) {
  for (int x = 0; x < 128; x++) fb.set(x, y + isin64(x * 2 + phase) * 2 / 100);
}

void App::drawIntro(ms_t now) {
  int t = (int)(now - bootAt_);
  bool shortI = set_.shortIntro;
  int finEnd = shortI ? 400 : 1100, textEnd = shortI ? 650 : 1700, eyesEnd = shortI ? 900 : 2400;
  if (t < finEnd) {
    // 1. barbatana branca nadando com pequena ondulação
    int p = t * 1000 / finEnd;
    int x = -24 + (54 + 24) * p / 1000;
    int bob = isin64(t / 40) * 2 / 100;
    drawFin(fb_, x, 44 + bob, 20);
    drawWave(fb_, 46, t / 25);
    return;
  }
  if (t < textEnd) {
    // 2. barbatana mergulha enquanto "TubaBot" é revelado
    int p = (t - finEnd) * 1000 / (textEnd - finEnd);
    int sink = 20 * p / 1000;
    drawFin(fb_, 54, 44 + sink, 20 - sink);
    fb_.fillRect(0, 45, 128, 19, false);
    drawWave(fb_, 46, t / 25);
    int w = Framebuffer::textWidth("TubaBot", 2);
    int x0 = (128 - w) / 2;
    fb_.text(x0, 14, "TubaBot", 2);
    fb_.fillRect(x0 + w * p / 1000, 12, w + 2, 18, false);
    return;
  }
  // 3. dois olhinhos piscam e assumem a expressão inicial; 4. saudação
  fb_.textCentered(0, "TubaBot");
  int pe = (t - textEnd);
  int blink = 1000;
  if (pe < 250) blink = 1000 - pe * 4;
  else if (pe < 350) blink = (pe - 250) * 10;
  else if (pe < 450) blink = 1000 - (pe - 350) * 10;
  else blink = 0;
  blink = tbClamp(blink, 0, 1000);
  FaceDef f = faceDef(baseExpr_);
  f.mouth = Mouth::None; f.extras = FX_NONE;
  f.L.dy += 4; f.R.dy += 4; f.L.h -= 6; f.R.h -= 6;
  FaceRenderer::drawFace(fb_, f, now, 0, 0, blink);
  if (t >= eyesEnd - 300 || shortI) {
    char b[40]; StrBuf s(b, sizeof b);
    s.put("Oi");
    if (set_.nickname[0]) s.put(", ").put(set_.nickname);
    s.put('!');
    fb_.fillRect(0, 54, 128, 10, false);
    fb_.textCentered(55, b);
  }
}

void App::drawList(const char* title, ms_t now) {
  (void)now;
  const char* labels[kListMax + 4];
  int n = 0, sel = 0;
  if (screen_ == Screen::Menu) { for (int i = 0; i < 10; i++) labels[n++] = MENU_LABELS_D[i]; sel = menuSel_; }
  else if (screen_ == Screen::GamesMenu) {
    for (int i = 0; i < 4; i++) labels[n++] = gameTitle((GameId)i);
    labels[n++] = "Voltar"; sel = gameSel_;
  } else { for (int i = 0; i < listN_; i++) labels[n++] = list_[i].label; sel = listSel_; }
  fb_.text(0, 0, title);
  if (screen_ == Screen::Menu && model_ == Model::Full && set_.silent) fb_.text(110, 0, "(s)");
  fb_.hline(0, 9, 128);
  const int rows = 4;
  int first = tbClamp(sel - 1, 0, tbMax(0, n - rows));
  for (int r = 0; r < rows && first + r < n; r++) {
    int i = first + r, y = 11 + r * 11;
    if (i == sel) { fb_.fillRoundRect(0, y, 122, 11, 2); fb_.text(4, y + 2, labels[i], 1, false); }
    else fb_.text(4, y + 2, labels[i]);
  }
  // barra de rolagem
  if (n > rows) {
    int h = 44 * rows / n, y = 11 + (44 - h) * first / tbMax(1, n - rows);
    fb_.vline(126, 11, 44); fb_.fillRect(125, y, 3, h);
  }
  if (screen_ == Screen::GamesMenu && sel < 4) {
    const Progress& P = testMode_ ? progTest_ : prog_;
    char b[32]; StrBuf s(b, sizeof b); s.put("Rec: ");
    switch ((GameId)sel) {
      case GameId::Jump: s.num(P.jumpBest); break;
      case GameId::Reflex: if (P.reflexBestMs) s.num(P.reflexBestMs).put(" ms"); else s.put("--"); break;
      case GameId::Fish: s.num(P.fishBest).put(" peixes"); break;
      default: if (P.timingBestPct100) s.fixed2(P.timingBestPct100).put('%'); else s.put("--");
    }
    fb_.textCentered(56, b);
  } else {
    fb_.textCentered(56, "toque:próx segure:ok");
  }
}

void App::drawHoldHint(ms_t now) {
  if (input_.mode() != InputMode::Normal || !input_.pressed()) return;
  HoldStage st = input_.holdStage(now);
  ms_t h = input_.holdMs(now);
  if (st == HoldStage::Stuck) {
    fb_.fillRect(8, 20, 112, 22, false); fb_.rect(8, 20, 112, 22);
    fb_.textCentered(23, "Sensor preso?"); fb_.textCentered(32, "Solte a cabeça");
    return;
  }
  if (h >= 4000) {
    // prévia da recuperação de configuração (8 s)
    fb_.fillRect(6, 16, 116, 32, false); fb_.rect(6, 16, 116, 32);
    if (st == HoldStage::Recovery) { fb_.textCentered(20, "Solte para"); fb_.textCentered(29, "configurar"); }
    else { fb_.textCentered(20, "Segure para"); fb_.textCentered(29, "configurar..."); }
    fb_.progressBar(14, 39, 100, 6, (int)tbMin<ms_t>(1000, (h - 4000) * 1000 / 4000));
    return;
  }
  if (st == HoldStage::Medium || st == HoldStage::Long) {
    const char* txt = "solte: ok";
    if (screen_ == Screen::Face) txt = st == HoldStage::Long ? "  amor!" : "carinho...";
    else if (screen_ == Screen::Pomodoro || screen_ == Screen::Timers || screen_ == Screen::Stopwatch || screen_ == Screen::Clock) txt = "solte: menu";
    else if (screen_ == Screen::Confirm) txt = "solte: sim";
    fb_.fillRect(0, 54, 128, 10, false);
    fb_.fillRoundRect(20, 54, 88, 10, 3);
    fb_.textCentered(55, txt, 1, false);
    if (screen_ == Screen::Face && st == HoldStage::Long) fb_.heart(43, 58, 2, false);
  }
}

void App::drawAlert(ms_t now) {
  const Alert& a = alerts_[0];
  fb_.clear();
  bool flash = ((now - a.at) / 500) % 2 == 0;
  fb_.rect(0, 0, 128, 64); fb_.rect(2, 2, 124, 60);
  if (flash) fb_.fillRect(4, 4, 120, 13);
  char b[48]; StrBuf s(b, sizeof b);
  const char* head = "AVISO";
  switch (a.type) {
    case AlertType::Timer: head = "TIMER"; s.put("Timer ").num(a.index + 1).put(" acabou!"); break;
    case AlertType::PomoFocusDone: head = "POMODORO"; s.put("Foco concluído!"); break;
    case AlertType::PomoBreakDone: head = "POMODORO"; s.put("Pausa acabou!"); break;
    case AlertType::Reminder: head = "LEMBRETE"; s.put(set_.reminders[a.index].label); break;
  }
  fb_.textCentered(7, head, 1, !flash);
  fb_.textCentered(24, b);
  if (a.type == AlertType::PomoFocusDone) { FaceDef f = faceDef(Expr::Celebrate); (void)f; fb_.heart(64, 38, 3); }
  else fb_.fillCircle(64, 38, (int)(2 + ((now / 250) % 3)));
  fb_.textCentered(47, a.type == AlertType::Reminder ? "toque:ok segure:adiar" : "toque: ok");
  if (alertN_ > 1) { char c[8]; StrBuf s2(c, sizeof c); s2.put('+').num(alertN_ - 1); fb_.text(104, 54, c); }
}

void App::drawConfigPages(bool firstUse, ms_t now) {
  (void)now;
  NetStatus ns; plat_.netStatus(ns);
  fb_.text(0, 0, firstUse ? "Primeiro uso" : "Configurar");
  int page = configPage_ % 4;
  char pg[8]; StrBuf sp(pg, sizeof pg); sp.num(page + 1).put("/4");
  fb_.text(128 - Framebuffer::textWidth(pg), 0, pg);
  fb_.hline(0, 9, 128);
  char b[64];
  switch (page) {
    case 0:
      fb_.textCentered(13, "Conecte o celular");
      fb_.textCentered(22, "na rede:");
      fb_.textCentered(33, ns.apSsid, Framebuffer::fitScale(ns.apSsid, 126, 2));
      break;
    case 1:
      fb_.textCentered(15, "Senha da rede:");
      fb_.textCentered(28, ns.apPass, Framebuffer::fitScale(ns.apPass, 126, 2));
      break;
    case 2:
      fb_.textCentered(13, "Abra no navegador:");
      fb_.textCentered(26, ns.apIp[0] ? ns.apIp : "192.168.4.1", Framebuffer::fitScale(ns.apIp[0] ? ns.apIp : "192.168.4.1", 126, 2));
      fb_.textCentered(43, "(funciona sem internet)");
      break;
    default: {
      fb_.textCentered(12, ns.mdns[0] ? ns.mdns : "");
      StrBuf s(b, sizeof b);
      s.put("Casa: ");
      if (!ns.staConfigured) s.put("não usada");
      else if (ns.sta == StaState::Connected) s.put(ns.staIp);
      else if (ns.sta == StaState::Connecting) s.put("conectando");
      else s.put("sem conexão");
      fb_.textCentered(24, b);
      if (ns.apSecondsLeft) { StrBuf s2(b, sizeof b); s2.put("Fecha em "); formatDuration(s2, (ms_t)ns.apSecondsLeft * 1000, false); fb_.textCentered(35, b); }
      if (!ns.apActive) fb_.textCentered(35, "Rede própria fechada");
      break;
    }
  }
  fb_.hline(0, 54, 128);
  fb_.textCentered(56, firstUse ? "segure: usar offline" : "segure: fechar rede");
}

void App::drawInfo(ms_t now) {
  char b[48];
  char pg[8]; StrBuf sp(pg, sizeof pg); sp.num(infoPage_ + 1).put("/6");
  fb_.text(0, 0, "Informações");
  fb_.text(128 - Framebuffer::textWidth(pg), 0, pg);
  fb_.hline(0, 9, 128);
  const Progress& P = testMode_ ? progTest_ : prog_;
  switch (infoPage_) {
    case 0: {
      fb_.textCentered(13, model_ == Model::Lite ? "TubaBot Lite" : "TubaBot");
      StrBuf s(b, sizeof b); s.put("Versão ").put(TB_VERSION); fb_.textCentered(24, b);
      StrBuf s2(b, sizeof b); s2.put("Ligado há "); formatDuration(s2, now - bootAt_, false); fb_.textCentered(35, b);
      if (recovered_) fb_.textCentered(45, "Dados restaurados");
      break;
    }
    case 1: {
      NetStatus ns; plat_.netStatus(ns);
      fb_.text(0, 13, ns.apActive ? "Rede própria: ligada" : "Rede própria: desl.");
      StrBuf s(b, sizeof b); s.put("Casa: ");
      if (!ns.staConfigured) s.put("não usada"); else if (ns.sta == StaState::Connected) s.put(ns.staIp); else s.put("desconectada");
      fb_.text(0, 24, b);
      fb_.text(0, 35, "Funciona offline");
      fb_.textCentered(56, "segure: configurar");
      break;
    }
    case 2:
      if (clock_.valid()) {
        CivilClock::Local L = clock_.local(now, TZ_TABLE[set_.tzIndex].offsetMin);
        StrBuf s(b, sizeof b); s.numPad(L.day, 2).put('/').numPad(L.month, 2).put('/').num(L.year).put(' ').numPad(L.hour, 2).put(':').numPad(L.minute, 2);
        fb_.textCentered(14, b);
        fb_.textCentered(25, TZ_TABLE[set_.tzIndex].label);
      } else {
        fb_.textCentered(14, "Hora não ajustada");
        fb_.textCentered(25, "Ajuste pelo portal");
      }
      fb_.textCentered(40, "Sem bateria de relógio");
      break;
    case 3: {
      SysInfo si; plat_.sysInfo(si);
      StrBuf s(b, sizeof b); s.put("Memória livre ").unum(si.heapFree / 1024).put("K"); fb_.text(0, 13, b);
      StrBuf s2(b, sizeof b); s2.put("Flash livre ").unum(si.sketchFree / 1024).put("K"); fb_.text(0, 24, b);
      fb_.text(0, 35, si.simulated ? "Simulador" : si.chip);
      break;
    }
    case 4: {
      StrBuf s(b, sizeof b); s.put("Toques brutos: ").unum(input_.rawEdges()); fb_.text(0, 13, b);
      fb_.text(0, 24, input_.stuck() ? "Sensor: preso!" : "Sensor: ok");
      fb_.text(0, 35, model_ == Model::Lite ? "Sem som/vibração" : (set_.silent ? "Silencioso" : "Som ativo"));
      break;
    }
    default: {
      StrBuf s(b, sizeof b); s.put("Pula: ").num(P.jumpBest).put("  Bote: ").num(P.fishBest); fb_.text(0, 13, b);
      StrBuf s2(b, sizeof b); s2.put("Reflexo: "); if (P.reflexBestMs) s2.num(P.reflexBestMs).put(" ms"); else s2.put("--"); fb_.text(0, 24, b);
      StrBuf s3(b, sizeof b); s3.put("Tempo: "); if (P.timingBestPct100) s3.fixed2(P.timingBestPct100).put('%'); else s3.put("--"); fb_.text(0, 35, b);
      StrBuf s4(b, sizeof b); s4.put("Carinhos: ").unum(P.pets); fb_.text(0, 46, b);
      break;
    }
  }
}

void App::drawClock(ms_t now) {
  char b[32];
  if (!clock_.valid()) {
    fb_.textCentered(6, "--:--", 3);
    fb_.textCentered(34, "Hora não ajustada");
    fb_.textCentered(45, "Ajuste pelo portal");
    return;
  }
  CivilClock::Local L = clock_.local(now, TZ_TABLE[set_.tzIndex].offsetMin);
  int h = L.hour;
  const char* ampm = nullptr;
  if (!set_.clock24h) { ampm = h < 12 ? "AM" : "PM"; h = h % 12; if (h == 0) h = 12; }
  StrBuf s(b, sizeof b); s.numPad(h, 2).put(':').numPad(L.minute, 2);
  if (clockSeconds_) s.put(':').numPad(L.second, 2);
  fb_.textCentered(8, b, clockSeconds_ ? 2 : 3);
  if (ampm) fb_.text(116, 32, ampm);
  StrBuf s2(b, sizeof b); s2.put(WEEKDAYS[L.weekday]).put(", ").numPad(L.day, 2).put('/').numPad(L.month, 2).put('/').num(L.year);
  fb_.textCentered(42, b);
}

static const char* hintFor(RunState s) {
  switch (s) { case RunState::Running: return "toque: pausar"; case RunState::Paused: return "toque: continuar"; default: return "toque: iniciar"; }
}

void App::drawPomodoro(ms_t now) {
  char b[32];
  const char* ph = pomo_.phase() == PomoPhase::Focus ? "FOCO" : (pomo_.phase() == PomoPhase::ShortBreak ? "PAUSA" : "PAUSA LONGA");
  fb_.text(0, 0, ph);
  StrBuf c(b, sizeof b);
  int cyc = set_.pomoCyclesBeforeLong ? pomo_.focusDone() % set_.pomoCyclesBeforeLong : 0;
  c.put("ciclo ").num(cyc + (pomo_.phase() == PomoPhase::Focus ? 1 : 0)).put('/').num(set_.pomoCyclesBeforeLong);
  fb_.text(128 - Framebuffer::textWidth(b), 0, b);
  StrBuf s(b, sizeof b); formatDuration(s, pomo_.remainingMs(now), true);
  fb_.textCentered(14, b, Framebuffer::fitScale(b, 126, 3));
  fb_.progressBar(4, 40, 120, 8, pomo_.permille(now));
  fb_.textCentered(54, pomo_.state() == RunState::Paused ? "pausado - toque" : hintFor(pomo_.state()));
}

void App::drawTimers(ms_t now) {
  char b[32];
  Countdown& t = timers_[timerSel_];
  StrBuf h(b, sizeof b); h.put("Timer ").num(timerSel_ + 1).put("/").num(kTimers);
  fb_.text(0, 0, b);
  // indicadores dos outros timers
  for (int i = 0; i < kTimers; i++) {
    int x = 104 + i * 8;
    if (timers_[i].state() == RunState::Running) fb_.fillCircle(x, 3, 2); else fb_.circle(x, 3, 2);
  }
  StrBuf s(b, sizeof b); formatDuration(s, t.remainingMs(now), true);
  fb_.textCentered(14, b, Framebuffer::fitScale(b, 126, 3));
  fb_.progressBar(4, 40, 120, 8, t.permilleDone(now));
  fb_.textCentered(54, t.state() == RunState::Done ? "Terminou!" : hintFor(t.state()));
}

void App::drawStopwatch(ms_t now) {
  char b[32];
  fb_.text(0, 0, "Cronômetro");
  uint64_t e = sw_.elapsedMs(now);
  StrBuf s(b, sizeof b);
  uint64_t hh = e / 3600000, mm = (e / 60000) % 60, ss = (e / 1000) % 60, cs = (e / 10) % 100;
  if (hh) s.unum(hh).put(':');
  s.numPad((int64_t)mm, 2).put(':').numPad((int64_t)ss, 2);
  int sc = hh ? 2 : 3;
  int w = Framebuffer::textWidth(b, sc) + 14;
  int x = (128 - w) / 2;
  fb_.text(x, 14, b, sc);
  char c[4]; StrBuf s2(c, sizeof c); s2.put(',').numPad((int64_t)cs, 2);
  fb_.text(x + w - 13, 14 + 7 * sc - 7, c);
  if (sw_.limitHit()) fb_.textCentered(42, "Limite atingido");
  fb_.textCentered(54, hintFor(sw_.state()));
}

void App::drawExpressions(ms_t now) {
  Expr e = (Expr)exprSel_;
  int uid;
  const Progress& P = testMode_ ? progTest_ : prog_;
  bool locked = exprIsUnlockable(e, &uid) && !((P.unlocked >> uid) & 1u);
  if (locked) {
    fb_.textCentered(6, "?", 4);
    fb_.textCentered(40, UNLOCK_CRITERIA[uid]);
  } else {
    FaceDef f = faceDef(e);
    FaceRenderer::drawFace(fb_, f, now);
  }
  fb_.fillRect(0, 54, 128, 10, false);
  fb_.fillRoundRect(0, 54, 128, 10, 2);
  char b[40]; StrBuf s(b, sizeof b);
  s.put(locked ? "Bloqueada" : exprName(e));
  fb_.textCentered(55, b, 1, false);
}

void App::drawResume(ms_t now) {
  (void)now;
  char b[40];
  fb_.textCentered(0, "Sessão interrompida");
  fb_.hline(0, 9, 128);
  int y = 12;
  if (ckpt_.pomoActive) {
    StrBuf s(b, sizeof b); s.put(ckpt_.pomoPhase == 0 ? "Foco " : "Pausa "); formatDuration(s, ckpt_.pomoRemainingMs, true);
    fb_.textCentered(y, b); y += 10;
  }
  for (int i = 0; i < kTimers && y < 34; i++) if (ckpt_.timerActive[i]) {
    StrBuf s(b, sizeof b); s.put("Timer ").num(i + 1).put(' '); formatDuration(s, ckpt_.timerRemainingMs[i], true);
    fb_.textCentered(y, b); y += 10;
  }
  if (ckpt_.swActive && y < 34) { StrBuf s(b, sizeof b); s.put("Cronôm. "); formatDuration(s, ckpt_.swElapsedMs, false); fb_.textCentered(y, b); }
  fb_.textCentered(36, "(tempo desligado");
  fb_.textCentered(45, "não é contado)");
  fb_.textCentered(56, "toque:retomar seg:não");
}

void App::drawConfirm(ms_t now) {
  (void)now;
  fb_.textCentered(10, confirmAction_ == 1 ? "Cancelar Pomodoro?" : "Zerar cronômetro?");
  fb_.textCentered(30, "segure: sim");
  fb_.textCentered(42, "toque: não");
}

void App::render(ms_t now) {
  switch (screen_) {
    case Screen::Intro: drawIntro(now); break;
    case Screen::FirstUse: drawConfigPages(true, now); break;
    case Screen::Config: drawConfigPages(false, now); break;
    case Screen::Face: {
      face_.draw(fb_, now);
      // pequenos indicadores discretos
      bool anyTimer = false;
      for (int i = 0; i < kTimers; i++) anyTimer |= timers_[i].state() == RunState::Running;
      if (anyTimer) { fb_.circle(3, 3, 2); fb_.set(3, 2); fb_.set(3, 3); fb_.set(4, 3); }
      if (pomo_.state() == RunState::Running) fb_.fillRect(124, 1, 3, 3);
      if (model_ == Model::Full && set_.silent) { fb_.hline(118, 60, 6); }
      break;
    }
    case Screen::Menu: drawList("Menu", now); break;
    case Screen::GamesMenu: drawList("Jogos", now); break;
    case Screen::Actions: drawList("Opções", now); break;
    case Screen::Clock: drawClock(now); break;
    case Screen::Pomodoro: drawPomodoro(now); break;
    case Screen::Timers: drawTimers(now); break;
    case Screen::Stopwatch: drawStopwatch(now); break;
    case Screen::Game: if (game_) game_->draw(fb_, now); break;
    case Screen::Expressions: drawExpressions(now); break;
    case Screen::Info: drawInfo(now); break;
    case Screen::Confirm: drawConfirm(now); break;
    case Screen::RecoveryConfirm:
      fb_.textCentered(6, "Abrir configuração?");
      fb_.textCentered(22, "toque: confirmar");
      fb_.textCentered(32, "2x ou segure: não");
      fb_.progressBar(14, 46, 100, 6, (int)(recoveryUntil_ > now ? (recoveryUntil_ - now) * 1000 / 6000 : 0));
      break;
    case Screen::Resume: drawResume(now); break;
    case Screen::ScreenOff: break;  // visor apagado (timers seguem)
    case Screen::Ota:
      fb_.textCentered(8, "Atualizando...");
      fb_.progressBar(10, 26, 108, 10, otaPermille_);
      fb_.textCentered(44, "Não desligue");
      break;
  }
  if (diagUntil_ > now) {
    // padrão de teste: borda, xadrez e cantos (verifica orientação e pixels mortos)
    fb_.clear();
    fb_.rect(0, 0, 128, 64);
    for (int y = 12; y < 52; y += 4) for (int x = 4; x < 60; x += 4) if (((x + y) / 4) & 1) fb_.fillRect(x, y, 4, 4);
    fb_.fillRect(1, 1, 6, 6);
    fb_.text(64, 14, "TESTE");
    fb_.text(64, 26, "128x64");
    fb_.text(64, 38, "canto >");
    fb_.fillRect(121, 57, 6, 6);
    return;
  }
  if (alertN_) drawAlert(now);
  else if (screen_ != Screen::ScreenOff && screen_ != Screen::Ota) drawHoldHint(now);
  if (toastUntil_ > now && screen_ != Screen::ScreenOff) {
    int w = Framebuffer::textWidth(toast_) + 8;
    int x = (128 - w) / 2;
    fb_.fillRoundRect(x, 0, w, 11, 3, false);
    fb_.fillRoundRect(x, 0, w, 10, 3);
    fb_.text(x + 4, 1, toast_, 1, false);
  }
}

}  // namespace tb
