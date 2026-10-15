// Gera imagens reais das telas do TubaBot (320×240, paisagem) a partir do núcleo
// (o mesmo código do firmware). Saída: um .ppm por tela em <pasta>; o Makefile
// monta docs/telas.png com tools/gallery.py.
#include "../tests/fake_platform.h"
#include "../core/app_ids.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <set>

using namespace tb;

static uint16_t fb[SCR_W * SCR_H];
static int g_langOpt = 0;                 // TB_LANG=en|es: gera as telas em outro idioma
static std::set<std::string> g_missing;   // TB_TR_AUDIT=1: textos desenhados sem tradução
static void onMiss(const char* t) { g_missing.insert(t); }
static std::string outDir = ".";
static int shot = 0;

static void save(App& a, const char* name) {
  char path[512];
  snprintf(path, sizeof path, "%s/%02d_%s.ppm", outDir.c_str(), shot++, name);
  FILE* f = fopen(path, "wb");
  if (!f) { perror(path); exit(1); }
  fprintf(f, "P6 %d %d 255\n", SCR_W, SCR_H);
  const uint16_t* px = a.canvas().pixels();
  for (int i = 0; i < SCR_W * SCR_H; i++) { Color c = from565(px[i]); fputc((int)(c >> 16), f); fputc((int)((c >> 8) & 255), f); fputc((int)(c & 255), f); }
  fclose(f);
}

struct Rig {
  FakePlatform plat;
  App* app;
  ms_t t = 0;
  explicit Rig(bool setup = true, Model model = Model::Pro, int body = 0, int theme = 0) {
    plat.setFactory(model, body, theme);
    FactoryConfig fc; parseFactory(plat.factory, plat.factoryLen, fc);
    Settings s; defaultSettings(s); applyFactoryDefaults(fc, s);
    s.setupDone = setup ? 1 : 0; tbStrlcpy(s.nickname, "Tubarãozinho", sizeof s.nickname);
    s.language = (uint8_t)g_langOpt;
    s.weatherEnabled = 1;
    uint8_t buf[BLOB_MAX]; size_t n = encodeBlob(BlobKind::Settings, SETTINGS_VERSION, &s, sizeof s, buf, sizeof buf);
    plat.storageSave(BlobKind::Settings, buf, n);
    app = new App(plat);
    app->attachFramebuffer(fb);
    HwCaps caps; caps.motor = false; caps.imu = false; caps.rtc = false; caps.battery = true; caps.mic = true;
    app->begin(caps, t, 1234, true);
    app->setTime(1792069500, t);  // 2026-10-14 10:25 UTC (07:25 em Brasília)
    PowerState p; p.known = true; p.battery = true; p.percent = 76; p.charging = false; p.usb = false; p.mv = 3940;
    app->powerUpdate(p, t);
    const char* body2 = "{\"current\":{\"temperature_2m\":24.6,\"relative_humidity_2m\":61,\"weather_code\":2,\"is_day\":1},"
                        "\"daily\":{\"weather_code\":[2,61,3],\"temperature_2m_max\":[28.1,24.0,26.4],\"temperature_2m_min\":[18.2,17.9,16.0]}}";
    app->weatherResult(body2, strlen(body2), 200, t);
  }
  ~Rig() { delete app; }
  void run(ms_t ms, ms_t step = 20) { ms_t end = t + ms; while (t < end) { t += step; app->tick(t); } }
  void tap(int x, int y) { app->touchSample(true, x, y, t); run(60); app->touchSample(false, x, y, t); run(400); }
  void button(int id) { int x, y; if (app->buttonPos(id, x, y)) tap(x, y); }
  void prefs(const char* json) {
    char out[2048]; StrBuf sb(out, sizeof out); ApiCtx ctx; ctx.token = app->sessionToken();
    app->api("POST", "/api/prefs", json, strlen(json), ctx, sb, t);
    run(100);
  }
  void go(Screen s, ms_t settle = 500) { app->gotoScreen(s, t); run(settle); }
};

int main(int argc, char** argv) {
  if (argc > 1) outDir = argv[1];
  if (const char* l = getenv("TB_LANG")) { Lang x = langFromKey(l); if (x != Lang::Count) g_langOpt = (int)x; }
  bool audit = getenv("TB_TR_AUDIT") != nullptr;
  if (audit) g_trMiss = onMiss;
  {
    Rig r(false);
    r.run(400); save(*r.app, "abertura_mar");
    r.run(900); save(*r.app, "abertura_nome");
    r.run(1100); save(*r.app, "abertura_olhos");
    r.run(1500); save(*r.app, "primeiro_uso_qr");
  }
  Rig r;  // TubaBot Pro vermelho, tema escuro
  r.run(3200);
  r.run(4200); save(*r.app, "rosto_vermelho_escuro");
  r.tap(150, 120); r.run(300); save(*r.app, "rosto_controles");
  r.run(4500);
  r.prefs("{\"showClock\":true,\"showWeather\":true,\"showBattery\":true}");
  r.run(600); save(*r.app, "rosto_com_informacoes");
  r.prefs("{\"showClock\":false,\"showWeather\":false,\"showBattery\":false}");
  r.app->face().setExpression(Expr::Love, r.t, true); r.run(500); save(*r.app, "rosto_amor");
  r.app->face().setExpression(Expr::Sleepy, r.t, true); r.run(700); save(*r.app, "rosto_sono");
  r.app->face().setExpression(Expr::Angry, r.t, true); r.run(500); save(*r.app, "rosto_bravo");
  r.app->face().setExpression(Expr::Celebrate, r.t, true); r.run(500); save(*r.app, "rosto_vitoria");
  r.app->face().setExpression(Expr::Happy, r.t, true); r.run(300);
  r.go(Screen::Launcher); save(*r.app, "menu");
  r.go(Screen::MoreApps); save(*r.app, "mais_apps");
  r.go(Screen::Appearance); save(*r.app, "aparencia");
  r.go(Screen::ColorPick); save(*r.app, "cor_das_expressoes");
  r.go(Screen::Overlays); save(*r.app, "informacoes_na_tela");
  r.go(Screen::Volume); save(*r.app, "volume");
  r.go(Screen::Brightness); save(*r.app, "brilho");
  r.go(Screen::Wifi); save(*r.app, "wifi");
  r.go(Screen::System); save(*r.app, "sistema");
  r.go(Screen::Quick); save(*r.app, "painel_rapido");
  r.go(Screen::Clock, 600); save(*r.app, "relogio_digital");
  r.prefs("{\"watchFace\":2}"); r.go(Screen::Clock, 600); save(*r.app, "relogio_tuba");
  r.prefs("{\"watchFace\":1}"); r.go(Screen::Clock, 600); save(*r.app, "relogio_analogico");
  r.prefs("{\"watchFace\":0}");
  r.app->pomodoro().start(r.t); r.app->timer(0).start(r.t); r.run(60000);
  r.go(Screen::Today, 600); save(*r.app, "hoje");
  r.go(Screen::Pomodoro); save(*r.app, "pomodoro");
  r.go(Screen::Timers); save(*r.app, "timer");
  r.app->stopwatch().start(r.t - 83456); r.go(Screen::Stopwatch); save(*r.app, "cronometro");
  r.go(Screen::Alarms); save(*r.app, "alarmes");
  r.button(25); r.run(600); save(*r.app, "alarme_editar");
  r.go(Screen::GamesMenu); save(*r.app, "jogos");
  r.app->startGame(GameId::Jump, r.t); r.run(400); save(*r.app, "jogo_pula_intro");
  r.tap(160, 120); r.run(2600); save(*r.app, "jogo_pula");
  r.app->startGame(GameId::Fish, r.t); r.tap(160, 120); r.run(900); save(*r.app, "jogo_bote");
  r.app->startGame(GameId::Timing, r.t); r.run(300); save(*r.app, "jogo_tempo_certo");
  r.app->startGame(GameId::Dive, r.t); r.tap(160, 120); r.run(4000); save(*r.app, "jogo_mergulho");
  r.app->startGame(GameId::Reflex, r.t); r.run(300); save(*r.app, "jogo_reflexo");
  r.go(Screen::Expressions); save(*r.app, "expressoes");
  r.go(Screen::Breath, 300); r.tap(160, 120); r.run(2500); save(*r.app, "respiracao");
  r.go(Screen::Dice, 300); r.tap(160, 120); r.run(1200); save(*r.app, "sorteio");
  r.go(Screen::Weather); save(*r.app, "clima");
  r.go(Screen::Stats); save(*r.app, "meu_tuba");
  r.app->openConfig(r.t); r.run(600); save(*r.app, "config_qr");
  r.go(Screen::Info); r.button(70); r.run(400); save(*r.app, "info_fabrica");
  r.go(Screen::System); { int x, y; if (r.app->listItemPos(S_FACTORY_RESET, x, y)) r.tap(x, y); } r.run(400); save(*r.app, "restaurar_fabrica");
  r.go(Screen::Face); r.run(25 * 60000, 200); save(*r.app, "alerta");
  {
    Rig l(true, Model::Pro, 1, 1);  // azul, tema claro
    l.run(7500); save(*l.app, "rosto_azul_claro");
    l.go(Screen::Launcher); save(*l.app, "menu_claro");
    l.app->pomodoro().start(l.t); l.go(Screen::Pomodoro); save(*l.app, "pomodoro_claro");
    l.go(Screen::ColorPick); save(*l.app, "cor_claro");
  }
  {
    Rig w(true, Model::Lite, 2, 0);  // branco, escuro, Lite
    w.run(7500); save(*w.app, "rosto_branco_escuro_lite");
    w.go(Screen::Overlays); save(*w.app, "lite_sem_bateria");
  }
  {
    Rig g(true, Model::Lite, 3, 1);  // grafite, claro
    g.run(7500); save(*g.app, "rosto_grafite_claro");
  }
  {
    // v3.1: reações lendárias, medalha, coleção, idioma e atualização
    Rig v;
    v.run(7500);
    auto act = [&](const char* json) {
      char out[2048]; StrBuf sb(out, sizeof out); ApiCtx ctx; ctx.token = v.app->sessionToken(); ctx.devTools = true;
      v.app->api("POST", "/api/action", json, strlen(json), ctx, sb, v.t);
    };
    act("{\"action\":\"legend.play\",\"scene\":\"cafe\",\"discover\":true}"); v.run(2000); save(*v.app, "lendaria_cafe");
    v.run(4700); save(*v.app, "lendaria_medalha");
    act("{\"action\":\"legend.play\",\"scene\":\"escada\",\"discover\":true}"); v.run(4600); save(*v.app, "lendaria_escada_tchau");
    v.run(5000);
    act("{\"action\":\"legend.play\",\"scene\":\"joinha\",\"discover\":true}"); v.run(900); save(*v.app, "lendaria_joinha");
    v.run(3000);
    act("{\"action\":\"legend.play\",\"scene\":\"ouvidos\",\"discover\":true}"); v.run(1200); save(*v.app, "lendaria_ouvidos");
    v.run(8000);   // espera a medalha sumir
    v.go(Screen::Legends); save(*v.app, "lendarias_colecao");
    v.go(Screen::Language); save(*v.app, "idioma");
    v.go(Screen::Update); save(*v.app, "atualizacoes");
  }
  if (audit) {
    for (const std::string& m : g_missing) printf("SEM TRADUÇÃO: \"%s\"\n", m.c_str());
    printf("%d textos sem tradução\n", (int)g_missing.size());
  }
  return 0;
}
