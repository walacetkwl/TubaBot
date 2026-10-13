// API local (JSON), idêntica no ESP32 e no simulador.
// Rotas documentadas em docs/API.md.
#include "app.h"

namespace tb {

static const size_t kMaxBody = 4096;

static void b64(StrBuf& sb, const uint8_t* d, size_t n) {
  static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  for (size_t i = 0; i < n; i += 3) {
    uint32_t v = (uint32_t)d[i] << 16 | (i + 1 < n ? (uint32_t)d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
    sb.put(T[(v >> 18) & 63]).put(T[(v >> 12) & 63]).put(i + 1 < n ? T[(v >> 6) & 63] : '=').put(i + 2 < n ? T[v & 63] : '=');
  }
}

static const char* runStateName(RunState s) {
  switch (s) { case RunState::Running: return "running"; case RunState::Paused: return "paused"; case RunState::Done: return "done"; default: return "idle"; }
}

static int errorJson(StrBuf& out, int code, const char* msg) {
  JsonOut j(out); j.beginObj().kbool("ok", false).kstr("error", msg).endObj();
  return code;
}

void App::writePrefs(JsonOut& j) {
  j.beginObj("prefs");
  j.kbool("setupDone", set_.setupDone);
  j.kstr("nickname", set_.nickname);
  j.kstr("personality", set_.personality ? "brincalhao" : "tranquilo");
  j.kint("contrast", set_.contrast);
  j.kbool("rotate180", set_.rotate180);
  j.kstr("favoriteExpr", exprKey((Expr)set_.favoriteExpr));
  j.kint("animLevel", set_.animLevel);
  j.kint("screenOffS", set_.screenOffS);
  j.kbool("shortIntro", set_.shortIntro);
  j.kbool("sound", set_.sound); j.kbool("silent", set_.silent); j.kbool("vibration", set_.vibration);
  j.beginObj("pomodoro")
      .kint("focusMin", set_.pomoFocusMin).kint("shortMin", set_.pomoShortMin).kint("longMin", set_.pomoLongMin)
      .kint("cyclesBeforeLong", set_.pomoCyclesBeforeLong).kbool("autoStart", set_.pomoAutoStart).endObj();
  j.beginArr("timerPresetsS");
  for (int i = 0; i < TIMER_PRESET_COUNT; i++) j.vint(set_.timerPresetS[i]);
  j.endArr();
  j.beginArr("reminders");
  for (int i = 0; i < REMINDER_COUNT; i++)
    j.beginObj().kstr("label", set_.reminders[i].label).kint("intervalMin", set_.reminders[i].intervalMin).kbool("enabled", set_.reminders[i].enabled).endObj();
  j.endArr();
  j.beginObj("quiet").kbool("enabled", set_.quietEnabled).kint("startH", set_.quietStartH).kint("endH", set_.quietEndH).endObj();
  j.kstr("tz", TZ_TABLE[set_.tzIndex].iana);
  j.kbool("clock24h", set_.clock24h);
  j.kbool("jumpEasy", set_.jumpEasy);
  j.kint("apTimeoutMin", set_.apTimeoutMin);
  j.kbool("weatherEnabled", set_.weatherEnabled);
  j.kbool("ntpEnabled", set_.ntpEnabled);
  j.endObj();
}

void App::writeProgress(JsonOut& j) {
  const Progress& P = testMode_ ? progTest_ : prog_;
  j.beginObj("progress");
  j.kint("unlocked", P.unlocked).kint("pets", P.pets).kint("pomodorosDone", P.pomodorosDone).kint("reflexValid", P.reflexValid);
  j.kint("jumpBest", P.jumpBest).kint("fishBest", P.fishBest).kint("reflexBestMs", P.reflexBestMs);
  j.kint("timingBestPct100", P.timingBestPct100).kint("timingBestErrMs", P.timingBestErrMs).kint("timingBestTargetCs", P.timingBestTargetCs);
  j.kint("gamesPlayed", P.gamesPlayed).kint("mood", P.mood).kint("bond", P.bond);
  j.kbool("testProfile", testMode_);
  j.endObj();
}

// Valida e (se !dryRun) aplica. Campos ausentes ficam como estão.
int App::applyPrefsJson(const JsonVal& v, JsonOut& err, int& errCount, ms_t now, bool dryRun) {
  Settings s = set_;
  auto fail = [&](const char* field, const char* msg) { errCount++; err.beginObj().kstr("field", field).kstr("msg", msg).endObj(); };
  auto getBool = [&](const JsonVal& o, const char* k, uint8_t& dst) {
    JsonVal x = o.get(k); if (!x.ok()) return; bool b; if (!x.asBool(b)) { fail(k, "use verdadeiro/falso"); return; } dst = b ? 1 : 0; };
  auto getInt = [&](const JsonVal& o, const char* k, int64_t lo, int64_t hi, int64_t& dst) -> bool {
    JsonVal x = o.get(k); if (!x.ok()) return false; int64_t n;
    if (!x.asInt(n) || n < lo || n > hi) { char m[48]; StrBuf sb(m, sizeof m); sb.put("valor entre ").num(lo).put(" e ").num(hi); fail(k, m); return false; }
    dst = n; return true; };
  int64_t n;
  JsonVal x = v.get("nickname");
  if (x.ok()) {
    char tmp[NICK_MAX_BYTES + 8];
    if (!x.asString(tmp, sizeof tmp)) fail("nickname", "apelido inválido ou longo demais");
    else {
      // apara espaços nas pontas
      char* a = tmp; while (*a == ' ') a++;
      size_t l = tbStrlen(a); while (l && a[l - 1] == ' ') a[--l] = 0;
      int c = utf8Count(a, l);
      bool ctrl = false; for (size_t i = 0; i < l; i++) if ((uint8_t)a[i] < 0x20) ctrl = true;
      if (c < 0 || ctrl) fail("nickname", "caracteres inválidos");
      else if (c > NICK_MAX_CHARS) fail("nickname", "máximo de 12 caracteres");
      else if (Framebuffer::textWidth(a) > 124) fail("nickname", "não cabe no visor");
      else tbStrlcpy(s.nickname, a, sizeof s.nickname);
    }
  }
  x = v.get("personality");
  if (x.ok()) {
    char p[16];
    if (x.asString(p, sizeof p) && tbStreq(p, "tranquilo")) s.personality = 0;
    else if (x.asString(p, sizeof p) && tbStreq(p, "brincalhao")) s.personality = 1;
    else fail("personality", "use tranquilo ou brincalhao");
  }
  if (getInt(v, "contrast", 1, 255, n)) s.contrast = (uint8_t)n;
  getBool(v, "rotate180", s.rotate180);
  x = v.get("favoriteExpr");
  if (x.ok()) {
    char k[24]; Expr e = Expr::Count;
    if (x.asString(k, sizeof k)) e = exprFromKey(k);
    Expr av[32]; int na = availableExprs(av, 32); bool okE = false;
    for (int i = 0; i < na; i++) if (av[i] == e) okE = true;
    if (!okE) fail("favoriteExpr", "expressão indisponível"); else s.favoriteExpr = (uint8_t)e;
  }
  if (getInt(v, "animLevel", 0, 2, n)) s.animLevel = (uint8_t)n;
  x = v.get("screenOffS");
  if (x.ok()) { if (!x.asInt(n) || !(n == 0 || (n >= 30 && n <= 3600))) fail("screenOffS", "0 (nunca) ou 30 a 3600 s"); else s.screenOffS = (uint16_t)n; }
  getBool(v, "shortIntro", s.shortIntro);
  getBool(v, "sound", s.sound); getBool(v, "silent", s.silent); getBool(v, "vibration", s.vibration);
  JsonVal po = v.get("pomodoro");
  if (po.isObject()) {
    if (getInt(po, "focusMin", 1, 120, n)) s.pomoFocusMin = (uint8_t)n;
    if (getInt(po, "shortMin", 1, 60, n)) s.pomoShortMin = (uint8_t)n;
    if (getInt(po, "longMin", 1, 90, n)) s.pomoLongMin = (uint8_t)n;
    if (getInt(po, "cyclesBeforeLong", 1, 12, n)) s.pomoCyclesBeforeLong = (uint8_t)n;
    getBool(po, "autoStart", s.pomoAutoStart);
  }
  JsonVal tp = v.get("timerPresetsS");
  if (tp.ok()) {
    if (!tp.isArray() || tp.arrayLen() != TIMER_PRESET_COUNT) fail("timerPresetsS", "lista de 6 durações");
    else for (int i = 0; i < TIMER_PRESET_COUNT; i++) {
      if (!tp.at(i).asInt(n) || n < 1 || n > 86399) { fail("timerPresetsS", "de 1 s a 23h59min59s"); break; }
      s.timerPresetS[i] = (uint32_t)n;
    }
  }
  JsonVal rm = v.get("reminders");
  if (rm.ok()) {
    if (!rm.isArray() || rm.arrayLen() > REMINDER_COUNT) fail("reminders", "até 3 lembretes");
    else for (int i = 0; i < rm.arrayLen(); i++) {
      JsonVal r = rm.at(i);
      JsonVal lb = r.get("label");
      if (lb.ok()) {
        char tmp[REMINDER_LABEL_BYTES];
        int c = lb.asString(tmp, sizeof tmp) ? utf8Count(tmp, tbStrlen(tmp)) : -1;
        if (c < 1 || c > 16) fail("reminders.label", "rótulo de 1 a 16 caracteres");
        else tbStrlcpy(s.reminders[i].label, tmp, sizeof s.reminders[i].label);
      }
      if (getInt(r, "intervalMin", 5, 240, n)) s.reminders[i].intervalMin = (uint16_t)n;
      getBool(r, "enabled", s.reminders[i].enabled);
    }
  }
  JsonVal q = v.get("quiet");
  if (q.isObject()) {
    getBool(q, "enabled", s.quietEnabled);
    if (getInt(q, "startH", 0, 23, n)) s.quietStartH = (uint8_t)n;
    if (getInt(q, "endH", 0, 23, n)) s.quietEndH = (uint8_t)n;
  }
  x = v.get("tz");
  if (x.ok()) {
    char tz[40]; int idx = x.asString(tz, sizeof tz) ? tzIndexFromIana(tz) : -1;
    if (idx < 0) fail("tz", "fuso não suportado"); else s.tzIndex = (uint8_t)idx;
  }
  getBool(v, "clock24h", s.clock24h);
  getBool(v, "jumpEasy", s.jumpEasy);
  if (getInt(v, "apTimeoutMin", 0, 120, n)) s.apTimeoutMin = (uint16_t)n;
  getBool(v, "weatherEnabled", s.weatherEnabled);
  getBool(v, "ntpEnabled", s.ntpEnabled);
  getBool(v, "setupDone", s.setupDone);
  if (errCount || dryRun) return errCount;
  bool reminderChanged = false;
  for (int i = 0; i < REMINDER_COUNT; i++)
    if (s.reminders[i].intervalMin != set_.reminders[i].intervalMin || s.reminders[i].enabled != set_.reminders[i].enabled) reminderChanged = true;
  bool favChanged = s.favoriteExpr != set_.favoriteExpr;
  set_ = s;
  sanitizeSettings(set_);
  applyPrefs(now);  // sem reiniciar; som/vibração desligados cortam a saída em andamento
  if (reminderChanged) for (int i = 0; i < REMINDER_COUNT; i++) reminderDue_[i] = now + (ms_t)set_.reminders[i].intervalMin * 60000;
  if (favChanged) { baseExpr_ = (Expr)set_.favoriteExpr; if (screen_ == Screen::Face) face_.setExpression(baseExpr_, now); }
  markSettings(now);
  forceFrame_ = true;
  return 0;
}

int App::apiStatus(JsonOut& j, ms_t now) {
  j.kbool("ok", true);
  j.kstr("model", model_ == Model::Lite ? "lite" : "full");
  j.kstr("modelName", model_ == Model::Lite ? "TubaBot Lite" : "TubaBot");
  j.kstr("version", TB_VERSION);
  j.kstr("nickname", set_.nickname);
  j.kstr("personality", set_.personality ? "brincalhao" : "tranquilo");
  j.kstr("screen", screenName(screen_));
  j.kstr("expression", exprKey(face_.expression()));
  j.kstr("expressionName", exprName(face_.expression()));
  j.kint("uptimeMs", (int64_t)(now - bootAt_));
  j.kbool("setupDone", set_.setupDone);
  j.beginObj("time");
  j.kbool("valid", clock_.valid());
  if (clock_.valid()) {
    j.kint("epoch", clock_.epoch(now));
    CivilClock::Local L = clock_.local(now, TZ_TABLE[set_.tzIndex].offsetMin);
    char b[24]; StrBuf s(b, sizeof b);
    s.numPad(L.day, 2).put('/').numPad(L.month, 2).put('/').num(L.year).put(' ').numPad(L.hour, 2).put(':').numPad(L.minute, 2);
    j.kstr("local", b);
  }
  j.kstr("tz", TZ_TABLE[set_.tzIndex].iana);
  j.endObj();
  NetStatus ns; plat_.netStatus(ns);
  j.beginObj("net").kbool("apActive", ns.apActive).kstr("apSsid", ns.apSsid).kstr("apIp", ns.apIp).kstr("mdns", ns.mdns)
      .kint("apSecondsLeft", ns.apSecondsLeft).kbool("homeConfigured", ns.staConfigured)
      .kstr("home", ns.sta == StaState::Connected ? "connected" : ns.sta == StaState::Connecting ? "connecting" : ns.sta == StaState::Failed ? "failed" : "off")
      .kstr("homeSsid", ns.staSsid).kstr("homeIp", ns.staIp).endObj();
  j.beginObj("outputs").kbool("soundAllowed", AlertPolicy::soundAllowed(model_, out_.prefs()))
      .kbool("vibrationAllowed", AlertPolicy::vibeAllowed(model_, out_.prefs()))
      .kbool("buzzer", out_.buzzer()).kbool("motor", out_.motor()).kstr("pattern", patternName(out_.current())).endObj();
  j.beginObj("pomodoro").kstr("state", runStateName(pomo_.state()))
      .kstr("phase", pomo_.phase() == PomoPhase::Focus ? "focus" : pomo_.phase() == PomoPhase::ShortBreak ? "short" : "long")
      .kint("remainingMs", pomo_.remainingMs(now)).kint("focusDone", pomo_.focusDone()).endObj();
  j.beginArr("timers");
  for (int i = 0; i < kTimers; i++)
    j.beginObj().kstr("state", runStateName(timers_[i].state())).kint("remainingMs", timers_[i].remainingMs(now)).kint("durationMs", timers_[i].durationMs()).endObj();
  j.endArr();
  j.beginObj("stopwatch").kstr("state", runStateName(sw_.state())).kint("elapsedMs", (int64_t)sw_.elapsedMs(now)).kbool("limit", sw_.limitHit()).endObj();
  j.kint("alerts", alertN_);
  j.beginObj("input").kbool("pressed", input_.pressed()).kbool("stuck", input_.stuck()).kint("rawEdges", input_.rawEdges()).endObj();
  j.beginObj("storage").kbool("recovered", recovered_).kint("writes", writes_).endObj();
  j.beginObj("event").kint("seq", eventSeq_).kstr("last", lastEvent_).endObj();
  return 200;
}

int App::apiAction(const JsonVal& v, JsonOut& j, ms_t now, bool devTools) {
  char a[32];
  if (!v.get("action").asString(a, sizeof a)) return -1;
  int64_t n = 0;
  if (tbStreq(a, "pomodoro.toggle")) { pomo_.toggle(now); writeCheckpoint(now); }
  else if (tbStreq(a, "pomodoro.skip")) { pomo_.skip(now); writeCheckpoint(now); }
  else if (tbStreq(a, "pomodoro.cancel")) { pomo_.cancel(); writeCheckpoint(now); }
  else if (tbStrStarts(a, "timer.")) {
    if (!v.get("slot").asInt(n) || n < 0 || n >= kTimers) return -2;
    Countdown& t = timers_[n];
    if (tbStreq(a, "timer.start")) {
      int64_t sec = 0;
      if (v.get("seconds").ok()) {
        if (!v.get("seconds").asInt(sec) || sec < 1 || sec > 86399) return -3;
        t.cancel(); t.set((uint32_t)sec * 1000);
      }
      t.cancel(); t.start(now);
    } else if (tbStreq(a, "timer.pause")) t.pause(now);
    else if (tbStreq(a, "timer.resume")) t.resume(now);
    else if (tbStreq(a, "timer.cancel")) t.cancel();
    else return -1;
    writeCheckpoint(now);
  }
  else if (tbStreq(a, "stopwatch.toggle")) { sw_.toggle(now); writeCheckpoint(now); }
  else if (tbStreq(a, "stopwatch.reset")) { sw_.reset(); writeCheckpoint(now); }
  else if (tbStreq(a, "alert.ack")) ackAlert(now, false);
  else if (tbStreq(a, "face")) {
    char k[24]; if (!v.get("expr").asString(k, sizeof k)) return -2;
    Expr e = exprFromKey(k);
    Expr av[32]; int na = availableExprs(av, 32); bool ok = false;
    for (int i = 0; i < na; i++) if (av[i] == e) ok = true;
    if (!ok) return -4;
    baseExpr_ = e; exprUntil_ = 0; nextIdleExpr_ = now + 120000;
    if (screen_ == Screen::Face) face_.setExpression(e, now);
  }
  else if (tbStreq(a, "preview")) {
    // Prévia obedece ao perfil físico e às preferências (não burla o silencioso)
    char k[16]; v.get("pattern").asString(k, sizeof k);
    Pattern p = patternFromName(k);
    if (p == Pattern::None) return -2;
    out_.play(p, now);
    j.kbool("soundAllowed", AlertPolicy::soundAllowed(model_, out_.prefs()));
    j.kbool("vibrationAllowed", AlertPolicy::vibeAllowed(model_, out_.prefs()));
  }
  else if (tbStreq(a, "diag.oled")) { diagUntil_ = now + 3000; event("teste do visor"); }
  else if (tbStreq(a, "config.open")) openConfig(now);
  else if (tbStreq(a, "ap.close")) { plat_.apClose(); }
  else if (tbStreq(a, "setup.offline")) { set_.setupDone = 1; markSettings(now); if (screen_ == Screen::FirstUse) gotoScreen(Screen::Face, now); }
  else if (tbStreq(a, "screen.wake")) { if (screen_ == Screen::ScreenOff) wake(now); else noteInteraction(now); }
  else if (tbStreq(a, "reboot")) { flush(now, true); plat_.requestReboot(); }
  else if (devTools && devBuild_ && tbStreq(a, "dev.unlockAll")) {
    // desbloqueio de TESTE: perfil separado, nunca gravado
    if (!testMode_) { progTest_ = prog_; testMode_ = true; }
    progTest_.unlocked = (1u << UNL_COUNT) - 1;
  }
  else if (devTools && devBuild_ && tbStreq(a, "dev.testProfileOff")) { testMode_ = false; }
  else return -1;
  forceFrame_ = true;
  return 0;
}

int App::api(const char* method, const char* path, const char* body, size_t bodyLen, const ApiCtx& ctx, StrBuf& out, ms_t now) {
  bool isGet = tbStreq(method, "GET"), isPost = tbStreq(method, "POST");
  if (bodyLen > kMaxBody) return errorJson(out, 413, "corpo grande demais");
  JsonVal root;
  if (isPost && bodyLen) { if (!jsonParse(body, bodyLen, root) || !root.isObject()) return errorJson(out, 400, "JSON inválido"); }
  else root.p = nullptr;
  auto authed = [&]() { return ctx.token && tbStreq(ctx.token, token_); };

  // Sessão: na rede própria do Tuba basta pedir; na rede de casa exige o PIN mostrado no visor.
  if (tbStreq(path, "/api/session")) {
    if (isGet) { JsonOut j(out); j.beginObj().kbool("ok", true).kbool("needPin", !ctx.viaAP).endObj(); return 200; }
    if (!isPost) return errorJson(out, 405, "método");
    if (!ctx.viaAP) {
      char pin[12] = {0};
      if (!root.ok() || !root.get("pin").asString(pin, sizeof pin) || !tbStreq(pin, pin_)) {
        toast(pin_, now, 8000);  // mostra o PIN no visor para quem está fisicamente perto
        return errorJson(out, 401, "PIN incorreto - veja o visor do TubaBot");
      }
    }
    JsonOut j(out); j.beginObj().kbool("ok", true).kstr("token", token_).endObj();
    return 200;
  }
  if (tbStreq(path, "/api/status") && isGet) { JsonOut j(out); j.beginObj(); int c = apiStatus(j, now); j.endObj(); return c; }
  if (tbStreq(path, "/api/frame") && isGet) {
    JsonOut j(out); j.beginObj().kint("w", FB_W).kint("h", FB_H).kstr("layout", "ssd1306-pages");
    out.put(",\"data\":\""); b64(out, prev_.buf, FB_BYTES); out.put('"'); j.endObj();
    return 200;
  }
  if (tbStreq(path, "/api/prefs") && isGet) { JsonOut j(out); j.beginObj().kbool("ok", true); writePrefs(j); j.endObj(); return 200; }
  if (tbStreq(path, "/api/records") && isGet) { JsonOut j(out); j.beginObj().kbool("ok", true); writeProgress(j); j.endObj(); return 200; }
  if (tbStreq(path, "/api/collection") && isGet) {
    static const char* const CRIT[UNL_COUNT] = {"Primeiro carinho concluído", "Primeiro Pomodoro completo", "Passar 10 obstáculos em uma partida de Pula, Tuba!",
                                               "Capturar 5 peixes em uma partida de Hora do Bote", "Concluir 5 rodadas válidas de Tuba Reflexo", "Fazer 98% ou mais em No Tempo Certo"};
    const Progress& P = testMode_ ? progTest_ : prog_;
    JsonOut j(out); j.beginObj().kbool("ok", true).kbool("testProfile", testMode_).beginArr("base");
    static const Expr base[] = {Expr::Happy, Expr::Neutral, Expr::Surprised, Expr::Love, Expr::Sad, Expr::Sleepy, Expr::Angry, Expr::WinkL, Expr::WinkR, Expr::Pet, Expr::Dead};
    for (Expr e : base) j.beginObj().kstr("key", exprKey(e)).kstr("name", exprName(e)).endObj();
    j.endArr().beginArr("unlockables");
    for (int i = 0; i < UNL_COUNT; i++) {
      j.beginObj().kstr("key", exprKey(exprForUnlock(i))).kstr("name", exprName(exprForUnlock(i))).kstr("criterion", CRIT[i])
          .kbool("unlocked", (P.unlocked >> i) & 1u);
      int64_t cur = 0, goal = 1;
      switch (i) {
        case UNL_TIMIDO: cur = P.pets; break; case UNL_FOCADO: cur = P.pomodorosDone; break;
        case UNL_VITORIOSO: cur = P.jumpBest; goal = 10; break; case UNL_SATISFEITO: cur = P.fishBest; goal = 5; break;
        case UNL_ESPANTADO: cur = P.reflexValid; goal = 5; break; default: cur = P.timingBestPct100; goal = 9800;
      }
      j.kint("current", tbMin(cur, goal)).kint("goal", goal).endObj();
    }
    j.endArr().endObj();
    return 200;
  }
  if (tbStreq(path, "/api/ota") && isGet) {
    const char* r = nullptr; bool ok = otaAllowed(&r);
    JsonOut j(out); j.beginObj().kbool("ok", true).kbool("allowed", ok); if (r) j.kstr("reason", r); j.endObj();
    return 200;
  }
  if (tbStreq(path, "/api/wifi") && isGet) {
    NetStatus ns; plat_.netStatus(ns);
    JsonOut j(out); j.beginObj().kbool("ok", true).kbool("configured", ns.staConfigured).kstr("ssid", ns.staSsid)
        .kstr("state", ns.sta == StaState::Connected ? "connected" : ns.sta == StaState::Connecting ? "connecting" : ns.sta == StaState::Failed ? "failed" : "off")
        .kstr("ip", ns.staIp).endObj();  // a senha NUNCA é devolvida
    return 200;
  }
  if (tbStreq(path, "/api/tz") && isGet) {
    JsonOut j(out); j.beginObj().kbool("ok", true).beginArr("zones");
    for (int i = 0; i < TZ_COUNT; i++) j.beginObj().kstr("iana", TZ_TABLE[i].iana).kstr("label", TZ_TABLE[i].label).endObj();
    j.endArr().endObj();
    return 200;
  }

  // ── a partir daqui: alterações exigem token ──
  if (!isPost) return errorJson(out, isGet ? 404 : 405, isGet ? "rota desconhecida" : "método");
  if (!authed()) return errorJson(out, 401, "sessão inválida");
  if (!root.ok()) return errorJson(out, 400, "corpo JSON obrigatório");
  noteInteraction(now);

  if (tbStreq(path, "/api/prefs")) {
    char eb[1024]; StrBuf es(eb, sizeof eb); JsonOut ej(es); ej.beginArr(nullptr);
    int ec = 0;
    applyPrefsJson(root, ej, ec, now, true);   // valida tudo antes (tudo ou nada)
    ej.endArr();
    if (ec) { JsonOut j(out); j.beginObj().kbool("ok", false).kstr("error", "validação").kraw("errors", eb).endObj(); return 422; }
    JsonOut dummy(es); int ec2 = 0;
    applyPrefsJson(root, dummy, ec2, now, false);
    JsonOut j(out); j.beginObj().kbool("ok", true); writePrefs(j); j.endObj();
    return 200;
  }
  if (tbStreq(path, "/api/time")) {
    int64_t ep; char tz[40];
    if (!root.get("epoch").asInt(ep) || ep < 1577836800 || ep > 4102444800) return errorJson(out, 422, "epoch inválido");
    if (root.get("tz").ok()) {
      int idx = root.get("tz").asString(tz, sizeof tz) ? tzIndexFromIana(tz) : -1;
      if (idx < 0) return errorJson(out, 422, "fuso não suportado");
      if (idx != set_.tzIndex) { set_.tzIndex = (uint8_t)idx; applyPrefs(now); markSettings(now); }
    }
    setTime(ep, now);
    JsonOut j(out); j.beginObj(); apiStatus(j, now); j.endObj();
    return 200;
  }
  if (tbStreq(path, "/api/action")) {
    JsonOut j(out); j.beginObj();
    int r = apiAction(root, j, now, ctx.devTools);
    if (r < 0) { out.len = 0; out.buf[0] = 0; return errorJson(out, 422, r == -1 ? "ação desconhecida" : r == -4 ? "expressão bloqueada" : "parâmetro inválido"); }
    j.kbool("ok", true).endObj();
    return 200;
  }
  if (tbStreq(path, "/api/backup")) {
    JsonOut j(out); j.beginObj().kbool("ok", true).kstr("format", "tubabot-backup").kint("version", 1).kstr("fw", TB_VERSION);
    writePrefs(j); writeProgress(j); j.endObj();
    return 200;
  }
  if (tbStreq(path, "/api/restore")) {
    JsonVal fmt = root.get("format"); char f[24];
    if (!fmt.asString(f, sizeof f) || !tbStreq(f, "tubabot-backup")) return errorJson(out, 422, "arquivo não é um backup do TubaBot");
    JsonVal pr = root.get("prefs");
    if (!pr.isObject()) return errorJson(out, 422, "backup sem preferências");
    char eb[1024]; StrBuf es(eb, sizeof eb); JsonOut ej(es); ej.beginArr(nullptr); int ec = 0;
    applyPrefsJson(pr, ej, ec, now, true); ej.endArr();
    if (ec) { JsonOut j(out); j.beginObj().kbool("ok", false).kstr("error", "validação").kraw("errors", eb).endObj(); return 422; }
    JsonOut d2(es); int e2 = 0; applyPrefsJson(pr, d2, e2, now, false);
    JsonVal pg = root.get("progress");
    if (pg.isObject() && !testMode_) {
      // progresso só aumenta com importação (nunca apaga conquistas existentes)
      int64_t n;
      if (pg.get("unlocked").asInt(n) && n >= 0) prog_.unlocked |= (uint32_t)n & ((1u << UNL_COUNT) - 1);
      if (pg.get("pets").asInt(n) && n > (int64_t)prog_.pets && n < 100000000) prog_.pets = (uint32_t)n;
      if (pg.get("pomodorosDone").asInt(n) && n > (int64_t)prog_.pomodorosDone && n < 1000000) prog_.pomodorosDone = (uint32_t)n;
      if (pg.get("jumpBest").asInt(n) && n > prog_.jumpBest && n < 65536) prog_.jumpBest = (uint16_t)n;
      if (pg.get("fishBest").asInt(n) && n > prog_.fishBest && n < 65536) prog_.fishBest = (uint16_t)n;
      markProgress(now);
    }
    JsonOut j(out); j.beginObj().kbool("ok", true).endObj();
    return 200;
  }
  if (tbStreq(path, "/api/reset")) {
    char scope[12] = {0}, confirm[24] = {0};
    root.get("scope").asString(scope, sizeof scope);
    root.get("confirm").asString(confirm, sizeof confirm);
    if (tbStreq(scope, "wifi")) {
      if (!tbStreq(confirm, "ESQUECER")) return errorJson(out, 422, "confirme com ESQUECER");
      forgetWifi();
    } else if (tbStreq(scope, "prefs")) {
      if (!tbStreq(confirm, "REDEFINIR")) return errorJson(out, 422, "confirme com REDEFINIR");
      resetPreferences(now);
    } else if (tbStreq(scope, "all")) {
      if (!tbStreq(confirm, "APAGAR TUDO")) return errorJson(out, 422, "confirme com APAGAR TUDO");
      eraseAll(now);
    } else return errorJson(out, 422, "escopo: wifi, prefs ou all");
    JsonOut j(out); j.beginObj().kbool("ok", true).kstr("scope", scope).endObj();
    return 200;
  }
  if (tbStreq(path, "/api/wifi")) {
    char ssid[33], pass[65];
    if (!root.get("ssid").asString(ssid, sizeof ssid) || tbStrlen(ssid) == 0) return errorJson(out, 422, "nome da rede (1 a 32 bytes)");
    if (!root.get("pass").asString(pass, sizeof pass)) pass[0] = 0;
    size_t pl = tbStrlen(pass);
    if (pl != 0 && (pl < 8 || pl > 63)) return errorJson(out, 422, "senha WPA de 8 a 63 caracteres (ou vazia)");
    bool ok = plat_.wifiSet(ssid, pass);
    tbMemset(pass, 0, sizeof pass);
    if (!ok) return errorJson(out, 500, "não foi possível salvar");
    event("rede doméstica configurada");
    JsonOut j(out); j.beginObj().kbool("ok", true).endObj();
    return 200;
  }
  return errorJson(out, 404, "rota desconhecida");
}

}  // namespace tb
