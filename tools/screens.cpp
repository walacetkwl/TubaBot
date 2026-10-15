// Gera imagens reais das telas do TubaBot v2 a partir do núcleo (o mesmo
// código do firmware). Saída: um .ppm por tela em <pasta>; o Makefile monta
// docs/telas.png com tools/gallery.py.
#include "../tests/fake_platform.h"
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace tb;

static uint16_t fb[SCR_W * SCR_H];
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
  explicit Rig(bool setup = true) {
    Settings s; defaultSettings(s); s.setupDone = setup ? 1 : 0; tbStrlcpy(s.nickname, "Tubarãozinho", sizeof s.nickname);
    s.weatherEnabled = 1;
    uint8_t buf[BLOB_MAX]; size_t n = encodeBlob(BlobKind::Settings, SETTINGS_VERSION, &s, sizeof s, buf, sizeof buf);
    plat.storageSave(BlobKind::Settings, buf, n);
    app = new App(plat);
    app->attachFramebuffer(fb);
    HwCaps caps; caps.motor = true;
    app->begin(caps, t, 1234, true);
    app->setTime(1792069500, t);  // 2026-10-14 10:25 (UTC-3: 07:25)
    PowerState p; p.known = true; p.battery = true; p.percent = 76; p.charging = false; p.usb = false; p.mv = 3940;
    app->powerUpdate(p, t);
    app->motionSample(0, 1000, 0, t);
  }
  ~Rig() { delete app; }
  void run(ms_t ms, ms_t step = 20) { ms_t end = t + ms; while (t < end) { t += step; app->motionSample(0, 1000, 0, t); app->tick(t); } }
  void tap(int x, int y) { app->touchSample(true, x, y, t); run(60); app->touchSample(false, x, y, t); run(400); }
  void button(int id) { int x, y; if (app->buttonPos(id, x, y)) tap(x, y); }
};

int main(int argc, char** argv) {
  if (argc > 1) outDir = argv[1];
  {
    Rig r(false);
    r.run(400); save(*r.app, "abertura_mar");
    r.run(900); save(*r.app, "abertura_nome");
    r.run(1100); save(*r.app, "abertura_olhos");
    r.run(1500); save(*r.app, "primeiro_uso_qr");
  }
  Rig r;
  r.run(3200);
  r.run(2600); save(*r.app, "rosto");
  r.app->face().setExpression(Expr::Love, r.t, true); r.run(500); save(*r.app, "rosto_amor");
  r.app->gotoScreen(Screen::Clock, r.t); r.run(600); save(*r.app, "relogio_digital");
  r.app->gotoScreen(Screen::Today, r.t); r.run(600);
  r.app->pomodoro().start(r.t); r.app->timer(0).start(r.t); r.run(60000);
  {
    const char* body = "{\"current\":{\"temperature_2m\":24.6,\"relative_humidity_2m\":61,\"weather_code\":2,\"is_day\":1},"
                       "\"daily\":{\"weather_code\":[2,61,3],\"temperature_2m_max\":[28.1,24.0,26.4],\"temperature_2m_min\":[18.2,17.9,16.0]}}";
    r.app->weatherResult(body, strlen(body), 200, r.t);
  }
  r.app->gotoScreen(Screen::Today, r.t); r.run(600); save(*r.app, "hoje");
  r.app->gotoScreen(Screen::Launcher, r.t); r.run(800); save(*r.app, "apps");
  r.app->gotoScreen(Screen::Quick, r.t); r.run(600); save(*r.app, "ajustes_rapidos");
  r.app->gotoScreen(Screen::Pomodoro, r.t); r.run(600); save(*r.app, "pomodoro");
  r.app->gotoScreen(Screen::Timers, r.t); r.run(600); save(*r.app, "timers");
  r.app->gotoScreen(Screen::Stopwatch, r.t); r.app->stopwatch().start(r.t - 83456); r.run(600); save(*r.app, "cronometro");
  r.app->gotoScreen(Screen::Alarms, r.t); r.run(600); save(*r.app, "alarmes");
  r.button(25); r.run(600); save(*r.app, "alarme_editar");
  r.app->gotoScreen(Screen::GamesMenu, r.t); r.run(600); save(*r.app, "jogos");
  r.app->startGame(GameId::Jump, r.t); r.run(400); save(*r.app, "jogo_pula_intro");
  r.tap(233, 233); r.run(2600); save(*r.app, "jogo_pula");
  r.app->startGame(GameId::Fish, r.t); r.tap(233, 233); r.run(900); save(*r.app, "jogo_bote");
  r.app->startGame(GameId::Timing, r.t); r.run(300); save(*r.app, "jogo_tempo_certo");
  r.app->startGame(GameId::Dive, r.t); r.tap(233, 233); r.run(4000); save(*r.app, "jogo_mergulho");
  r.app->gotoScreen(Screen::Expressions, r.t); r.run(600); save(*r.app, "expressoes");
  r.app->gotoScreen(Screen::Breath, r.t); r.run(300); r.tap(233, 233); r.run(2500); save(*r.app, "respiracao");
  r.app->gotoScreen(Screen::Level, r.t); r.run(600); save(*r.app, "nivel");
  r.app->gotoScreen(Screen::Dice, r.t); r.run(300); r.tap(233, 233); r.run(1200); save(*r.app, "sorteio");
  r.app->gotoScreen(Screen::Weather, r.t); r.run(600); save(*r.app, "clima");
  r.app->gotoScreen(Screen::Stats, r.t); r.run(600); save(*r.app, "meu_tuba");
  r.app->gotoScreen(Screen::Settings, r.t); r.run(600); save(*r.app, "ajustes");
  r.app->openConfig(r.t); r.run(600); save(*r.app, "config_qr");
  r.app->gotoScreen(Screen::Info, r.t); r.run(600); save(*r.app, "informacoes");
  r.app->gotoScreen(Screen::Face, r.t); r.run(25 * 60000, 200); save(*r.app, "alerta");
  return 0;
}
