// Kit de interface do TubaBot para a tela 320×240 em PAISAGEM: tema claro/escuro,
// cabeçalho com voltar, botões, alternadores, controle deslizante, lista em
// linhas (rolável, com inércia) e grade de blocos 3×3 (menu principal).
#pragma once
#include "gfx.h"
#include "icons.h"

namespace tb {
namespace ui {

// ── tema ──
// Cores neutras da interface. O app chama setTheme() antes de cada quadro;
// as referências abaixo (BG, TEXT...) apontam para o tema em uso.
struct Theme {
  bool light = false;
  Color bg, surf, surf2, line, text, muted, dim, scrim, toastBg;
  Color accent;    // cor das expressões adaptada ao tema
  Color onAccent;  // texto/ícone sobre a cor de destaque
};
extern Theme g_theme;
void setTheme(bool light, Color accent);
// Cor de app adaptada ao tema (no claro fica mais escura para manter contraste).
Color acc(Color c);

static const Color& BG = g_theme.bg;
static const Color& SURF = g_theme.surf;
static const Color& SURF2 = g_theme.surf2;
static const Color& LINE = g_theme.line;
static const Color& TEXT = g_theme.text;
static const Color& MUTED = g_theme.muted;
static const Color& DIM = g_theme.dim;
static const Color& ACCENT = g_theme.accent;
static const Color& ON_ACCENT = g_theme.onAccent;
static const Color INK = 0x0C0F14;   // ícone escuro sobre círculo colorido
static const Color OK = 0x34D399;
static const Color WARN = 0xF59E0B;
static const Color BAD = 0xEF4444;
// cor de cada app (passe por acc() ao usar como texto/traço)
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
static const Color C_LOOK = 0xFF9BD2;
static const Color C_VOL = 0x7BD88F;
static const Color C_BRIGHT = 0xFFD54A;
static const Color C_WIFI = 0x5CC8FF;

// ── medidas ──
static const int HEADER_H = 38;        // cabeçalho das telas de app
static const float FOOT_Y = 206.f;     // linha dos botões de ação
static const float SLIDER_X0 = 40.f, SLIDER_W = 240.f;  // controles deslizantes (Volume, Brilho, painel)

void pill(Canvas& c, float cx, float cy, float w, float h, Color bg, const char* label, const Font& f, Color fg, int a = 255);
void roundButton(Canvas& c, float cx, float cy, float r, Color bg, Icon ic, Color fg, bool pressed = false, int a = 255);
void header(Canvas& c, const char* title, Color col);   // título no topo (o app desenha o "voltar")
void hint(Canvas& c, const char* text, float y = 232.f);
void pageDots(Canvas& c, int n, int sel, float y = 230.f, Color col = 0);
void progressRing(Canvas& c, float cx, float cy, float r, float th, int permille, Color col, Color track = 0);
void slider(Canvas& c, float x, float y, float w, float h, int permille, Color col, Icon ic);
void toggle(Canvas& c, float cx, float cy, bool on, Color col);

// ── lista em linhas ──
enum class RowKind : uint8_t { Link, Value, Toggle, Action };
struct ListItem {
  const char* label;
  const char* sub;   // valor à direita (Value) ou linha secundária (Link/Action); pode ser nullptr
  Icon icon;
  Color color;
  int id;
  RowKind kind;
  bool on;           // estado do alternador (Toggle)
};

class ListView {
 public:
  static const int kRowH = 46;
  static const int kTop = HEADER_H + 2;
  void reset(int n, int keepScrollPx = 0);
  int count() const { return n_; }
  int scrollPx() const { return iround(scroll_); }
  void onDown(int y, ms_t t);
  void onMove(int y, ms_t t);
  void onUp(int vy);
  void tick(ms_t now);
  bool moving() const { return dragging_ || vel_ != 0.f || snapping_; }
  bool dragged() const { return dragDist_ > 10; }
  int hit(int x, int y) const;   // índice do item sob o ponto, −1 se nenhum
  void scrollTo(int i);          // garante o item visível
  void rowCenter(int i, int& x, int& y) const;
  void draw(Canvas& c, const ListItem* items, int n, int pressedIdx = -1) const;

 private:
  float maxScroll() const;
  int n_ = 0;
  float scroll_ = 0.f, vel_ = 0.f;
  bool dragging_ = false, snapping_ = false;
  float dragScroll0_ = 0.f;
  int dragY0_ = 0, dragDist_ = 0;
  ms_t lastTick_ = 0;
};

// ── grade 3×3 (menu principal e "mais apps") ──
struct GridGeom {
  static const int kCols = 3;
  static const int kTileW = 100, kTileH = 60, kGapX = 6, kGapY = 6;
  static const int kX0 = (SCR_W - (kCols * kTileW + (kCols - 1) * kGapX)) / 2;
  static const int kY0 = HEADER_H + 2;
  static float cx(int i) { return (float)(kX0 + (i % kCols) * (kTileW + kGapX) + kTileW / 2); }
  static float cy(int i) { return (float)(kY0 + (i / kCols) * (kTileH + kGapY) + kTileH / 2); }
};
void drawGrid(Canvas& c, const ListItem* items, int n, int pressedIdx = -1);
int gridHit(int n, int x, int y);

}  // namespace ui
}  // namespace tb
