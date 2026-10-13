// Gera docs/telas.pbm com várias telas reais produzidas pelo núcleo (mesmo
// framebuffer do firmware). Uso: make screens
#include "../tests/fake_platform.h"
#include <cstdio>
#include <vector>
#include <string>
using namespace tb;

struct Shot { std::string name; Framebuffer fb; };
static std::vector<Shot> shots;

struct Sim {
  FakePlatform plat; App app{plat}; ms_t t = 0;
  Sim(Model m, bool setup = true) {
    if (setup) { Settings s; defaultSettings(s); s.setupDone = 1; tbStrlcpy(s.nickname, "Walace", sizeof s.nickname);
      uint8_t b[600]; size_t n = encodeBlob(BlobKind::Settings, SETTINGS_VERSION, &s, sizeof s, b, sizeof b); plat.storageSave(BlobKind::Settings, b, n); }
    app.begin(m, 0, 77, true);
  }
  void run(ms_t ms) { ms_t e = t + ms; while (t < e) { t += 10; if (app.tick(t)) app.framePresented(t); } }
  void lvl(bool v) { app.touchSample(v, t); }
  void tap() { lvl(true); run(80); lvl(false); run(420); }
  void hold(ms_t ms) { lvl(true); run(ms); lvl(false); run(100); }
  void shot(const char* n) { app.tick(t + 1); Shot s; s.name = n; s.fb = app.frame(); shots.push_back(s); }
  void api(const char* path, const char* body) { char b[8192]; StrBuf sb(b, sizeof b); ApiCtx c; c.token = app.sessionToken(); c.devTools = true; app.api("POST", path, body, tbStrlen(body), c, sb, t); }
};

int main(int argc, char** argv) {
  const char* outPath = argc > 1 ? argv[1] : "docs/telas.pbm";
  { Sim s(Model::Full); s.run(500); s.shot("abertura 1"); s.run(800); s.shot("abertura 2"); s.run(900); s.shot("abertura 3"); s.run(500); s.shot("saudacao"); }
  { Sim s(Model::Full); s.run(3000); s.shot("rosto"); s.lvl(true); s.run(1300); s.shot("segurando: carinho"); s.run(2000); s.shot("segurando: amor"); s.lvl(false); s.run(300); s.shot("amor");
    s.run(4000); s.lvl(true); s.run(6000); s.shot("segurando 6s: config"); s.lvl(false); s.run(200); s.shot("confirmar config"); s.run(7000);
    s.lvl(true); s.run(70); s.lvl(false); s.run(80); s.lvl(true); s.run(70); s.lvl(false); s.run(400); s.shot("menu");
    s.tap(); s.tap(); s.hold(1200); s.tap(); s.run(65000); s.shot("pomodoro"); }
  { Sim s(Model::Full); s.run(3000); s.api("/api/action", "{\"action\":\"timer.start\",\"slot\":0,\"seconds\":3}"); s.run(3100); s.shot("alerta timer"); }
  { Sim s(Model::Lite, false); s.run(3000); s.shot("primeiro uso"); s.tap(); s.shot("primeiro uso 2"); }
  { Sim s(Model::Full); s.run(3000); s.app.startGame(GameId::Jump, s.t); s.shot("pula intro"); s.tap(); s.run(2400); s.shot("pula jogando"); s.run(4000); s.shot("pula fim"); }
  { Sim s(Model::Full); s.run(3000); s.app.startGame(GameId::Fish, s.t); s.tap(); s.run(1300); s.shot("hora do bote"); }
  { Sim s(Model::Full); s.run(3000); s.app.startGame(GameId::Reflex, s.t); s.tap(); s.run(200); s.shot("reflexo espere"); s.run(4500); s.shot("reflexo agora"); s.tap(); s.shot("reflexo resultado"); }
  { Sim s(Model::Full); s.run(3000); s.app.startGame(GameId::Timing, s.t); s.shot("tempo certo"); s.tap(); s.run(1200); s.shot("contagem"); s.run(3000); s.shot("cronometrando"); s.tap(); s.shot("resultado"); }
  { Sim s(Model::Full); s.run(3000); s.api("/api/time", "{\"epoch\":1791936000}"); s.app.gotoScreen(Screen::Clock, s.t); s.run(100); s.shot("relogio"); s.app.gotoScreen(Screen::GamesMenu, s.t); s.run(100); s.shot("menu jogos"); }

  int cols = 4, rows = ((int)shots.size() + cols - 1) / cols;
  int W = cols * 132, Hh = rows * 68;
  FILE* f = fopen(outPath, "w");
  if (!f) return 1;
  fprintf(f, "P1\n%d %d\n", W, Hh);
  for (int y = 0; y < Hh; y++) {
    for (int x = 0; x < W; x++) {
      int c = x / 132, r = y / 68, i = r * cols + c, px = x % 132 - 2, py = y % 68 - 2;
      int v = 0;  // PBM: 1 = preto; aqui 1 = pixel aceso (o script inverte)
      if (i < (int)shots.size() && px >= 0 && px < 128 && py >= 0 && py < 64) v = shots[i].fb.get(px, py);
      fputc(v ? '1' : '0', f);
    }
    fputc('\n', f);
  }
  fclose(f);
  for (size_t i = 0; i < shots.size(); i++) printf("%2zu %s\n", i, shots[i].name.c_str());
  return 0;
}
