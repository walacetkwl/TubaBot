// Kit de interface do TubaBot v2 para tela redonda: tema, botões, títulos,
// dicas e a lista rolável com efeito "lupa" (o item do centro é o maior).
#pragma once
#include "gfx.h"
#include "icons.h"

namespace tb {
namespace ui {

// ── tema (fundo preto = AMOLED apagado) ──
static const Color BG = 0x000000;
static const Color SURF = 0x141924;
static const Color SURF2 = 0x1E2533;
static const Color LINE = 0x2A3242;
static const Color TEXT = 0xF2F5FA;
static const Color MUTED = 0x8D97AB;
static const Color DIM = 0x566074;
static const Color OK = 0x4BF0A6;
static const Color WARN = 0xFFB547;
static const Color BAD = 0xFF5E57;
// cor de cada app
static const Color C_FACE = 0x3EE6FF;
static const Color C_CLOCK = 0x4C8DFF;
static const Color C_POMO = 0xFF5E57;
static const Color C_TIMER = 0xFFB547;
static const Color C_SW = 0x4BF0A6;
static const Color C_ALARM = 0xFF8A3D;
static const Color C_GAMES = 0xB592FF;
static const Color C_EXPR = 0xFF7AB6;
static const Color C_BREATH = 0x3EE6D0;
static const Color C_LEVEL = 0xA8E05F;
static const Color C_DICE = 0xFFD54A;
static const Color C_WEATHER = 0x5CC8FF;
static const Color C_STATS = 0xFF7AB6;
static const Color C_SET = 0x9AA5B8;
static const Color C_INFO = 0x6FA8FF;

void pill(Canvas& c, float cx, float cy, float w, float h, Color bg, const char* label, const Font& f, Color fg, int a = 255);
void roundButton(Canvas& c, float cx, float cy, float r, Color bg, Icon ic, Color fg, bool pressed = false, int a = 255);
void title(Canvas& c, float y, const char* text, Color col);
void hint(Canvas& c, const char* text, float y = 428.f);
void pageDots(Canvas& c, int n, int sel, float y = 446.f, Color col = TEXT);
void appHeader(Canvas& c, Icon ic, const char* name, Color col);  // ícone + nome no topo
void progressRing(Canvas& c, float r, float th, int permille, Color col, Color track = SURF2);
void slider(Canvas& c, float x, float y, float w, int permille, Color col, Icon ic);
void toggle(Canvas& c, float cx, float cy, bool on, Color col);

// ── lista rolável ──
struct ListItem {
  const char* label;
  const char* sub;   // pode ser nullptr
  Icon icon;
  Color color;
  int id;
};

class ListView {
 public:
  static const int kItemH = 96;
  void reset(int n, int sel = 0);
  int count() const { return n_; }
  int centered() const;
  void onDown(int y, ms_t t);
  void onMove(int y, ms_t t);
  void onUp(int vy);
  void tick(ms_t now);
  bool moving() const { return dragging_ || vel_ != 0.f || snapping_; }
  bool dragged() const { return dragDist_ > 18; }
  int hit(int x, int y) const;   // índice do item sob o ponto, −1 se nenhum
  void scrollTo(int i) { scroll_ = (float)tbClamp(i, 0, tbMax(0, n_ - 1)); vel_ = 0; snapping_ = false; }
  void draw(Canvas& c, const ListItem* items, int n, ms_t now, int pressedIdx = -1) const;
  float scroll() const { return scroll_; }

 private:
  int n_ = 0;
  float scroll_ = 0.f, vel_ = 0.f;
  bool dragging_ = false, snapping_ = false;
  float dragScroll0_ = 0.f;
  int dragY0_ = 0, dragDist_ = 0;
  ms_t lastTick_ = 0;
};

}  // namespace ui
}  // namespace tb
