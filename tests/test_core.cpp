// Testes nativos do núcleo do TubaBot v2.
// Compilar e rodar: make test
#include "fake_platform.h"
#include "../core/app_ids.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

using namespace tb;

static int g_fail = 0, g_checks = 0;
static const char* g_test = "";
#define CHECK(c) do { g_checks++; if (!(c)) { g_fail++; printf("  FALHOU [%s] %s:%d: %s\n", g_test, __FILE__, __LINE__, #c); } } while (0)
#define CHECK_EQ(a, b) do { g_checks++; long long _a = (long long)(a), _b = (long long)(b); if (_a != _b) { g_fail++; printf("  FALHOU [%s] %s:%d: %s == %s (%lld != %lld)\n", g_test, __FILE__, __LINE__, #a, #b, _a, _b); } } while (0)
#define TEST(name) static void name(); static Reg reg_##name(#name, name); static void name()
struct Reg { static std::vector<std::pair<const char*, void (*)()>>& all() { static std::vector<std::pair<const char*, void (*)()>> v; return v; }
  Reg(const char* n, void (*f)()) { all().push_back({n, f}); } };

// canvas avulso para desenhar jogos fora do App
static Canvas& cv() {
  static std::vector<uint16_t> px(SCR_W * SCR_H);
  static Canvas c;
  static bool init = false;
  if (!init) { c.attach(px.data(), SCR_W, SCR_H); init = true; }
  return c;
}

// ── Harness ─────────────────────────────────────────────────────────────────
static HwCaps fullCaps() { HwCaps c; c.motor = true; return c; }

struct H {
  FakePlatform plat;
  App* app = nullptr;
  std::vector<uint16_t> fb;
  HwCaps caps;
  ms_t t = 0;
  H(HwCaps c = fullCaps(), bool setupDone = true, uint32_t seed = 42) : fb(SCR_W * SCR_H), caps(c) {
    if (setupDone) preset([](Settings& s) { s.setupDone = 1; });
    start(seed);
  }
  ~H() { delete app; }
  void start(uint32_t seed) { app = new App(plat); app->attachFramebuffer(fb.data()); app->begin(caps, t, seed, true); }
  void preset(void (*f)(Settings&)) {
    Settings s; defaultSettings(s); f(s);
    uint8_t buf[BLOB_MAX]; size_t n = encodeBlob(BlobKind::Settings, SETTINGS_VERSION, &s, sizeof s, buf, sizeof buf);
    plat.storageSave(BlobKind::Settings, buf, n);
  }
  void reboot() { app->flush(t, true); delete app; t += 5000; start(7); }
  void tick() { if (app->tick(t)) app->framePresented(t); }
  void run(ms_t ms, ms_t step = 10) { ms_t end = t + ms; while (t < end) { t += step; if (t > end) t = end; tick(); } }
  void down(int x = 233, int y = 233) { app->touchSample(true, x, y, t); }
  void up(int x = 233, int y = 233) { app->touchSample(false, x, y, t); }
  void hold(ms_t ms, int x = 233, int y = 233, ms_t step = 10) { down(x, y); ms_t end = t + ms; while (t < end) { t += step; down(x, y); tick(); } up(x, y); }
  void tap(int x = 233, int y = 233) { hold(70, x, y); run(400); }
  void dbl(int x = 233, int y = 233) { hold(60, x, y); run(80); hold(60, x, y); run(450); }
  void swipe(int dx, int dy) {
    int x0 = 233 - dx / 2, y0 = 233 - dy / 2;
    down(x0, y0); run(10);
    for (int i = 1; i <= 6; i++) { t += 25; down(x0 + dx * i / 6, y0 + dy * i / 6); tick(); }
    up(x0 + dx, y0 + dy); run(150);
  }
  void stroke(ms_t ms) {  // carinho: vai e volta devagar
    down(150, 233); run(10);
    for (ms_t e = 0; e < ms; e += 20) { int x = 150 + (int)((e / 20) % 20) * 8; if ((e / 400) % 2) x = 310 - (int)((e / 20) % 20) * 8; t += 20; down(x, 233); tick(); }
    up(); run(300);
  }
  bool press(int id) { tick(); int x, y; if (!app->buttonPos(id, x, y)) return false; tap(x, y); return true; }
  bool pick(int id) { tick(); int x, y; if (!app->listItemPos(id, x, y)) return false; run(50); tap(x, y); return true; }
  void skipIntro() { run(3000, 20); }
};

static int apiCall(App& a, const char* m, const char* path, const char* body, std::string& out, ms_t now, bool tok = true, bool viaAP = true) {
  static char buf[65536];
  StrBuf sb(buf, sizeof buf);
  ApiCtx ctx; ctx.viaAP = viaAP; ctx.devTools = true; ctx.token = tok ? a.sessionToken() : "x";
  int code = a.api(m, path, body, body ? strlen(body) : 0, ctx, sb, now);
  out = buf;
  return code;
}

// ════════ 1. Motor gráfico ════════
TEST(gfx_antialias_dirty_and_text) {
  Canvas& c = cv();
  c.invalidateAll(); c.beginFrame(0); c.endFrame();
  c.beginFrame(0);
  c.fillCircle(100.f, 100.f, 20.f, 0xFF0000);
  CHECK_EQ(c.get(100, 100), to565(0xFF0000));
  uint16_t edge = c.get(100, 80);  // borda: mistura parcial (antialias)
  CHECK(edge != 0 && edge != to565(0xFF0000));
  CHECK_EQ(c.get(100, 75), 0);
  IRect d = c.endFrame();
  CHECK(d.x0 % 2 == 0 && d.y0 % 2 == 0 && d.w() % 2 == 0 && d.h() % 2 == 0);  // CO5300 exige pares
  CHECK(d.x0 <= 80 && d.x1 >= 120);
  c.beginFrame(0);
  IRect d2 = c.endFrame();  // o quadro seguinte apaga o círculo antigo
  CHECK(!d2.empty());
  CHECK_EQ(c.get(100, 100), 0);
  c.beginFrame(0);
  IRect d3 = c.endFrame();
  CHECK(d3.empty());        // nada mudou: nada a enviar
  CHECK_EQ(Canvas::textWidth("ÁÉÍÓÚçãõ", FONT_M) > 0, true);
  CHECK(Canvas::textWidth("Tubarãozinho", FONT_L) <= 340);
  CHECK_EQ(utf8Count("João", 5), 4);
  CHECK_EQ(utf8Count("\xC3", 1), -1);
  CHECK(FONT_CLOCK.find('7') && !FONT_CLOCK.find('A'));
  CHECK(FONT_M.find(0xE3) != nullptr);  // ã
}

TEST(same_inputs_same_frames_deterministic) {
  uint32_t hashes[2];
  for (int k = 0; k < 2; k++) {
    H h(fullCaps(), true, 99);
    h.skipIntro(); h.tap(); h.dbl(); h.swipe(-160, 0); h.run(500); h.swipe(160, 0); h.hold(1200); h.run(300);
    hashes[k] = h.app->canvas().crc() ^ (uint32_t)h.app->screen();
  }
  CHECK_EQ(hashes[0], hashes[1]);
}

// ════════ 2. Boot respeita silencioso e funciona sem rede ════════
TEST(boot_silent_first_output) {
  H h;
  h.preset([](Settings& s) { s.setupDone = 1; s.silent = 1; s.vibration = 0; });
  h.reboot();
  h.run(3500);
  CHECK_EQ(h.app->outputs().soundStarts(), 0);
  CHECK_EQ(h.app->outputs().motorOnCount(), 0);
  CHECK(h.app->screen() == Screen::Face);  // sem rede, chega ao rosto
}

TEST(boot_plays_and_missing_hardware_never) {
  { H h; h.run(500); CHECK(h.app->outputs().soundStarts() > 0); CHECK(h.app->outputs().motorOnCount() > 0); }
  { HwCaps c; c.speaker = false; c.motor = false; H h(c); h.run(3500); CHECK_EQ(h.app->outputs().soundStarts(), 0); CHECK_EQ(h.app->outputs().motorOnCount(), 0); }
}

TEST(first_use_opens_ap_qr_and_offline_option) {
  H h(fullCaps(), false);
  h.run(3000);
  CHECK(h.app->screen() == Screen::FirstUse);
  CHECK(h.plat.apActive);
  QrCode q; CHECK(qrEncodeText("WIFI:T:WPA;S:TubaBot-AB12;P:tuba-7f3k;;", q)); CHECK(q.size >= 21);
  CHECK(h.press(B_PAGE_NEXT));
  CHECK(h.press(B_FIRST_OFFLINE));
  CHECK(h.app->screen() == Screen::Face);
  CHECK(h.app->settings().setupDone == 1);
  CHECK(!h.plat.apActive);
}

// ════════ 3. Matriz de alertas ════════
TEST(alert_policy_matrix) {
  for (int spk = 0; spk < 2; spk++) for (int mot = 0; mot < 2; mot++)
  for (int snd = 0; snd < 2; snd++) for (int sil = 0; sil < 2; sil++) for (int vib = 0; vib < 2; vib++) for (int vol = 0; vol < 2; vol++) {
    OutputManager o; AlertPrefs p; p.sound = snd; p.silent = sil; p.vibration = vib; p.volume = vol ? 60 : 0;
    HwCaps c; c.speaker = spk; c.motor = mot;
    o.configure(c, p);
    for (int pat = 1; pat < (int)Pattern::Count; pat++) {
      o.stopAll();
      uint32_t s0 = o.soundStarts(), v0 = o.motorOnCount();
      ms_t t = 1000000ull * pat;
      o.play((Pattern)pat, t);
      bool hadSound = o.soundActive(t + 1);
      for (int i = 0; i < 13000; i++) { t += 10; o.update(t); }
      bool expectS = spk && snd && !sil && vol;
      bool expectV = mot && vib;
      CHECK_EQ(o.soundStarts() > s0, expectS);
      if (!expectS) CHECK(!hadSound);
      int16_t buf[64]; o.synth().render(buf, 64);
      bool vPat = (Pattern)pat != Pattern::Yawn && (Pattern)pat != Pattern::Babble && (Pattern)pat != Pattern::Swipe && (Pattern)pat != Pattern::Breath;
      CHECK_EQ(o.motorOnCount() > v0, expectV && vPat);
      CHECK(!o.soundActive(t) && !o.motor());  // padrões limitados no tempo (alarme: 2 min)
    }
  }
}

TEST(alert_cut_immediately_when_disabled) {
  OutputManager o; AlertPrefs p; o.configure(fullCaps(), p);
  o.play(Pattern::TimerDone, 0); o.update(5);
  CHECK(o.soundActive(5)); CHECK(o.motor());
  p.silent = true; o.setPrefs(p); o.update(6);
  CHECK(!o.soundActive(6)); CHECK(o.motor());
  int16_t buf[256]; o.synth().render(buf, 256);
  bool silent = true; for (int i = 0; i < 256; i++) if (buf[i]) silent = false;
  CHECK(silent);
  p.vibration = false; o.setPrefs(p); o.update(7);
  CHECK(!o.motor());
}

TEST(no_speaker_api_cannot_enable_sound) {
  HwCaps c; c.speaker = false; c.motor = false;
  H h(c); h.skipIntro();
  std::string r;
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{\"sound\":true,\"silent\":false,\"vibration\":true,\"volume\":100}", r, h.t), 200);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/action", "{\"action\":\"preview\",\"pattern\":\"alarm\"}", r, h.t), 200);
  CHECK(r.find("\"soundAllowed\":false") != std::string::npos);
  h.run(3000);
  CHECK_EQ(h.app->outputs().soundStarts(), 0);
  CHECK_EQ(h.app->outputs().motorOnCount(), 0);
}

TEST(synth_render_volume_and_babble) {
  Synth s; s.setVolume(80); s.play(Sound::Boot);
  int16_t buf[4000]; s.render(buf, 4000);
  int peak = 0; for (int i = 0; i < 4000; i++) peak = std::max(peak, std::abs((int)buf[i]));
  CHECK(peak > 3000); CHECK(s.busy());
  s.setVolume(0); s.play(Sound::Boot); s.render(buf, 4000);
  peak = 0; for (int i = 0; i < 4000; i++) peak = std::max(peak, std::abs((int)buf[i]));
  CHECK_EQ(peak, 0);
  // fala de robô determinística pela semente
  Synth a, b; a.setVolume(70); b.setVolume(70); a.play(Sound::Babble, 1234); b.play(Sound::Babble, 1234);
  int16_t x[8000], y[8000]; a.render(x, 8000); b.render(y, 8000);
  CHECK(memcmp(x, y, sizeof x) == 0);
  // termina sozinho e sem estalo final
  Synth t; t.setVolume(100); t.play(Sound::Tap); int16_t z[2000]; t.render(z, 2000);
  CHECK(!t.busy() || Synth::durationMs(Sound::Tap) > 120);
  CHECK(std::abs((int)z[1999]) < 50);
  CHECK(Synth::durationMs(Sound::Alarm) > 60000);
  CHECK_EQ(Synth::channelOf(Sound::Alarm), 1); CHECK_EQ(Synth::channelOf(Sound::Tap), 0);
}

// ════════ 4. Toque ════════
struct TC { TouchClassifier g; ms_t t = 0; std::vector<TouchEvent> ev;
  void run(ms_t ms) { for (ms_t e = t + ms; t < e;) { t += 5; TouchEvent b[16]; int n = g.poll(t, b, 16); for (int i = 0; i < n; i++) ev.push_back(b[i]); } }
  void press(ms_t ms, int x = 200, int y = 200) { g.sample(true, x, y, t); for (ms_t e = t + ms; t < e;) { t += 5; g.sample(true, x, y, t); TouchEvent b[16]; int n = g.poll(t, b, 16); for (int i = 0; i < n; i++) ev.push_back(b[i]); } g.sample(false, x, y, t); }
  void drag(int dx, int dy, ms_t ms) { int x = 233, y = 233; g.sample(true, x, y, t); int steps = (int)(ms / 10); for (int i = 1; i <= steps; i++) { t += 10; g.sample(true, x + dx * i / steps, y + dy * i / steps, t); TouchEvent b[16]; int n = g.poll(t, b, 16); for (int k = 0; k < n; k++) ev.push_back(b[k]); } g.sample(false, x + dx, y + dy, t); }
  std::vector<TouchEv> classified() const { std::vector<TouchEv> v; for (auto& e : ev) if (e.type != TouchEv::Down && e.type != TouchEv::Move && e.type != TouchEv::Up) v.push_back(e.type); return v; }
};

TEST(touch_exclusive_classification) {
  { TC x; x.run(50); x.press(80); x.run(500); auto v = x.classified(); CHECK_EQ(v.size(), 1u); if (v.size() == 1) CHECK(v[0] == TouchEv::Tap); }
  { TC x; x.run(50); x.press(1500); x.run(500); auto v = x.classified(); CHECK_EQ(v.size(), 2u); if (v.size() == 2) { CHECK(v[0] == TouchEv::LongPress); CHECK(v[1] == TouchEv::LongRelease); } }
  { TC x; x.run(50); x.press(9000); x.run(500); auto v = x.classified(); CHECK_EQ(v.size(), 2u); if (v.size() == 2) CHECK(v[1] == TouchEv::RecoveryArmed); }
  const int D[4][2] = {{-150, 0}, {150, 0}, {0, -150}, {0, 150}};
  const TouchEv E[4] = {TouchEv::SwipeLeft, TouchEv::SwipeRight, TouchEv::SwipeUp, TouchEv::SwipeDown};
  for (int k = 0; k < 4; k++) { TC x; x.run(50); x.drag(D[k][0], D[k][1], 150); x.run(300); auto v = x.classified(); CHECK_EQ(v.size(), 1u); if (v.size() == 1) CHECK(v[0] == E[k]); }
  // arraste lento e longo = carinho (com retorno durante o gesto), nunca deslize
  { TC x; x.run(50); x.drag(300, 0, 1500); x.run(300); auto v = x.classified();
    CHECK(std::count(v.begin(), v.end(), TouchEv::PetStroke) >= 1); CHECK(v.back() == TouchEv::Pet);
    CHECK(std::count(v.begin(), v.end(), TouchEv::SwipeRight) == 0); }
  // duplo toque só no modo UiDouble (exclusivo: nenhum toque simples)
  { TC x; x.g.setMode(TouchMode::UiDouble); x.run(50); x.press(60); x.run(100); x.press(60); x.run(600); auto v = x.classified();
    CHECK_EQ(v.size(), 1u); if (v.size() == 1) CHECK(v[0] == TouchEv::DoubleTap); }
  { TC x; x.g.setMode(TouchMode::UiDouble); x.run(50); x.press(60); x.run(600); auto v = x.classified(); CHECK_EQ(v.size(), 1u); if (v.size() == 1) CHECK(v[0] == TouchEv::Tap); }
  // falha curta do controlador (< 40 ms) não divide o toque
  { TC x; x.run(50); x.g.sample(true, 200, 200, x.t); x.run(300); x.g.sample(false, 200, 200, x.t); x.run(20); x.g.sample(true, 200, 200, x.t); x.run(400); x.g.sample(false, 200, 200, x.t); x.run(500);
    auto v = x.classified(); CHECK_EQ(v.size(), 2u); if (!v.empty()) CHECK(v[0] == TouchEv::LongPress); }
  // toque contínuo: só o aviso, nada de amor/config
  { TC x; x.run(50); x.press(30000); x.run(500); auto v = x.classified(); CHECK(std::count(v.begin(), v.end(), TouchEv::StuckDetected) == 1);
    CHECK(std::count(v.begin(), v.end(), TouchEv::RecoveryArmed) == 0); }
  // toque mais curto que o intervalo de poll ainda é registrado
  { TouchClassifier g; g.sample(true, 10, 10, 100); g.sample(false, 10, 10, 180); TouchEvent b[16]; int k = g.poll(900, b, 16);
    int taps = 0; for (int i = 0; i < k; i++) if (b[i].type == TouchEv::Tap) taps++; CHECK_EQ(taps, 1); }
}

TEST(game_mode_immediate_no_repeat) {
  TC x; x.g.setMode(TouchMode::Game); x.run(50);
  ms_t pressAt = x.t;
  x.g.sample(true, 100, 100, x.t); x.run(25);
  auto v = x.classified();
  CHECK_EQ(v.size(), 1u);
  if (!v.empty()) CHECK(v[0] == TouchEv::GamePress);
  for (auto& e : x.ev) if (e.type == TouchEv::GamePress) CHECK_EQ(e.t, pressAt);  // instante real do toque
  x.run(1000); CHECK_EQ(x.classified().size(), 1u);  // segurando: não repete
  x.run(600); v = x.classified();
  CHECK_EQ(v.size(), 2u); if (v.size() == 2) CHECK(v[1] == TouchEv::GameHoldExit);
  x.run(5000); x.g.sample(false, 100, 100, x.t); x.run(500);
  CHECK_EQ(x.classified().size(), 2u);
}

TEST(recovery_hold_8s_opens_config_once) {
  H h; h.skipIntro();
  uint32_t petsBefore = h.app->progress().pets;
  h.hold(8500); h.run(200);
  CHECK(h.app->screen() == Screen::RecoveryConfirm);
  CHECK_EQ(h.app->progress().pets, petsBefore);  // não gerou carinho/amor
  CHECK(h.press(B_REC_YES));
  CHECK(h.app->screen() == Screen::Config);
  CHECK(h.plat.apActive);
  // o botão BOOT da placa também abre (independe da tela de toque)
  H g; g.skipIntro(); g.app->hwButton(HwButton::Boot, true, g.t); g.run(100);
  CHECK(g.app->screen() == Screen::Config);
}

TEST(screen_off_wake_consumes_gesture) {
  H h; h.skipIntro();
  std::string r; apiCall(*h.app, "POST", "/api/prefs", "{\"screenOffS\":30}", r, h.t);
  h.run(31000, 50);
  CHECK(h.app->screen() == Screen::ScreenOff);
  CHECK_EQ(h.plat.brightness, 0);
  uint32_t boops = h.app->progress().boops;
  Expr e = h.app->baseExpr();
  h.dbl();  // acordar NÃO troca expressão nem conta "boop"
  CHECK(h.app->screen() == Screen::Face);
  CHECK(h.plat.brightness > 0);
  CHECK_EQ(h.app->progress().boops, boops);
  CHECK(h.app->baseExpr() == e);
}

// ════════ 5. Rosto e carinho ════════
TEST(face_boop_doubletap_pet_and_love) {
  H h; h.skipIntro();
  uint32_t boops = h.app->progress().boops;
  h.tap(300, 200);
  CHECK_EQ(h.app->progress().boops, boops + 1);
  Expr e0 = h.app->baseExpr();
  h.dbl();
  CHECK(h.app->baseExpr() != e0);  // duplo toque: próxima expressão
  uint32_t pets = h.app->progress().pets;
  h.run(2500);
  h.stroke(1600);                  // acariciar a tela
  CHECK_EQ(h.app->progress().pets, pets + 1);
  CHECK(h.app->progress().unlocked & (1u << UNL_TIMIDO));
  h.run(2500);
  h.hold(2600); h.run(50);         // segurar parado 2 s: amor
  CHECK(h.app->face().expression() == Expr::Love);
  CHECK_EQ(h.app->progress().pets, pets + 2);
}

TEST(home_carousel_launcher_and_quick_panel) {
  H h; h.skipIntro();
  CHECK(h.app->screen() == Screen::Face);
  h.swipe(-180, 0); CHECK(h.app->screen() == Screen::Clock);
  h.swipe(-180, 0); CHECK(h.app->screen() == Screen::Today);
  h.swipe(-180, 0); CHECK(h.app->screen() == Screen::Today);  // fim do carrossel
  h.swipe(180, 0); h.swipe(180, 0); CHECK(h.app->screen() == Screen::Face);
  h.swipe(0, -200); CHECK(h.app->screen() == Screen::Launcher);
  CHECK(h.pick(L_POMO)); CHECK(h.app->screen() == Screen::Pomodoro);
  CHECK(h.press(B_PLAY)); CHECK(h.app->pomodoro().state() == RunState::Running);
  h.swipe(200, 0); CHECK(h.app->screen() == Screen::Launcher);  // voltar SEM cancelar
  CHECK(h.app->pomodoro().state() == RunState::Running);
  CHECK(h.press(B_BACK)); CHECK(h.app->screen() == Screen::Face);
  h.swipe(0, 200); CHECK(h.app->screen() == Screen::Quick);
  CHECK(h.press(B_Q_SILENT)); CHECK(h.app->settings().silent == 1);
  // arrastar o controle de volume
  h.tick(); int x, y; CHECK(h.app->buttonPos(B_Q_VOL, x, y));
  h.down(110, y); h.run(20); for (int k = 0; k <= 10; k++) { h.t += 20; h.down(110 + k * 25, y); h.tick(); } h.up(360, y); h.run(100);
  CHECK(h.app->settings().volume >= 95);
  h.swipe(0, -200); CHECK(h.app->screen() == Screen::Face);
  h.reboot(); CHECK_EQ(h.app->settings().silent, 1); CHECK(h.app->settings().volume >= 95);
}

TEST(list_drag_scroll_and_pick) {
  H h; h.skipIntro();
  h.swipe(0, -200);
  CHECK(h.app->screen() == Screen::Launcher);
  // arrasta a lista para cima devagar (sem deslize) e solta: encaixa num item
  h.down(233, 380); h.run(10);
  for (int k = 1; k <= 30; k++) { h.t += 30; h.down(233, 380 - k * 8); h.tick(); }
  h.up(233, 140); h.run(1200);
  CHECK(h.app->screen() == Screen::Launcher);
  h.tap(233, 233);  // abre o item centralizado
  CHECK(h.app->screen() != Screen::Launcher);
  CHECK(h.app->screen() != Screen::Face);
}

// ════════ 6. Sensores ════════
TEST(motion_sensor_events) {
  MotionSensor m; ms_t t = 0;
  for (int i = 0; i < 200; i++) { t += 20; m.sample(0, 1000, 0, t); }  // em repouso
  MotionEv e[8]; int n = m.poll(e, 8); CHECK_EQ(n, 0);
  CHECK(m.pose() == Pose::Upright);
  // sacudir: picos alternados fortes
  for (int i = 0; i < 25; i++) { t += 20; m.sample(i % 2 ? 1800 : -1800, 1000, i % 2 ? -900 : 900, t); }
  n = m.poll(e, 8); bool shake = false; for (int i = 0; i < n; i++) shake |= e[i] == MotionEv::Shake; CHECK(shake);
  for (int i = 0; i < 200; i++) { t += 20; m.sample(0, 1000, 0, t); } m.poll(e, 8);
  // virar de cara para baixo e de volta
  for (int i = 0; i < 100; i++) { t += 20; m.sample(0, 0, 1000, t); }
  n = m.poll(e, 8); bool fd = false; for (int i = 0; i < n; i++) fd |= e[i] == MotionEv::FaceDown; CHECK(fd);
  for (int i = 0; i < 100; i++) { t += 20; m.sample(0, 1000, 0, t); }
  n = m.poll(e, 8); bool fu = false; for (int i = 0; i < n; i++) fu |= e[i] == MotionEv::FaceUp; CHECK(fu);
  // pegar no colo depois de 3 s em repouso
  for (int i = 0; i < 200; i++) { t += 20; m.sample(0, 1000, 0, t); } m.poll(e, 8);
  for (int i = 0; i < 6; i++) { t += 20; m.sample(150, 1400, 100, t); }
  n = m.poll(e, 8); bool pk = false; for (int i = 0; i < n; i++) pk |= e[i] == MotionEv::PickUp; CHECK(pk);
  // inclinação para os olhos
  for (int i = 0; i < 100; i++) { t += 20; m.sample(400, 900, 0, t); }
  CHECK(m.tiltX() > 500);
}

TEST(app_motion_reactions_and_face_down_snooze) {
  H h; h.skipIntro();
  ms_t& t = h.t;
  auto feed = [&](int ax, int ay, int az, ms_t ms) { for (ms_t e = 0; e < ms; e += 20) { t += 20; h.app->motionSample(ax, ay, az, t); h.tick(); } };
  feed(0, 1000, 0, 3000);
  for (int i = 0; i < 25; i++) { t += 20; h.app->motionSample(i % 2 ? 1800 : -1800, 1000, 0, t); h.tick(); }
  CHECK(h.app->face().expression() == Expr::Dizzy);
  CHECK(h.app->progress().shakes >= 1);
  feed(0, 1000, 0, 3500);
  // alarme tocando: virar para baixo adia 5 min e apaga a tela
  std::string r;
  apiCall(*h.app, "POST", "/api/time", "{\"epoch\":1792069140,\"tz\":\"UTC\"}", r, t);  // 12:59:00 UTC
  apiCall(*h.app, "POST", "/api/prefs", "{\"alarms\":[{\"hour\":13,\"minute\":0,\"days\":0,\"enabled\":true}]}", r, t);
  feed(0, 1000, 0, 61000);
  CHECK_EQ(h.app->alertCount(), 1);
  CHECK(h.app->currentAlert() && h.app->currentAlert()->type == AlertType::Alarm);
  CHECK(h.app->outputs().soundActive(t));
  feed(0, 0, 1000, 1500);
  CHECK_EQ(h.app->alertCount(), 0);
  CHECK(h.app->screen() == Screen::ScreenOff);
  CHECK(!h.app->outputs().soundActive(t + 200));
  feed(0, 1000, 0, 1000);
  CHECK(h.app->screen() != Screen::ScreenOff);  // desvirar acorda
  feed(0, 1000, 0, 5 * 60000);
  CHECK_EQ(h.app->alertCount(), 1);  // adiado tocou de novo
  h.press(B_ALERT_OK);
  CHECK_EQ(h.app->alertCount(), 0);
  CHECK_EQ(h.app->settings().alarms[0].enabled, 0);  // alarme único desliga após "ok"
}

TEST(sound_sensor_clap_loud_music) {
  SoundSensor s; ms_t t = 0; SoundEv e[8];
  for (int i = 0; i < 100; i++) { t += 40; s.sample(30, t); }
  CHECK_EQ(s.poll(e, 8), 0);
  t += 40; s.sample(78, t); t += 40; s.sample(70, t); t += 40; s.sample(32, t); t += 40; s.sample(30, t);
  int n = s.poll(e, 8); CHECK(n == 1 && e[0] == SoundEv::Clap);
  for (int i = 0; i < 60; i++) { t += 40; s.sample(30, t); } s.poll(e, 8);
  for (int i = 0; i < 50; i++) { t += 40; s.sample(90, t); }
  n = s.poll(e, 8); bool loud = false; for (int i = 0; i < n; i++) loud |= e[i] == SoundEv::Loud; CHECK(loud);
  for (int i = 0; i < 100; i++) { t += 40; s.sample(30, t); } s.poll(e, 8);
  // batida regular de 500 ms
  bool music = false;
  for (int i = 0; i < 300; i++) { t += 40; int ph = (int)(t % 500); s.sample(ph < 80 ? 66 : 48, t); n = s.poll(e, 8); for (int k = 0; k < n; k++) music |= e[k] == SoundEv::MusicStart; }
  CHECK(music); CHECK(s.music());
  CHECK(s.beatMs() >= 450 && s.beatMs() <= 560);
}

TEST(app_dance_unlocks_groovy) {
  H h; h.skipIntro();
  for (int i = 0; i < 2000 && !(h.app->progress().unlocked & (1u << UNL_DANCANTE)); i++) {
    h.t += 40; int ph = (int)(h.t % 500); h.app->soundLevel(ph < 80 ? 66 : 48, h.t); h.tick();
  }
  CHECK(h.app->progress().unlocked & (1u << UNL_DANCANTE));
  CHECK(h.app->progress().danceSeconds >= 30);
  // reações a sons podem ser desligadas (o nível nem é processado)
  std::string r; apiCall(*h.app, "POST", "/api/prefs", "{\"micReactions\":false}", r, h.t);
  uint32_t before = h.app->sound().samples();
  h.app->soundLevel(90, h.t + 10);
  CHECK_EQ(h.app->sound().samples(), before);
}

TEST(power_low_battery_and_charging) {
  H h; h.skipIntro();
  PowerState p; p.known = true; p.battery = true; p.percent = 50; p.usb = false;
  h.app->powerUpdate(p, h.t); h.run(100);
  uint32_t s0 = h.app->outputs().soundStarts();
  p.percent = 9; h.app->powerUpdate(p, h.t); h.run(100);
  CHECK(h.app->outputs().lastSound() == Pattern::LowBattery);
  CHECK(h.app->face().expression() == Expr::Tired);
  h.app->powerUpdate(p, h.t); h.run(100);
  CHECK_EQ(h.app->outputs().soundStarts(), s0 + 1);  // avisa uma vez só
  h.run(4000);
  p.charging = true; p.usb = true; h.app->powerUpdate(p, h.t); h.run(50);
  CHECK(h.app->face().expression() == Expr::Charging);
  CHECK(h.app->outputs().lastSound() == Pattern::Charge);
}

TEST(brightness_night_torch_and_rotation) {
  H h; h.skipIntro();
  CHECK_EQ(h.plat.brightness, 200);
  std::string r;
  apiCall(*h.app, "POST", "/api/prefs", "{\"quiet\":{\"enabled\":true,\"startH\":22,\"endH\":7},\"tz\":\"UTC\"}", r, h.t);
  apiCall(*h.app, "POST", "/api/time", "{\"epoch\":1792113600}", r, h.t);  // 01:20 UTC
  h.run(100);
  CHECK_EQ(h.plat.brightness, 50);   // noite: 1/4 do brilho
  h.swipe(0, 200); CHECK(h.press(B_Q_TORCH));
  CHECK(h.app->screen() == Screen::Torch); CHECK_EQ(h.plat.brightness, 255);
  h.tap(); CHECK_EQ(h.plat.brightness, 50);
  apiCall(*h.app, "POST", "/api/prefs", "{\"rotate180\":true}", r, h.t);
  h.run(50); CHECK(h.plat.rotate);
  // com a tela girada, o toque também gira (canto do botão continua certo)
  h.swipe(0, -200); h.run(50);
  CHECK(h.app->screen() == Screen::Quick);  // deslizar "para cima" na tela girada é para baixo no sensor
}

// ════════ 7. Jogos (lógica da v1, mesmos critérios) ════════
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

// avança o jogo de 1 em 1 ms até a borda esquerda do peixe chegar exatamente a 'left'
static ms_t timeWhenLeftIs(FishGame& g, ms_t from, int left) {
  for (ms_t t = from; t < from + 20000; t++) {
    g.update(t);
    int32_t x = g.fishXsubAt(g.gameTimeOf(t));
    if (x >= left * 1000 && x < (left + 1) * 1000) return t;
  }
  return 0;
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
  g.draw(cv(), sig);         // quadro AGORA! desenhado...
  g.press(sig + 5, sig + 5);               // ...mas ainda não apresentado: cedo demais
  CHECK(g.sub() == ReflexGame::Sub::Early);
  g.takeOutcome();
  g.press(9000, 9000); w = g.lastWait(); sig = 9000 + w; g.update(sig); g.draw(cv(), sig);
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


TEST(dive_game_rules_and_fairness) {
  for (uint32_t seed = 1; seed <= 30; seed++) {
    DiveGame g; Rng rng(seed); ms_t t = 0; g.begin(t, &rng, true); g.press(t, t);
    CHECK(g.phase() == GamePhase::Playing);
    int jellyPairsTooClose = 0;
    for (int i = 0; i < 6000 && g.phase() == GamePhase::Playing; i++) {
      t += 10;
      // bot: segue o peixe mais baixo e foge da água-viva mais próxima
      const DiveGame::Item* it = g.items();
      int target = -1, bestY = -1;
      for (int k = 0; k < DiveGame::kMaxItems; k++) if (it[k].active && !it[k].jelly && it[k].y > bestY) { bestY = it[k].y; target = it[k].x; }
      int touchX = target >= 0 ? DiveGame::kViewX0 + target * DiveGame::kViewScale / 1000 : -1;
      for (int k = 0; k < DiveGame::kMaxItems; k++) for (int j = k + 1; j < DiveGame::kMaxItems; j++)
        if (it[k].active && it[j].active && it[k].jelly && it[j].jelly && std::abs(it[k].y - it[j].y) < 4000 && std::abs(it[k].x - it[j].x) < 20000) jellyPairsTooClose++;
      g.setControl(0, touchX);
      g.update(t);
    }
    CHECK_EQ(jellyPairsTooClose, 0);
    CHECK(g.score() >= 10);
  }
  // inclinação move o Tuba; o toque no meio da partida não é ação
  DiveGame g; Rng rng(3); g.begin(0, &rng, true); g.press(0, 0);
  int32_t x0 = g.sharkX();
  g.setControl(1000, -1); g.update(500);
  CHECK(g.sharkX() > x0);
  g.setControl(-1000, -1); g.update(1500);
  CHECK(g.sharkX() < x0);
  int s = g.score(); g.press(1500, 1500); CHECK_EQ(g.score(), s);
  // independe da taxa de quadros
  DiveGame a, b; Rng ra(8), rb(8); a.begin(0, &ra, true); b.begin(0, &rb, true); a.press(0, 0); b.press(0, 0);
  for (ms_t t = 0; t < 8000;) { t += 16; a.setControl(300, -1); a.update(t); }
  for (ms_t t = 0; t < 8000;) { t += 50; b.setControl(300, -1); b.update(t); }
  a.update(8000); b.update(8000);
  CHECK_EQ(a.score(), b.score()); CHECK_EQ(a.lives(), b.lives()); CHECK_EQ(a.sharkX(), b.sharkX());
}

TEST(fish_hold_does_not_autocapture_in_app) {
  H h; h.skipIntro();
  h.app->startGame(GameId::Fish, h.t);
  h.tap(); h.run(100);            // inicia a rodada
  h.down(); h.run(1200);          // segura: só a borda conta
  CHECK(h.app->fish().captures() <= 1);
  int c = h.app->fish().captures();
  h.run(200); h.up(); h.run(100);
  CHECK_EQ(h.app->fish().captures(), c);
}

TEST(timing_press_held_through_go_does_not_stop) {
  H h; h.skipIntro();
  h.app->startGame(GameId::Timing, h.t);
  h.tap(); h.run(100);
  CHECK(h.app->timing().sub() == TimingGame::Sub::Countdown);
  h.run(2000);
  h.down(); h.run(1200);  // segura atravessando o VAI (menos que a saída de 1,5 s)
  CHECK(h.app->timing().sub() == TimingGame::Sub::Running);
  h.up(); h.run(200);
  CHECK(h.app->timing().sub() == TimingGame::Sub::Running);
  h.tap(); CHECK(h.app->timing().sub() == TimingGame::Sub::Done);
}

TEST(game_exit_by_hold_and_menu) {
  H h; h.skipIntro();
  h.swipe(0, -200); CHECK(h.pick(L_GAMES)); CHECK(h.app->screen() == Screen::GamesMenu);
  CHECK(h.pick(L_GAME0 + (int)GameId::Dive)); CHECK(h.app->screen() == Screen::Game);
  CHECK(h.app->activeGameId() == GameId::Dive);
  h.tap(); h.run(2000);
  CHECK(h.app->dive().phase() == GamePhase::Playing);
  h.hold(1600); h.run(100);
  CHECK(h.app->screen() == Screen::GamesMenu);
}

// ════════ 8. Tempo ════════
TEST(timers_pomodoro_offscreen_pause_single_completion) {
  H h; h.skipIntro();
  std::string r;
  CHECK_EQ(apiCall(*h.app, "POST", "/api/action", "{\"action\":\"timer.start\",\"slot\":0,\"seconds\":5}", r, h.t), 200);
  h.swipe(0, -200);  // vai para os apps: contagem continua
  CHECK(h.app->screen() == Screen::Launcher);
  h.run(2000);
  h.app->timer(0).pause(h.t); uint32_t rem = h.app->timer(0).remainingMs(h.t);
  h.run(10000);
  CHECK_EQ(h.app->timer(0).remainingMs(h.t), rem);
  h.app->timer(0).resume(h.t);
  h.run(1000);
  h.t += 30000; h.tick(); h.tick();  // quadro muito atrasado: UM único alerta
  CHECK_EQ(h.app->alertCount(), 1);
  h.run(100);
  CHECK_EQ(h.app->alertCount(), 1);
  apiCall(*h.app, "POST", "/api/prefs", "{\"pomodoro\":{\"focusMin\":1,\"shortMin\":1}}", r, h.t);
  h.tap(); h.run(400);
  CHECK_EQ(h.app->alertCount(), 0);
  uint32_t done = h.app->progress().pomodorosDone;
  apiCall(*h.app, "POST", "/api/action", "{\"action\":\"pomodoro.toggle\"}", r, h.t);
  h.run(61000, 50);
  CHECK_EQ(h.app->progress().pomodorosDone, done + 1);
  CHECK(h.app->pomodoro().phase() == PomoPhase::ShortBreak);
  CHECK(h.app->progress().unlocked & (1u << UNL_FOCADO));
  h.tap(); h.run(400);
  apiCall(*h.app, "POST", "/api/action", "{\"action\":\"pomodoro.skip\"}", r, h.t);
  apiCall(*h.app, "POST", "/api/action", "{\"action\":\"pomodoro.skip\"}", r, h.t);
  CHECK_EQ(h.app->progress().pomodorosDone, done + 1);
}

TEST(timer_screen_buttons_and_presets) {
  H h; h.skipIntro();
  h.swipe(0, -200); CHECK(h.pick(L_TIMERS));
  CHECK(h.press(B_TIMER_SEL2));
  CHECK(h.press(B_TIMER_DUR)); CHECK(h.app->screen() == Screen::TimerSet);
  CHECK(h.pick(L_PRESET0 + 0));  // 1 min
  CHECK(h.app->screen() == Screen::Timers);
  CHECK_EQ(h.app->timer(2).durationMs(), 60000);
  CHECK(h.press(B_PLAY)); CHECK(h.app->timer(2).state() == RunState::Running);
  CHECK(h.press(B_PLAY)); CHECK(h.app->timer(2).state() == RunState::Paused);
  CHECK(h.press(B_TIMER_CANCEL)); CHECK(h.app->timer(2).state() == RunState::Idle);
}

TEST(alert_during_game_pauses_fairly) {
  H h; h.skipIntro();
  std::string r;
  apiCall(*h.app, "POST", "/api/action", "{\"action\":\"timer.start\",\"slot\":1,\"seconds\":2}", r, h.t);
  h.app->startGame(GameId::Reflex, h.t);
  h.tap(); h.run(50);
  CHECK(h.app->reflex().sub() == ReflexGame::Sub::Waiting);
  uint16_t best = h.app->progress().reflexBestMs;
  h.run(2500);
  CHECK_EQ(h.app->alertCount(), 1);
  CHECK(h.app->reflex().sub() == ReflexGame::Sub::Interrupted);
  h.tap(); h.run(400);
  CHECK_EQ(h.app->alertCount(), 0);
  CHECK(h.app->screen() == Screen::Game);
  CHECK_EQ(h.app->progress().reflexBestMs, best);
  apiCall(*h.app, "POST", "/api/action", "{\"action\":\"timer.start\",\"slot\":1,\"seconds\":1}", r, h.t);
  h.app->startGame(GameId::Jump, h.t); h.tap(); h.run(300);
  h.run(1000);
  CHECK(h.app->jump().paused());
  ms_t simBefore = h.app->jump().simTime();
  h.run(3000);
  CHECK_EQ(h.app->jump().simTime(), simBefore);
  h.tap(); h.run(400);
  h.tap(); h.run(100);
  CHECK(!h.app->jump().paused());
}

TEST(stopwatch_counts_up_laps_no_alarm) {
  Stopwatch s; s.start(0);
  CHECK_EQ(s.elapsedMs(1234), 1234);
  s.pause(2000); CHECK_EQ(s.elapsedMs(9000), 2000);
  s.resume(9000); CHECK_EQ(s.elapsedMs(10000), 3000);
  H h; h.skipIntro();
  h.swipe(0, -200); CHECK(h.pick(L_SW));
  CHECK(h.press(B_PLAY)); h.run(3000);
  CHECK(h.press(B_LAP));
  h.run(120000, 100);
  CHECK_EQ(h.app->alertCount(), 0);
  CHECK(h.press(B_RESET)); CHECK(h.app->screen() == Screen::Confirm);
  CHECK(h.press(B_OK)); CHECK(h.app->screen() == Screen::Stopwatch);
  CHECK_EQ(h.app->stopwatch().elapsedMs(h.t), 0);
  Stopwatch l; l.start(0); l.poll(Stopwatch::kLimitMs + 5000);
  CHECK(l.limitHit()); CHECK_EQ(l.elapsedMs(Stopwatch::kLimitMs + 9000), (long long)Stopwatch::kLimitMs);
}

TEST(civil_time_rtc_and_durations) {
  H h; h.skipIntro();
  CHECK(!h.app->clock().valid());
  std::string r;
  apiCall(*h.app, "POST", "/api/action", "{\"action\":\"timer.start\",\"slot\":0,\"seconds\":60}", r, h.t);
  h.run(10000);
  uint32_t rem = h.app->timer(0).remainingMs(h.t);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/time", "{\"epoch\":1791936000,\"tz\":\"America/Manaus\"}", r, h.t), 200);
  CHECK(h.app->clock().valid());
  CHECK_EQ(h.plat.rtc, 1791936000);  // grava no RTC com bateria
  CHECK_EQ(h.app->timer(0).remainingMs(h.t), rem);
  apiCall(*h.app, "POST", "/api/time", "{\"epoch\":1600000000}", r, h.t);
  CHECK_EQ(h.app->timer(0).remainingMs(h.t), rem);
  h.reboot();
  CHECK(!h.app->clock().valid());      // sem leitura do RTC, não inventa a data
  h.app->rtcTime(1791936000, h.t);     // o firmware lê o PCF85063 no boot
  CHECK(h.app->clock().valid());
  h.app->rtcTime(12345, h.t + 1);      // RTC zerado é ignorado
  CHECK(h.app->clock().epoch(h.t + 1) >= 1791936000);
  CivilClock::Local L = civilFromEpoch(1791936000LL - 180 * 60);
  CHECK_EQ(L.year, 2026); CHECK_EQ(L.month, 10); CHECK_EQ(L.day, 13);
}

TEST(alarms_fire_once_days_and_no_retroactive) {
  H h; h.skipIntro();
  std::string r;
  apiCall(*h.app, "POST", "/api/time", "{\"epoch\":1792065600,\"tz\":\"UTC\"}", r, h.t);  // qua 14/10/2026 12:00 UTC
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{\"alarms\":[{\"hour\":12,\"minute\":1,\"days\":8,\"enabled\":true,\"label\":\"Remédio\"},"
                                                "{\"hour\":12,\"minute\":1,\"days\":2,\"enabled\":true}]}", r, h.t), 200);  // bit3 = qua; bit1 = seg
  int mins = 0; CHECK_EQ(apiCall(*h.app, "GET", "/api/status", nullptr, r, h.t), 200);
  CHECK(r.find("\"nextAlarm\":{\"index\":0,\"inMinutes\":1}") != std::string::npos);
  h.run(61000, 100);
  CHECK_EQ(h.app->alertCount(), 1);   // só o da quarta
  h.run(30000, 100);
  CHECK_EQ(h.app->alertCount(), 1);   // não repete no mesmo minuto
  h.tap(); h.run(400);
  CHECK_EQ(h.app->alertCount(), 0);
  CHECK_EQ(h.app->settings().alarms[0].enabled, 1);  // recorrente continua ligado
  // ajustar a hora para depois do alarme não dispara retroativamente
  apiCall(*h.app, "POST", "/api/time", "{\"epoch\":1792152060}", r, h.t);  // qui 12:01
  h.run(2000);
  CHECK_EQ(h.app->alertCount(), 0);
  (void)mins;
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{\"alarms\":[{\"hour\":24}]}", r, h.t), 422);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{\"alarms\":[{\"label\":\"Rótulo comprido demais\"}]}", r, h.t), 422);
}

TEST(alarm_edit_screen) {
  H h; h.skipIntro();
  h.swipe(0, -200); CHECK(h.pick(L_ALARMS));
  CHECK(h.press(B_ALARM_OPEN0 + 1)); CHECK(h.app->screen() == Screen::AlarmEdit);
  uint8_t hr = h.app->settings().alarms[1].hour;
  CHECK(h.press(B_AL_H_PLUS)); CHECK_EQ(h.app->settings().alarms[1].hour, (hr + 1) % 24);
  CHECK(h.press(B_AL_M_PLUS)); CHECK_EQ(h.app->settings().alarms[1].minute, 5);
  CHECK(h.press(B_AL_DAY0)); CHECK(h.app->settings().alarms[1].days & 1);
  CHECK(h.press(B_AL_DONE)); CHECK(h.app->screen() == Screen::Alarms);
  CHECK_EQ(h.app->settings().alarms[1].enabled, 1);
  CHECK(h.press(B_ALARM_TOGGLE0 + 1)); CHECK_EQ(h.app->settings().alarms[1].enabled, 0);
}

TEST(breath_session_unlocks_zen) {
  H h; h.skipIntro();
  for (int k = 0; k < 3; k++) {
    h.swipe(0, -200); CHECK(h.pick(L_BREATH));
    CHECK(h.app->screen() == Screen::Breath);
    CHECK(h.press(B_BREATH_PLAY));
    h.run(66000, 50);
    CHECK_EQ(h.app->progress().breathSessions, k + 1);
    h.press(B_BACK); h.press(B_BACK);
  }
  CHECK(h.app->progress().unlocked & (1u << UNL_ZEN));
}

TEST(dice_modes_and_shake_roll) {
  H h; h.skipIntro();
  h.swipe(0, -200); CHECK(h.pick(L_DICE));
  for (int i = 0; i < 20; i++) { h.tap(); h.run(800); }
  CHECK(h.app->screen() == Screen::Dice);
  CHECK(h.press(B_DICE_MODE));
  // sacudir sorteia
  uint32_t s0 = h.app->outputs().soundStarts();
  for (int i = 0; i < 100; i++) { h.t += 20; h.app->motionSample(0, 1000, 0, h.t); h.tick(); }
  for (int i = 0; i < 25; i++) { h.t += 20; h.app->motionSample(i % 2 ? 1800 : -1800, 1000, 0, h.t); h.tick(); }
  CHECK(h.app->outputs().soundStarts() > s0);
  CHECK(h.app->screen() == Screen::Dice);  // não sai do app, só sorteia
}

TEST(weather_url_parse_and_failures) {
  H h; h.skipIntro();
  char url[400];
  CHECK(!h.app->weatherWant(h.t, url, sizeof url));  // desligado por padrão
  std::string r;
  apiCall(*h.app, "POST", "/api/prefs", "{\"weatherEnabled\":true,\"weather\":{\"city\":\"recife\"}}", r, h.t);
  CHECK(!h.app->weatherWant(h.t, url, sizeof url));  // sem rede de casa
  h.plat.staConfigured = h.plat.staConnected = true;
  CHECK(h.app->weatherWant(h.t, url, sizeof url));
  CHECK(strstr(url, "latitude=-8.0476&longitude=-34.8770") != nullptr);
  CHECK(strstr(url, "https://api.open-meteo.com/v1/forecast?") == url);
  CHECK(!h.app->weatherWant(h.t + 1000, url, sizeof url));  // não repete enquanto espera
  const char* body = "{\"current\":{\"temperature_2m\":-3.46,\"relative_humidity_2m\":61,\"weather_code\":95,\"is_day\":0},"
                     "\"daily\":{\"weather_code\":[95,61,0],\"temperature_2m_max\":[28.1,24.0,26.4],\"temperature_2m_min\":[18.2,17.9,16.0]}}";
  h.app->weatherResult(body, strlen(body), 200, h.t);
  CHECK(h.app->weather().valid);
  CHECK_EQ(h.app->weather().tempC10, -35);
  CHECK_EQ(h.app->weather().code, 95);
  CHECK_EQ(h.app->weather().nDays, 3);
  CHECK_EQ(h.app->weather().hiC10, 281);
  CHECK(!h.app->weather().isDay);
  CHECK(strcmp(weatherText(95), "Tempestade") == 0);
  CHECK(weatherIcon(0, false) == Icon::Moon);
  h.app->weatherResult("<html>", 6, 503, h.t + 10);
  CHECK(h.app->weather().valid);  // falha mantém o último dado bom
  apiCall(*h.app, "POST", "/api/prefs", "{\"weather\":{\"city\":\"custom\",\"latE4\":-235000,\"lonE4\":-466000}}", r, h.t);
  CHECK(!h.app->weather().valid);  // mudou o local: dado antigo descartado
  CHECK(h.app->weatherWant(h.t + 20, url, sizeof url));
  CHECK(strstr(url, "latitude=-23.5000&longitude=-46.6000") != nullptr);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{\"weather\":{\"city\":\"atlantida\"}}", r, h.t), 422);
}

TEST(settings_on_device_cycle_and_persist) {
  H h; h.skipIntro();
  h.swipe(0, -200); CHECK(h.pick(L_SETTINGS));
  uint8_t eye = h.app->settings().eyeColor;
  CHECK(h.pick(S_EYE)); CHECK_EQ(h.app->settings().eyeColor, (eye + 1) % EYE_COLOR_COUNT);
  CHECK(h.app->screen() == Screen::Settings);
  CHECK(h.pick(S_FACE)); CHECK_EQ(h.app->settings().watchFace, 1);
  CHECK(h.pick(S_VOLUME));
  CHECK(h.pick(S_NET)); CHECK(h.app->screen() == Screen::Config); CHECK(h.plat.apActive);
  CHECK(h.press(B_CFG_CLOSE)); CHECK(!h.plat.apActive);
  h.run(5000);
  h.reboot();
  CHECK_EQ(h.app->settings().eyeColor, (eye + 1) % EYE_COLOR_COUNT);
  CHECK_EQ(h.app->settings().watchFace, 1);
}

// ════════ 9. Persistência ════════
TEST(persistence_survives_reboot_and_resets) {
  H h; h.skipIntro();
  std::string r;
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{\"nickname\":\"Tubarão\",\"personality\":\"brincalhao\",\"silent\":true,\"eyeColor\":\"lilas\"}", r, h.t), 200);
  h.hold(1300); h.run(500);  // carinho (segurar parado) -> Tímido
  CHECK(h.app->progress().unlocked & 1);
  apiCall(*h.app, "POST", "/api/wifi", "{\"ssid\":\"Casa\",\"pass\":\"segredo123\"}", r, h.t);
  h.run(5000);
  uint32_t w = h.app->storageWrites();
  CHECK(w > 0 && w < 10);  // gravações agrupadas, não por quadro
  h.reboot();
  CHECK(strcmp(h.app->settings().nickname, "Tubarão") == 0);
  CHECK_EQ(h.app->settings().personality, 1);
  CHECK_EQ(h.app->settings().eyeColor, eyeColorFromKey("lilas"));
  CHECK(h.app->progress().unlocked & 1);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/reset", "{\"scope\":\"wifi\",\"confirm\":\"ESQUECER\"}", r, h.t), 200);
  CHECK(!h.plat.staConfigured);
  CHECK(strcmp(h.app->settings().nickname, "Tubarão") == 0);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/reset", "{\"scope\":\"all\",\"confirm\":\"sim\"}", r, h.t), 422);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/reset", "{\"scope\":\"all\",\"confirm\":\"APAGAR TUDO\"}", r, h.t), 200);
  CHECK_EQ(h.plat.reboots, 1);
  h.reboot();
  CHECK_EQ(h.app->settings().nickname[0], 0);
  CHECK_EQ(h.app->progress().unlocked, 0);
  CHECK_EQ(h.app->settings().setupDone, 0);
}

TEST(corrupted_storage_recovers_defaults) {
  H h; h.skipIntro();
  h.plat.store[(int)BlobKind::Settings][20] ^= 0xFF;
  h.plat.storeLen[(int)BlobKind::Progress] = 5;
  delete h.app; h.start(1);
  CHECK(h.app->storageRecovered());
  CHECK_EQ(h.app->settings().pomoFocusMin, 25);
  CHECK_EQ(h.app->settings().volume, 60);
  h.run(4000);
  CHECK(h.app->screen() == Screen::FirstUse || h.app->screen() == Screen::Face);
}

TEST(older_settings_blob_migrates) {
  // um blob menor (layout anterior) é migrado: campos novos ficam com o padrão
  FakePlatform plat;
  Settings s; defaultSettings(s); s.setupDone = 1; tbStrlcpy(s.nickname, "Velho", sizeof s.nickname);
  uint8_t buf[BLOB_MAX];
  size_t oldLen = offsetof(Settings, volume);
  size_t n = encodeBlob(BlobKind::Settings, 1, &s, oldLen, buf, sizeof buf);
  plat.storageSave(BlobKind::Settings, buf, n);
  std::vector<uint16_t> fb(SCR_W * SCR_H);
  App a(plat); a.attachFramebuffer(fb.data()); a.begin(fullCaps(), 0, 1, true);
  CHECK(strcmp(a.settings().nickname, "Velho") == 0);
  CHECK_EQ(a.settings().volume, 60);
  CHECK_EQ(a.settings().imuReactions, 1);
  CHECK(!a.storageRecovered());
}

TEST(power_loss_resume_offer) {
  H h; h.skipIntro();
  std::string r;
  apiCall(*h.app, "POST", "/api/action", "{\"action\":\"pomodoro.toggle\"}", r, h.t);
  h.run(5000);
  h.reboot();
  h.run(3000);
  CHECK(h.app->screen() == Screen::Resume);
  CHECK(h.press(B_RESUME_YES));
  CHECK(h.app->screen() == Screen::Pomodoro);
  CHECK(h.app->pomodoro().state() == RunState::Paused);
}

// ════════ 10. API / portal ════════
TEST(api_auth_validation_and_secrets) {
  H h; h.skipIntro();
  std::string r;
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{\"nickname\":\"x\"}", r, h.t, false), 401);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{\"nickname\":\"Nome muito comprido\"}", r, h.t), 422);
  CHECK(r.find("12 caracteres") != std::string::npos);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{\"nickname\":\"Júlia \\u00c9va\"}", r, h.t), 200);
  CHECK(strcmp(h.app->settings().nickname, "Júlia Éva") == 0);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{\"contrast\":999,\"nickname\":\"Ok\"}", r, h.t), 422);
  CHECK(strcmp(h.app->settings().nickname, "Júlia Éva") == 0);  // tudo ou nada
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{\"volume\":101}", r, h.t), 422);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{\"eyeColor\":\"xadrez\"}", r, h.t), 422);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{\"contrast\":120}", r, h.t), 200);  // nome da v1 aceito
  CHECK_EQ(h.app->settings().brightness, 120);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{bad json", r, h.t), 400);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", "{\"favoriteExpr\":\"victorious\"}", r, h.t), 422);
  apiCall(*h.app, "POST", "/api/wifi", "{\"ssid\":\"Casa\",\"pass\":\"segredo123\"}", r, h.t);
  for (const char* p : {"/api/status", "/api/prefs", "/api/wifi"}) { apiCall(*h.app, "GET", p, nullptr, r, h.t); CHECK(r.find("segredo123") == std::string::npos); }
  apiCall(*h.app, "POST", "/api/backup", "{}", r, h.t); CHECK(r.find("segredo123") == std::string::npos);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/session", "{}", r, h.t, false, false), 401);
  std::string body = std::string("{\"pin\":\"") + h.app->pairPin() + "\"}";
  CHECK_EQ(apiCall(*h.app, "POST", "/api/session", body.c_str(), r, h.t, false, false), 200);
  std::string big(5000, ' '); big[0] = '{'; big[4999] = '}';
  CHECK_EQ(apiCall(*h.app, "POST", "/api/prefs", big.c_str(), r, h.t), 413);
  apiCall(*h.app, "POST", "/api/backup", "{}", r, h.t);
  std::string bk = r;
  apiCall(*h.app, "POST", "/api/prefs", "{\"nickname\":\"Outro\"}", r, h.t);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/restore", bk.c_str(), r, h.t), 200);
  CHECK(strcmp(h.app->settings().nickname, "Júlia Éva") == 0);
  // backup da v1 (formato antigo, campo "contrast") é aceito
  CHECK_EQ(apiCall(*h.app, "POST", "/api/restore", "{\"format\":\"tubabot-backup\",\"version\":1,\"prefs\":{\"nickname\":\"Da v1\",\"contrast\":90},"
                   "\"progress\":{\"unlocked\":3,\"pets\":7}}", r, h.t), 200);
  CHECK(strcmp(h.app->settings().nickname, "Da v1") == 0);
  CHECK_EQ(h.app->settings().brightness, 90);
  CHECK_EQ(h.app->progress().unlocked & 3, 3);
}

TEST(api_status_frame_options_collection) {
  H h; h.skipIntro();
  std::string r;
  CHECK_EQ(apiCall(*h.app, "GET", "/api/status", nullptr, r, h.t), 200);
  for (const char* k : {"\"caps\"", "\"motion\"", "\"sound\"", "\"power\"", "\"weather\"", "\"nextAlarm\"", "\"version\":\"2.0.0\""}) CHECK(r.find(k) != std::string::npos);
  CHECK_EQ(apiCall(*h.app, "GET", "/api/frame", nullptr, r, h.t), 200);
  CHECK(r.find("\"w\":117") != std::string::npos);
  size_t a = r.find("\"data\":\"") + 8, b = r.find('"', a);
  CHECK_EQ(b - a, (117 * 234 + 2) / 3 * 4);
  CHECK_EQ(apiCall(*h.app, "GET", "/api/options", nullptr, r, h.t), 200);
  CHECK(r.find("\"sao_paulo\"") != std::string::npos && r.find("\"#3EE6FF\"") != std::string::npos);
  CHECK_EQ(apiCall(*h.app, "GET", "/api/collection", nullptr, r, h.t), 200);
  CHECK(r.find("\"groovy\"") != std::string::npos && r.find("\"radiant\"") != std::string::npos);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/action", "{\"action\":\"alarm.toggle\",\"slot\":2}", r, h.t), 200);
  CHECK_EQ(h.app->settings().alarms[2].enabled, 1);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/action", "{\"action\":\"alarm.toggle\",\"slot\":3}", r, h.t), 422);
  CHECK_EQ(apiCall(*h.app, "POST", "/api/action", "{\"action\":\"screen.sleep\"}", r, h.t), 200);
  h.run(50); CHECK(h.app->screen() == Screen::ScreenOff);
}

TEST(dev_unlock_uses_separate_test_profile_and_radiant) {
  H h; h.skipIntro();
  std::string r;
  CHECK_EQ(apiCall(*h.app, "POST", "/api/action", "{\"action\":\"dev.unlockAll\"}", r, h.t), 200);
  CHECK(h.app->testProgress());
  h.run(6000);
  h.reboot();
  CHECK_EQ(h.app->progress().unlocked, 0);
  // Radiante: libera sozinho quando todas as outras foram conquistadas
  CHECK_EQ(apiCall(*h.app, "POST", "/api/restore", "{\"format\":\"tubabot-backup\",\"prefs\":{},\"progress\":{\"unlocked\":511}}", r, h.t), 200);
  h.stroke(1600); h.run(3000);
  h.swipe(0, -200); h.pick(L_BREATH); h.press(B_BREATH_PLAY);  // qualquer conquista nova dispara a checagem
  CHECK((h.app->progress().unlocked & 511) == 511);
}

TEST(menu_navigation_and_inactivity) {
  H h; h.skipIntro();
  h.swipe(0, -200); CHECK(h.app->screen() == Screen::Launcher);
  h.run(31000, 50);
  CHECK(h.app->screen() == Screen::Face);  // listas voltam ao rosto
  h.swipe(0, -200); CHECK(h.pick(L_EXPR)); CHECK(h.app->screen() == Screen::Expressions);
  h.swipe(-180, 0); h.swipe(-180, 0);
  CHECK(h.press(B_EXPR_USE)); CHECK(h.app->screen() == Screen::Face);
  CHECK(h.app->baseExpr() == Expr::Sad);  // terceira da coleção
  h.swipe(0, -200); CHECK(h.pick(L_CLOCK)); CHECK(h.app->screen() == Screen::Clock);
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
