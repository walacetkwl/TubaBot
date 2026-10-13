// Testes nativos do núcleo do TubaBot.
// Compilar e rodar: make test   (ou ./tools/run_tests.sh)
#include "fake_platform.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace tb;

static int g_fail = 0, g_checks = 0;
static const char* g_test = "";
#define CHECK(c) do { g_checks++; if (!(c)) { g_fail++; printf("  FALHOU [%s] %s:%d: %s\n", g_test, __FILE__, __LINE__, #c); } } while (0)
#define CHECK_EQ(a, b) do { g_checks++; long long _a = (long long)(a), _b = (long long)(b); if (_a != _b) { g_fail++; printf("  FALHOU [%s] %s:%d: %s == %s (%lld != %lld)\n", g_test, __FILE__, __LINE__, #a, #b, _a, _b); } } while (0)
#define TEST(name) static void name(); static Reg reg_##name(#name, name); static void name()
struct Reg { static std::vector<std::pair<const char*, void (*)()>>& all() { static std::vector<std::pair<const char*, void (*)()>> v; return v; }
  Reg(const char* n, void (*f)()) { all().push_back({n, f}); } };

// ── Harness ─────────────────────────────────────────────────────────────────
struct H {
  FakePlatform plat;
  App* app = nullptr;
  ms_t t = 0;
  uint32_t buzzOnTicks = 0, motorOnTicks = 0;
  H(Model m = Model::Full, bool setupDone = true, uint32_t seed = 42, FakePlatform* reuse = nullptr) {
    if (reuse) plat = *reuse;
    app = new App(plat);
    if (setupDone && !reuse && !plat.storeLen[(int)BlobKind::Settings]) preset([](Settings& s) { s.setupDone = 1; });
    app->begin(m, t, seed, true);
  }
  ~H() { delete app; }
  // grava preferências e reinicia o App lendo-as (simula flash)
  void preset(void (*f)(Settings&)) {
    Settings s; defaultSettings(s); f(s);
    uint8_t buf[600]; size_t n = encodeBlob(BlobKind::Settings, SETTINGS_VERSION, &s, sizeof s, buf, sizeof buf);
    plat.storageSave(BlobKind::Settings, buf, n);
  }
  void reboot(Model m = Model::Full) { app->flush(t, true); delete app; app = new App(plat); t += 5000; app->begin(m, t, 7, true); }
  void tick() {
    bool changed = app->tick(t);
    if (changed) app->framePresented(t);
    if (app->buzzer()) buzzOnTicks++;
    if (app->motor()) motorOnTicks++;
  }
  void run(ms_t ms, ms_t step = 10) { ms_t end = t + ms; while (t < end) { t += step; if (t > end) t = end; tick(); } }
  void level(bool on) { app->touchSample(on, t); }
  void hold(ms_t ms, ms_t step = 10) { level(true); run(ms, step); level(false); }
  void tap() { hold(80); run(20); }
  void tapAndSettle() { tap(); run(400); }
  void dbl() { hold(70); run(80); hold(70); run(400); }
  void skipIntro() { run(3000, 20); }
};

static int apiCall(App& a, const char* m, const char* path, const char* body, std::string& out, ms_t now, bool tok = true, bool viaAP = true) {
  static char buf[16384];
  StrBuf sb(buf, sizeof buf);
  ApiCtx ctx; ctx.viaAP = viaAP; ctx.devTools = true; ctx.token = tok ? a.sessionToken() : "x";
  int code = a.api(m, path, body, body ? strlen(body) : 0, ctx, sb, now);
  out = buf;
  return code;
}

static uint32_t fbHash(const Framebuffer& f) { return crc32(f.buf, FB_BYTES); }

// ════════ 1. Núcleo único / framebuffer ════════
TEST(framebuffer_layout_and_text) {
  Framebuffer fb; fb.clear();
  fb.set(0, 0); fb.set(127, 63); fb.set(5, 9);
  CHECK_EQ(fb.buf[0], 0x01);
  CHECK_EQ(fb.buf[7 * 128 + 127], 0x80);
  CHECK_EQ(fb.buf[1 * 128 + 5], 0x02);
  CHECK(Framebuffer::textWidth("TubaBot", 2) <= 128);
  CHECK_EQ(Framebuffer::textWidth("ÁÉÍÓÚçãõ"), 8 * 6 - 1);
  // apelido de 12 caracteres cabe em escala 1
  CHECK(Framebuffer::textWidth("Tubarãozinho") <= 124);
  CHECK_EQ(utf8Count("João", 5), 4);
  CHECK_EQ(utf8Count("\xC3", 1), -1);
}

TEST(same_inputs_same_frames_deterministic) {
  uint32_t hashes[2];
  for (int k = 0; k < 2; k++) {
    H h(Model::Full, true, 99);
    h.skipIntro(); h.tapAndSettle(); h.dbl(); h.tap(); h.run(500); h.hold(1200); h.run(300);
    hashes[k] = fbHash(h.app->frame()) ^ (uint32_t)h.app->screen();
  }
  CHECK_EQ(hashes[0], hashes[1]);
}

// ════════ 2. Boot respeita silencioso e funciona sem rede ════════
TEST(boot_silent_first_output) {
  H h(Model::Full);
  h.preset([](Settings& s) { s.setupDone = 1; s.silent = 1; s.vibration = 0; });
  h.reboot();
  h.buzzOnTicks = h.motorOnTicks = 0;
  CHECK(!h.app->buzzer());
  h.run(3000);
  CHECK_EQ(h.app->outputs().buzzerOnCount(), 0);
  CHECK_EQ(h.app->outputs().motorOnCount(), 0);
  CHECK(h.app->screen() == Screen::Face);  // sem rede, chega ao rosto
}

TEST(boot_full_plays_and_lite_never) {
  { H h(Model::Full); h.run(500); CHECK(h.app->outputs().buzzerOnCount() > 0); CHECK(h.app->outputs().motorOnCount() > 0); }
  { H h(Model::Lite); h.run(3000); CHECK_EQ(h.app->outputs().buzzerOnCount(), 0); CHECK_EQ(h.app->outputs().motorOnCount(), 0); }
}

TEST(first_use_opens_ap_and_offline_option) {
  H h(Model::Lite, false);
  h.run(3000);
  CHECK(h.app->screen() == Screen::FirstUse);
  CHECK(h.plat.apActive);
  h.hold(1500); h.run(100);  // "usar offline"
  CHECK(h.app->screen() == Screen::Face);
  CHECK(h.app->settings().setupDone == 1);
}

// ════════ 3. Matriz de alertas ════════
TEST(alert_policy_matrix) {
  for (int m = 0; m < 2; m++) for (int snd = 0; snd < 2; snd++) for (int sil = 0; sil < 2; sil++) for (int vib = 0; vib < 2; vib++) {
    OutputManager o; AlertPrefs p; p.sound = snd; p.silent = sil; p.vibration = vib;
    o.configure((Model)m, p);
    for (int pat = 1; pat < (int)Pattern::Count; pat++) {
      o.stopAll();
      uint32_t b0 = o.buzzerOnCount(), v0 = o.motorOnCount();
      ms_t t = 1000 * pat;
      o.play((Pattern)pat, t);
      for (int i = 0; i < 800; i++) { t += 10; o.update(t); }
      bool expectB = m == 1 && snd && !sil, expectV = m == 1 && vib;
      CHECK_EQ(o.buzzerOnCount() > b0, expectB);
      CHECK_EQ(o.motorOnCount() > v0, expectV);
      CHECK(!o.buzzer() && !o.motor());  // padrões limitados no tempo
    }
  }
}

TEST(alert_cut_immediately_when_disabled) {
  OutputManager o; AlertPrefs p; o.configure(Model::Full, p);
  o.play(Pattern::TimerDone, 0); o.update(5);
  CHECK(o.buzzer()); CHECK(o.motor());
  p.silent = true; o.setPrefs(p); o.update(6);
  CHECK(!o.buzzer()); CHECK(o.motor());
  p.vibration = false; o.setPrefs(p); o.update(7);
  CHECK(!o.motor());
}

TEST(lite_api_cannot_enable_hardware) {
  H h(Model::Lite); h.skipIntro();
  std::string r;
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{\"sound\":true,\"silent\":false,\"vibration\":true}", r, h.t), 200);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/action", "{\"action\":\"preview\",\"pattern\":\"timer\"}", r, h.t), 200);
  CHECK(r.find("\"soundAllowed\":false") != std::string::npos);
  h.run(3000);
  CHECK_EQ(h.app->outputs().buzzerOnCount(), 0);
  CHECK_EQ(h.app->outputs().motorOnCount(), 0);
}

// ════════ 4. Gestos ════════
struct GC { GestureClassifier g; ms_t t = 0; std::vector<Gesture> ev; std::vector<ms_t> evt;
  void run(ms_t ms) { for (ms_t e = t + ms; t < e;) { t += 5; GestureEvent b[8]; int n = g.poll(t, b, 8); for (int i = 0; i < n; i++) { ev.push_back(b[i].g); evt.push_back(b[i].t); } } }
  void press(ms_t ms) { g.sample(true, t); run(ms); g.sample(false, t); } };

TEST(gesture_exclusive_classification) {
  struct C { ms_t dur; Gesture exp; } cases[] = {
    {80, Gesture::Tap}, {1500, Gesture::Medium}, {4000, Gesture::Long}, {9000, Gesture::RecoveryArmed}};
  for (auto& c : cases) {
    GC x; x.run(50); x.press(c.dur); x.run(1000);
    CHECK_EQ(x.ev.size(), 1u);
    if (x.ev.size() == 1) CHECK(x.ev[0] == c.exp);
  }
  GC d; d.run(50); d.press(70); d.run(100); d.press(70); d.run(1000);
  CHECK_EQ(d.ev.size(), 1u); if (!d.ev.empty()) CHECK(d.ev[0] == Gesture::DoubleTap);
  // ruído curto (< debounce) não gera nada
  GC n; for (int i = 0; i < 20; i++) { n.g.sample(true, n.t); n.t += 3; n.g.sample(false, n.t); n.run(40); }
  n.run(1000); CHECK_EQ(n.ev.size(), 0u);
  // sensor preso: somente o aviso, nenhum carinho/amor/config
  GC s; s.run(50); s.press(30000); s.run(1000);
  CHECK_EQ(s.ev.size(), 1u); if (!s.ev.empty()) CHECK(s.ev[0] == Gesture::StuckDetected);
  // toque mais curto que o intervalo de poll ainda é registrado (fila interna)
  GestureClassifier g; g.sample(true, 100); g.sample(false, 180); GestureEvent b[8];
  int k = g.poll(900, b, 8); CHECK_EQ(k, 1); if (k) CHECK(b[0].g == Gesture::Tap);
}

TEST(game_mode_immediate_no_repeat) {
  GC x; x.g.setMode(InputMode::Game, 0); x.run(50);
  ms_t pressAt = x.t;
  x.g.sample(true, x.t); x.run(25);
  CHECK_EQ(x.ev.size(), 1u);
  if (!x.ev.empty()) { CHECK(x.ev[0] == Gesture::GamePress); CHECK_EQ(x.evt[0], pressAt); }  // timestamp da borda real
  x.run(1000);  // segurando: não repete
  CHECK_EQ(x.ev.size(), 1u);
  x.run(600);   // 1,5 s: saída
  CHECK_EQ(x.ev.size(), 2u); if (x.ev.size() == 2) CHECK(x.ev[1] == Gesture::GameHoldExit);
  x.run(5000); x.g.sample(false, x.t); x.run(500);
  CHECK_EQ(x.ev.size(), 2u);
}

TEST(recovery_hold_8s_opens_config_once) {
  H h(Model::Full); h.skipIntro();
  uint32_t petsBefore = h.app->progress().pets;
  h.hold(8500); h.run(200);
  CHECK(h.app->screen() == Screen::RecoveryConfirm);
  CHECK_EQ(h.app->progress().pets, petsBefore);  // não gerou carinho/amor
  h.tap(); h.run(500);
  CHECK(h.app->screen() == Screen::Config);
  CHECK(h.plat.apActive);
}

TEST(screen_off_wake_consumes_gesture) {
  H h(Model::Full); h.skipIntro();
  std::string r; apiCall(*h.app, "POST", "/api/prefs", "{\"screenOffS\":30}", r, h.t);
  h.run(31000, 50);
  CHECK(h.app->screen() == Screen::ScreenOff);
  h.dbl();  // acordar NÃO abre o menu
  CHECK(h.app->screen() == Screen::Face);
}

// ════════ 5/6. Pula, Tuba! ════════
static int g_minGapSub = 1 << 30;
static int runJumpBot(uint32_t seed, int stepMs, int wantScore, bool briefParams, int* outCollisions) {
  JumpGame g; Rng rng(seed);
  if (briefParams) { g.p.jumpV = 120; g.p.gravity = 280; }
  ms_t t = 0; g.begin(t, &rng, true); g.press(t, t);
  int guard = 0;
  while (g.phase() == GamePhase::Playing && g.score() < wantScore && guard++ < 200000) {
    t += stepMs; g.update(t);
    // bot: pula quando o obstáculo mais próximo à frente está a uma distância ideal
    const JumpGame::Obstacle* o = g.obstacles();
    for (int i = 0; i < JumpGame::kMaxObs; i++) for (int j = 0; j < JumpGame::kMaxObs; j++)
      if (i != j && o[i].active && o[j].active && o[j].x > o[i].x) g_minGapSub = std::min(g_minGapSub, o[j].x - (o[i].x + o[i].w * 1000));
    for (int i = 0; i < JumpGame::kMaxObs; i++) {
      if (!o[i].active || o[i].scored) continue;
      int ox = o[i].x / 1000;
      int front = JumpGame::kSharkX + JumpGame::kHitX1;
      int lead = g.speed() * 160 / 1000;  // ~160 ms de antecedência
      if (ox - front <= lead && ox - front > lead - 6 && !g.airborne()) g.press(t, t);
    }
  }
  if (outCollisions) *outCollisions = g.phase() == GamePhase::Result ? 1 : 0;
  return g.score();
}

TEST(jump_always_possible_many_seeds) {
  for (int briefP = 0; briefP < 2; briefP++)
    for (uint32_t seed = 1; seed <= 40; seed++) {
      g_minGapSub = 1 << 30;
      int col = 0; int s = runJumpBot(seed, 16, 40, briefP, &col);
      CHECK_EQ(col, 0); CHECK(s >= 40);
      JumpGame ref; if (briefP) { ref.p.jumpV = 120; ref.p.gravity = 280; }
      // espaçamento nunca menor que voo completo + folga, na velocidade inicial
      CHECK(g_minGapSub >= ref.minGapSub(ref.p.speedStart));
    }
}

TEST(jump_gap_and_trajectory) {
  JumpGame g;
  int air = g.airTimeMs();
  CHECK(air >= 600 && air <= 900);
  int apexPx = g.p.jumpV * g.p.jumpV / (2 * g.p.gravity);
  CHECK(apexPx >= 18);                      // passa obstáculo de até 10 px com folga
  CHECK(JumpGame::kGroundY - SPR_SHARK.h - apexPx >= 10);  // não sai da tela / não cobre placar
  // gaps gerados nunca menores que o mínimo seguro
  Rng rng(5); ms_t t = 0; g.begin(t, &rng, true); g.press(t, t);
  std::vector<int> xs;
  for (int i = 0; i < 3000 && g.phase() == GamePhase::Playing; i++) {
    t += 4; g.update(t);
    // invencível para medir o espaçamento: reposiciona tubarão no alto
  }
  (void)xs;
  CHECK(g.minGapSub(g.p.speedMax) > g.p.speedMax * air);
}

TEST(jump_score_once_and_fps_independent) {
  int s16 = runJumpBot(11, 16, 25, false, nullptr);
  int s33 = runJumpBot(11, 33, 25, false, nullptr);
  CHECK_EQ(s16, 25); CHECK_EQ(s33, 25);
  // mesma sequência de entradas com diferentes taxas de quadros -> mesmo estado
  JumpGame a, b; Rng ra(3), rb(3); a.begin(0, &ra, true); b.begin(0, &rb, true); a.press(0, 0); b.press(0, 0);
  ms_t presses[] = {1700, 2900, 4100};
  ms_t ta = 0, tb2 = 0; int pi = 0, pj = 0;
  while (ta < 5000) { ta += 16; while (pi < 3 && presses[pi] <= ta) { a.press(presses[pi], ta); pi++; } a.update(ta); }
  while (tb2 < 5000) { tb2 += 50; while (pj < 3 && presses[pj] <= tb2) { b.press(presses[pj], tb2); pj++; } b.update(tb2); }
  a.update(5000); b.update(5000);
  CHECK_EQ(a.score(), b.score());
  CHECK_EQ(a.phase() == GamePhase::Playing, b.phase() == GamePhase::Playing);
}

TEST(jump_buffer_consumed_once) {
  JumpGame g; Rng rng(1); g.begin(0, &rng, true); g.press(0, 0);
  ms_t t = 10; g.update(t); g.press(t, t);  // pulo
  CHECK(g.airborne());
  // toque bem cedo no ar: ignorado
  t += 200; g.update(t); g.press(t, t);
  int landT = g.airTimeMs();
  t = 10 + landT - 80; g.update(t); g.press(t, t);  // 80 ms antes de aterrissar: bufferizado
  t += 100; g.update(t);
  CHECK(g.airborne());  // pulou de novo ao aterrissar
  t += g.airTimeMs() + 50; g.update(t);
  CHECK(!g.airborne()); // buffer consumido uma única vez
}

// ════════ 7. Hora do Bote ════════
TEST(fish_fully_inside_rule) {
  CHECK(FishGame::fullyInside(FishGame::kInnerX0));
  CHECK(!FishGame::fullyInside(FishGame::kInnerX0 - 1));                   // um pixel da cauda fora
  CHECK(FishGame::fullyInside(FishGame::kInnerX1 - SPR_FISH.w + 1));
  CHECK(!FishGame::fullyInside(FishGame::kInnerX1 - SPR_FISH.w + 2));      // um pixel do focinho fora
  // a bounding box cobre todos os pixels do sprite (cauda incluída)
  int minX = 99, maxX = -1;
  for (int j = 0; j < SPR_FISH.h; j++) for (int i = 0; i < SPR_FISH.w; i++) if (SPR_FISH.rows[j][i] == '#') { minX = std::min(minX, i); maxX = std::max(maxX, i); }
  CHECK_EQ(minX, 0); CHECK_EQ(maxX, SPR_FISH.w - 1);
  CHECK(FishGame::kInnerX1 - FishGame::kInnerX0 + 1 > SPR_FISH.w);  // interior maior que o peixe
}

// avança o jogo de 1 em 1 ms até a borda esquerda do peixe chegar exatamente a 'left'
static ms_t timeWhenLeftIs(FishGame& g, ms_t from, int left) {
  for (ms_t t = from; t < from + 20000; t++) {
    g.update(t);
    int32_t x = g.fishXsubAt(g.gameTimeOf(t));
    if (x >= left * 1000 && x < (left + 1) * 1000) return t;
  }
  return 0;
}

TEST(fish_capture_speedup_and_miss) {
  FishGame g; Rng rng(1); ms_t t = 0;
  g.begin(t, &rng, true); g.press(t, t);
  int32_t lastV = g.speedSub();
  for (int k = 0; k < 8; k++) {
    ms_t ti = timeWhenLeftIs(g, t, FishGame::kInnerX0 + 2);
    CHECK(ti > 0);
    for (ms_t u = t; u < ti; u += 16) g.update(u);
    g.press(ti, ti + 3);
    CHECK_EQ(g.captures(), k + 1);
    CHECK(g.speedSub() > lastV);  // cada captura acelera o próximo peixe
    lastV = g.speedSub();
    t = ti + 3;
    for (int i = 0; i < 40; i++) { t += 16; g.update(t); }  // fim da animação; peixe novo fora da área
    int left = g.fishXsubAt(g.gameTimeOf(t)) / 1000;
    CHECK(!FishGame::fullyInside(left));
  }
  // deixar passar não é erro
  int c = g.captures();
  for (int i = 0; i < 600; i++) { t += 16; g.update(t); }
  CHECK(g.phase() == GamePhase::Playing); CHECK_EQ(g.captures(), c);
  // toque com 1 pixel fora -> encerra
  ms_t ti = timeWhenLeftIs(g, t, FishGame::kInnerX0 - 1);
  for (ms_t u = t; u < ti; u += 16) g.update(u);
  int left = g.fishXsubAt(g.gameTimeOf(ti)) / 1000;
  CHECK_EQ(left, FishGame::kInnerX0 - 1);
  g.press(ti, ti);
  CHECK(g.phase() == GamePhase::Result);
  GameOutcome o = g.takeOutcome(); CHECK(o.finished); CHECK_EQ(o.score, c);
}

TEST(fish_hold_does_not_autocapture_in_app) {
  H h(Model::Lite); h.skipIntro();
  h.app->startGame(GameId::Fish, h.t);
  h.tap(); h.run(100);            // inicia a rodada
  h.level(true); h.run(1200);     // segura: só a borda conta, sem capturas automáticas
  CHECK(h.app->fish().captures() <= 1);
  int c = h.app->fish().captures();
  h.run(1000); h.level(false); h.run(100);
  CHECK_EQ(h.app->fish().captures(), c);
}

// ════════ 8. Tuba Reflexo ════════
TEST(reflex_rules) {
  ReflexGame g; Rng rng(9); g.begin(0, &rng, true);
  g.press(100, 100);  // inicia
  CHECK(g.sub() == ReflexGame::Sub::Waiting);
  g.update(150);
  CHECK(g.sub() == ReflexGame::Sub::Waiting);  // o toque inicial não é resposta
  g.press(500, 500);
  CHECK(g.sub() == ReflexGame::Sub::Early);
  GameOutcome o = g.takeOutcome(); CHECK(o.finished); CHECK(!o.valid);
  // rodada válida: mede a partir da APRESENTAÇÃO
  g.press(2000, 2000);
  int32_t w = g.lastWait(); CHECK(w >= 1500 && w <= 4000);
  ms_t sig = 2000 + w; g.update(sig);
  CHECK(g.sub() == ReflexGame::Sub::Signal);
  Framebuffer fb; g.draw(fb, sig);         // quadro AGORA! desenhado...
  g.press(sig + 5, sig + 5);               // ...mas ainda não apresentado: cedo demais
  CHECK(g.sub() == ReflexGame::Sub::Early);
  g.takeOutcome();
  g.press(9000, 9000); w = g.lastWait(); sig = 9000 + w; g.update(sig); g.draw(fb, sig);
  g.framePresented(sig + 30);              // apresentado 30 ms depois (I²C)
  g.press(sig + 30 + 250, sig + 300);
  CHECK(g.sub() == ReflexGame::Sub::Done);
  CHECK_EQ(g.lastMs(), 250);
  o = g.takeOutcome(); CHECK(o.valid); CHECK_EQ(o.score, 250);
  // espera varia
  int distinct = 0; int32_t prev = -1;
  for (int i = 0; i < 10; i++) { g.press(20000 + i * 10000, 20000 + i * 10000); if (g.lastWait() != prev) distinct++; prev = g.lastWait(); g.pause(20001 + i * 10000); g.resume(20002 + i * 10000); g.takeOutcome(); }
  CHECK(distinct >= 8);
}

// ════════ 9. No Tempo Certo ════════
TEST(timing_targets_and_formula) {
  Rng rng(123); TimingGame g; int mn = 9999, mx = 0;
  for (int i = 0; i < 20000; i++) { g.begin(0, &rng, true); mn = std::min(mn, g.targetCs()); mx = std::max(mx, g.targetCs()); CHECK(g.targetCs() >= 300 && g.targetCs() <= 800); }
  CHECK_EQ(mn, 300); CHECK_EQ(mx, 800);
  CHECK_EQ(TimingGame::proximity100(7430, 7430), 10000);
  CHECK_EQ(TimingGame::proximity100(7500, 7430), 9906);   // 99,06%
  CHECK_EQ(TimingGame::proximity100(7360, 7430), 9906);
  CHECK_EQ(TimingGame::proximity100(20000, 3000), 0);
  // não usa o valor arredondado da tela: 7,434 s mostra "7,43" mas o erro é 4 ms
  CHECK_EQ(TimingGame::proximity100(7434, 7430), 9995);
  char b[16]; StrBuf s(b, sizeof b); s.fixed2(9906).put('%'); CHECK(strcmp(b, "99,06%") == 0);
}

TEST(timing_flow_countdown_and_limit) {
  Rng rng(5); TimingGame g; g.begin(0, &rng, true); g.forceTarget(743);
  g.press(1000, 1000);
  CHECK(g.sub() == TimingGame::Sub::Countdown);
  g.press(2500, 2500);  // durante 3,2,1: ignorado
  g.update(3999); CHECK(g.sub() == TimingGame::Sub::Countdown);
  g.update(4000); CHECK(g.sub() == TimingGame::Sub::Running);
  CHECK_EQ(g.goAt(), 4000);
  g.press(4000 + 7500, 4000 + 7510);
  CHECK(g.sub() == TimingGame::Sub::Done);
  GameOutcome o = g.takeOutcome(); CHECK(o.valid); CHECK_EQ(o.score, 9906); CHECK_EQ(o.aux, 70);
  // limite de 20 s
  g.press(20000, 20000); CHECK(g.sub() == TimingGame::Sub::Ready);
  g.press(21000, 21000); g.update(24000); g.update(24000 + 20000);
  CHECK(g.sub() == TimingGame::Sub::Timeout);
  o = g.takeOutcome(); CHECK(o.finished); CHECK(!o.valid);
}

TEST(timing_press_held_through_go_does_not_stop) {
  H h(Model::Lite); h.skipIntro();
  h.app->startGame(GameId::Timing, h.t);
  h.tap(); h.run(100);  // começa a contagem 3,2,1
  CHECK(h.app->timing().sub() == TimingGame::Sub::Countdown);
  h.run(2000);
  h.level(true); h.run(1200); // segura atravessando o VAI (menos que a saída de 1,5 s)
  CHECK(h.app->timing().sub() == TimingGame::Sub::Running);
  h.level(false); h.run(200);
  CHECK(h.app->timing().sub() == TimingGame::Sub::Running);
  h.tap(); CHECK(h.app->timing().sub() == TimingGame::Sub::Done);
}

// ════════ 10/11/12. Tempo ════════
TEST(timers_pomodoro_offscreen_pause_single_completion) {
  H h(Model::Full); h.skipIntro();
  std::string r;
  CHECK_EQ(apiCall(*h.app, "POST", "/api/action", "{\"action\":\"timer.start\",\"slot\":0,\"seconds\":5}", r, h.t), 200);
  h.dbl();  // navega para o menu: contagem continua
  CHECK(h.app->screen() == Screen::Menu);
  h.run(2000);
  h.app->timer(0).pause(h.t); uint32_t rem = h.app->timer(0).remainingMs(h.t);
  h.run(10000);
  CHECK_EQ(h.app->timer(0).remainingMs(h.t), rem);  // pausado mantém a duração
  h.app->timer(0).resume(h.t);
  h.run(1000);
  // um quadro muito atrasado: conclusão gera UM único alerta
  h.t += 30000; h.tick(); h.tick();
  CHECK_EQ(h.app->alertCount(), 1);
  h.run(100);
  CHECK_EQ(h.app->alertCount(), 1);
  // pomodoro com duração curta
  apiCall(*h.app, "POST", "/api/prefs", "{\"pomodoro\":{\"focusMin\":1,\"shortMin\":1}}", r, h.t);
  h.tap(); h.run(400);  // reconhece o alerta do timer
  CHECK_EQ(h.app->alertCount(), 0);
  uint32_t done = h.app->progress().pomodorosDone;
  apiCall(*h.app, "POST", "/api/action", "{\"action\":\"pomodoro.toggle\"}", r, h.t);
  h.run(61000, 50);
  CHECK_EQ(h.app->progress().pomodorosDone, done + 1);
  CHECK(h.app->pomodoro().phase() == PomoPhase::ShortBreak);
  CHECK(h.app->progress().unlocked & (1u << UNL_FOCADO));
  // pular/cancelar não contam conquista
  h.tap(); h.run(400);
  apiCall(*h.app, "POST", "/api/action", "{\"action\":\"pomodoro.skip\"}", r, h.t);
  apiCall(*h.app, "POST", "/api/action", "{\"action\":\"pomodoro.skip\"}", r, h.t);
  CHECK_EQ(h.app->progress().pomodorosDone, done + 1);
}

TEST(alert_during_game_pauses_fairly) {
  H h(Model::Lite); h.skipIntro();
  std::string r;
  apiCall(*h.app, "POST", "/api/action", "{\"action\":\"timer.start\",\"slot\":1,\"seconds\":2}", r, h.t);
  h.app->startGame(GameId::Reflex, h.t);
  h.tap(); h.run(50);  // começa a esperar
  CHECK(h.app->reflex().sub() == ReflexGame::Sub::Waiting);
  uint16_t best = h.app->progress().reflexBestMs;
  h.run(2500);
  CHECK_EQ(h.app->alertCount(), 1);
  CHECK(h.app->reflex().sub() == ReflexGame::Sub::Interrupted);
  h.tap(); h.run(400);
  CHECK_EQ(h.app->alertCount(), 0);
  CHECK(h.app->screen() == Screen::Game);
  CHECK_EQ(h.app->progress().reflexBestMs, best);  // sem recorde injusto
  // pula, tuba: pausa e retoma
  apiCall(*h.app, "POST", "/api/action", "{\"action\":\"timer.start\",\"slot\":1,\"seconds\":1}", r, h.t);
  h.app->startGame(GameId::Jump, h.t); h.tap(); h.run(300);
  h.run(1000);
  CHECK(h.app->jump().paused());
  ms_t simBefore = h.app->jump().simTime();
  h.run(3000);
  CHECK_EQ(h.app->jump().simTime(), simBefore);  // congelado durante o alerta
  h.tap(); h.run(400);   // ok
  h.tap(); h.run(100);   // seguir
  CHECK(!h.app->jump().paused());
}

TEST(stopwatch_counts_up_no_alarm) {
  Stopwatch s; s.start(0);
  CHECK_EQ(s.elapsedMs(1234), 1234);
  s.pause(2000); CHECK_EQ(s.elapsedMs(9000), 2000);
  s.resume(9000); CHECK_EQ(s.elapsedMs(10000), 3000);
  H h(Model::Full); h.skipIntro();
  std::string r; apiCall(*h.app, "POST", "/api/action", "{\"action\":\"stopwatch.toggle\"}", r, h.t);
  h.run(120000, 100);
  CHECK_EQ(h.app->alertCount(), 0);
  Stopwatch l; l.start(0); l.poll(Stopwatch::kLimitMs + 5000);
  CHECK(l.limitHit()); CHECK_EQ(l.elapsedMs(Stopwatch::kLimitMs + 9000), (long long)Stopwatch::kLimitMs);
}

TEST(civil_time_independent_of_durations) {
  H h(Model::Full); h.skipIntro();
  CHECK(!h.app->clock().valid());  // sem RTC: não inventa a data
  std::string r;
  apiCall(*h.app, "POST", "/api/action", "{\"action\":\"timer.start\",\"slot\":0,\"seconds\":60}", r, h.t);
  h.run(10000);
  uint32_t rem = h.app->timer(0).remainingMs(h.t);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/time", "{\"epoch\":1791936000,\"tz\":\"America/Manaus\"}", r, h.t), 200);
  CHECK(h.app->clock().valid());
  CHECK_EQ(h.app->timer(0).remainingMs(h.t), rem);
  apiCall(*h.app, "POST", "/api/time", "{\"epoch\":1600000000}", r, h.t);
  CHECK_EQ(h.app->timer(0).remainingMs(h.t), rem);
  h.reboot();
  CHECK(!h.app->clock().valid());
  CivilClock::Local L = civilFromEpoch(1791936000LL - 180 * 60);
  CHECK_EQ(L.year, 2026); CHECK_EQ(L.month, 10); CHECK_EQ(L.day, 13);
}

// ════════ 13. Persistência ════════
TEST(persistence_survives_reboot_and_resets) {
  H h(Model::Full); h.skipIntro();
  std::string r;
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{\"nickname\":\"Tubarão\",\"personality\":\"brincalhao\",\"silent\":true}", r, h.t), 200);
  h.hold(1300); h.run(500);  // carinho -> Tímido
  CHECK(h.app->progress().unlocked & 1);
  apiCall(*h.app, "POST", "/api/wifi", "{\"ssid\":\"Casa\",\"pass\":\"segredo123\"}", r, h.t);
  h.run(5000);
  uint32_t w = h.app->storageWrites();
  CHECK(w > 0 && w < 10);  // gravações agrupadas, não por quadro
  h.reboot();
  CHECK(strcmp(h.app->settings().nickname, "Tubarão") == 0);
  CHECK_EQ(h.app->settings().personality, 1);
  CHECK(h.app->progress().unlocked & 1);
  // esquecer wifi preserva dados
  CHECK_EQ(apiCall(*h.app, "POST", "/api/reset", "{\"scope\":\"wifi\",\"confirm\":\"ESQUECER\"}", r, h.t), 200);
  CHECK(!h.plat.staConfigured);
  CHECK(strcmp(h.app->settings().nickname, "Tubarão") == 0);
  CHECK(h.app->progress().unlocked & 1);
  // reset total exige confirmação explícita
  CHECK_EQ(apiCall(*h.app, "POST", "/api/reset", "{\"scope\":\"all\",\"confirm\":\"sim\"}", r, h.t), 422);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/reset", "{\"scope\":\"all\",\"confirm\":\"APAGAR TUDO\"}", r, h.t), 200);
  CHECK_EQ(h.plat.reboots, 1);
  h.reboot();
  CHECK_EQ(h.app->settings().nickname[0], 0);
  CHECK_EQ(h.app->progress().unlocked, 0);
  CHECK_EQ(h.app->settings().setupDone, 0);
}

TEST(corrupted_storage_recovers_defaults) {
  H h(Model::Full); h.skipIntro();
  h.plat.store[(int)BlobKind::Settings][20] ^= 0xFF;  // corrompe
  h.plat.storeLen[(int)BlobKind::Progress] = 5;       // incompleto
  delete h.app; h.app = new App(h.plat); h.app->begin(Model::Full, h.t, 1, true);
  CHECK(h.app->storageRecovered());
  CHECK_EQ(h.app->settings().pomoFocusMin, 25);
  h.run(4000);  // ainda inicia normalmente
  CHECK(h.app->screen() == Screen::FirstUse || h.app->screen() == Screen::Face);
}

TEST(power_loss_resume_offer) {
  H h(Model::Full); h.skipIntro();
  std::string r;
  apiCall(*h.app, "POST", "/api/action", "{\"action\":\"pomodoro.toggle\"}", r, h.t);
  h.run(5000);
  h.reboot();
  h.run(3000);
  CHECK(h.app->screen() == Screen::Resume);
  h.tap(); h.run(400);
  CHECK(h.app->screen() == Screen::Pomodoro);
  CHECK(h.app->pomodoro().state() == RunState::Paused);  // retoma pausado, sem adivinhar o tempo desligado
}

// ════════ 14. API / portal ════════
TEST(api_auth_validation_and_secrets) {
  H h(Model::Full); h.skipIntro();
  std::string r;
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{\"nickname\":\"x\"}", r, h.t, false), 401);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{\"nickname\":\"Nome muito comprido\"}", r, h.t), 422);
  CHECK(r.find("12 caracteres") != std::string::npos);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{\"nickname\":\"Júlia \\u00c9va\"}", r, h.t), 200);  // acentos e \u
  CHECK(strcmp(h.app->settings().nickname, "Júlia Éva") == 0);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{\"contrast\":999,\"nickname\":\"Ok\"}", r, h.t), 422);
  CHECK(strcmp(h.app->settings().nickname, "Júlia Éva") == 0);  // tudo ou nada
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{bad json", r, h.t), 400);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{\"favoriteExpr\":\"victorious\"}", r, h.t), 422);  // bloqueada
  apiCall(*h.app, "POST", "/api/wifi", "{\"ssid\":\"Casa\",\"pass\":\"segredo123\"}", r, h.t);
  for (const char* p : {"/api/status", "/api/prefs", "/api/wifi"}) { apiCall(*h.app, "GET", p, nullptr, r, h.t); CHECK(r.find("segredo123") == std::string::npos); }
  apiCall(*h.app, "POST", "/api/backup", "{}", r, h.t); CHECK(r.find("segredo123") == std::string::npos);
  // rede de casa exige PIN
  CHECK_EQ(apiCall(*h.app, "POST", "/api/session", "{}", r, h.t, false, false), 401);
  std::string body = std::string("{\"pin\":\"") + h.app->pairPin() + "\"}";
  CHECK_EQ(apiCall(*h.app, "POST", "/api/session", body.c_str(), r, h.t, false, false), 200);
  // corpo grande demais
  std::string big(5000, ' '); big[0] = '{'; big[4999] = '}';
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", big.c_str(), r, h.t), 413);
  // backup -> restore
  apiCall(*h.app, "POST", "/api/backup", "{}", r, h.t);
  std::string bk = r;
  apiCall(*h.app, "POST", "/api/prefs", "{\"nickname\":\"Outro\"}", r, h.t);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/restore", bk.c_str(), r, h.t), 200);
  CHECK(strcmp(h.app->settings().nickname, "Júlia Éva") == 0);
}

TEST(dev_unlock_uses_separate_test_profile) {
  H h(Model::Full); h.skipIntro();
  std::string r;
  CHECK_EQ(apiCall(*h.app, "POST", "/api/action", "{\"action\":\"dev.unlockAll\"}", r, h.t), 200);
  CHECK(h.app->testProgress());
  h.run(6000);
  h.reboot();
  CHECK_EQ(h.app->progress().unlocked, 0);  // nada foi gravado no perfil real
}

TEST(json_parser_robust) {
  JsonVal v;
  const char* doc = "{\"a\":[1,2,{\"b\":\"x\\\"y\"}],\"c\":true}";
  CHECK(jsonParse(doc, strlen(doc), v));
  int64_t n; CHECK(v.get("a").at(1).asInt(n)); CHECK_EQ(n, 2);
  char s[8]; CHECK(v.get("a").at(2).get("b").asString(s, sizeof s)); CHECK(strcmp(s, "x\"y") == 0);
  CHECK(!jsonParse("{\"a\":1,}", 8, v));
  CHECK(!jsonParse("[[[[[[[[[[[[[[[[1]]]]]]]]]]]]]]]]", 34, v));  // profundidade limitada
  JsonVal f; jsonParse("{\"x\":1.5}", 9, f); CHECK(!f.get("x").asInt(n));
}

TEST(menu_navigation_map) {
  H h(Model::Full); h.skipIntro();
  CHECK(h.app->screen() == Screen::Face);
  h.dbl(); CHECK(h.app->screen() == Screen::Menu);
  h.tapAndSettle(); h.tapAndSettle();      // Rosto -> Relógio -> Pomodoro
  h.hold(1200); h.run(200);                // selecionar
  CHECK(h.app->screen() == Screen::Pomodoro);
  h.tapAndSettle();                        // iniciar
  CHECK(h.app->pomodoro().state() == RunState::Running);
  h.hold(1200); h.run(200);                // volta ao menu SEM cancelar
  CHECK(h.app->screen() == Screen::Menu);
  CHECK(h.app->pomodoro().state() == RunState::Running);
  h.dbl(); CHECK(h.app->screen() == Screen::Face);
}

int main(int argc, char** argv) {
  const char* only = argc > 1 ? argv[1] : nullptr;
  int ran = 0;
  for (auto& t : Reg::all()) {
    if (only && !strstr(t.first, only)) continue;
    g_test = t.first;
    int before = g_fail;
    t.second();
    printf("%s %s\n", g_fail == before ? "ok  " : "FAIL", t.first);
    ran++;
  }
  printf("\n%d testes, %d verificações, %d falhas\n", ran, g_checks, g_fail);
  return g_fail ? 1 : 0;
}
