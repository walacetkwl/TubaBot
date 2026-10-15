// Rosto do TubaBot: olhos luminosos coloridos ocupando a tela inteira.
// Expressões paramétricas (todas as medidas num "espaço de desenho" 466×466,
// centro 233,233, ampliado/reduzido para a tela por FaceXf), transições suaves,
// piscar, olhar natural, respiração, olhar que segue o dedo, dança no ritmo.
// Cores vêm de FaceStyle (cor das expressões + tema claro/escuro).
#pragma once
#include "gfx.h"

namespace tb {

enum class Expr : uint8_t {
  Happy = 0, Neutral, Sad, Surprised, Love, Sleepy, Angry, Dead, Blink, WinkL, WinkR, Pet,
  Focus, Rest, Hit, NearMiss, Celebrate, Waiting,
  // desbloqueáveis (mesma ordem de UnlockId)
  Shy, Focused, Victorious, Satisfied, Amazed, Precision,
  Groovy, Zen, Diver, Radiant,
  // reações da v2 (não aparecem na coleção)
  Dizzy, Startled, Squint, Charging, Tired, Curious,
  Count
};
const char* exprName(Expr e);        // nome em pt-BR para portal/telas
const char* exprKey(Expr e);         // chave estável para API/backup
Expr exprFromKey(const char* k);
bool exprIsUnlockable(Expr e, int* unlockId);
Expr exprForUnlock(int unlockId);
bool exprIsReaction(Expr e);

enum class EyeKind : uint8_t { Normal, Heart, X, ArcUp, ArcDown, Line, Star, Ring, Crosshair, Spiral, Squint, Goggle };
enum class Mouth : uint8_t { None, Smile, BigSmile, Frown, O, Flat, Cat, Small, Wobble, Open };
enum FaceExtra : uint16_t {
  FX_NONE = 0, FX_BLUSH = 1, FX_ZZZ = 2, FX_SPARKLE = 4, FX_HEARTS = 8, FX_SWEAT = 16, FX_DOTS = 32,
  FX_NOTES = 64, FX_STARS = 128, FX_TEAR = 256, FX_ANGER = 512, FX_BOLT = 1024, FX_BUBBLES = 2048, FX_AURA = 4096, FX_RAINBOW = 8192,
};

struct EyeShape {
  int16_t w, h;        // tamanho (px)
  int16_t lidTop;      // pálpebra superior: px cobertos
  int16_t lidBottom;   // bochecha (arco inferior): px cobertos
  int16_t slope;       // inclinação da pálpebra: + bravo (canto interno baixo), − triste
  int16_t dx, dy;      // deslocamento
  EyeKind kind;
};
struct FaceDef { EyeShape L, R; Mouth mouth; uint16_t extras; };
const FaceDef& faceDef(Expr e);

struct FaceStyle {
  Color eye = 0x3EE6FF;     // cor das expressões (já adaptada ao tema)
  bool glow = true;         // halo luminoso
  bool mouth = true;        // mostrar boca
  bool light = false;       // tema claro (fundo branco)
  Color fx = 0xFFFFFF;      // brilhos/estrelinhas (branco no escuro; tom da cor no claro)
  Color zzz = 0xA9B4FF;     // "zzz" do sono
};

struct PersonalityTuning {
  uint16_t blinkMinMs, blinkMaxMs;
  uint16_t lookMinMs, lookMaxMs;
  uint8_t lookRangeX, lookRangeY;   // px
  uint8_t doubleBlinkPct;
  uint8_t mischiefPct;
};

// Transformação do espaço de desenho (centro 233,233) para a tela.
static const float FACE_DESIGN_C = 233.f;
static const float FACE_SCALE = 0.78f;   // rosto em tela cheia (320×240, paisagem)
static const float FACE_CY = 122.f;
struct FaceXf { float cx = (float)SCR_CX, cy = FACE_CY, s = FACE_SCALE; };

class FaceRenderer {
 public:
  static const int kLeftX = 141, kRightX = 325, kEyeY = 205, kMouthY = 318;

  void reset(ms_t now);
  void setExpression(Expr e, ms_t now, bool instant = false);
  Expr expression() const { return target_; }
  Expr shown() const { return microActive_ ? micro_ : target_; }
  void update(ms_t now, Rng& rng, const PersonalityTuning& t, uint8_t animLevel);
  void draw(Canvas& c, ms_t now, const FaceStyle& st);
  void setPetting(bool on) { petting_ = on; }
  // Olhar dirigido (dedo, inclinação): −1000..1000 nos dois eixos.
  void setLookTarget(bool active, int nx, int ny) { lookExt_ = active; extX_ = nx; extY_ = ny; }
  void bounce(ms_t now) { bounceAt_ = now; }        // "boop": achata e volta
  void setDance(bool on, int phase1000) { dance_ = on; dancePhase_ = phase1000; }
  void setDrift(int dx, int dy) { driftX_ = dx; driftY_ = dy; }  // pequenas variações de posição
  void setLayout(float cy, float s) { layoutCy_ = cy; layoutS_ = s; }
  bool animating(ms_t now) const;

  static void drawFace(Canvas& c, const FaceDef& f, const FaceStyle& st, ms_t now, const FaceXf& xf,
                       float lookX = 0, float lookY = 0, int blink = 0, float breathe = 0.f);

 private:
  Expr from_ = Expr::Happy, target_ = Expr::Happy;
  ms_t transStart_ = 0;
  uint16_t transMs_ = 0;
  ms_t nextBlink_ = 0, blinkStart_ = 0;
  bool blinking_ = false, secondBlink_ = false;
  ms_t nextLook_ = 0;
  float lookX_ = 0, lookY_ = 0;
  int lookTX_ = 0, lookTY_ = 0;
  bool lookExt_ = false;
  int extX_ = 0, extY_ = 0;
  bool petting_ = false;
  ms_t microUntil_ = 0;
  Expr micro_ = Expr::Happy;
  bool microActive_ = false;
  ms_t bounceAt_ = 0;
  bool dance_ = false;
  int dancePhase_ = 0;
  int driftX_ = 0, driftY_ = 0;
  float layoutCy_ = FACE_CY, layoutS_ = FACE_SCALE;
  ms_t lastUpdate_ = 0;
};

}  // namespace tb
