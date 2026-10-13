// Rosto do TubaBot: expressões paramétricas, transições suaves, piscar e
// olhar natural. Branco sobre preto, 128×64.
#pragma once
#include "framebuffer.h"

namespace tb {

enum class Expr : uint8_t {
  Happy = 0, Neutral, Sad, Surprised, Love, Sleepy, Angry, Dead, Blink, WinkL, WinkR, Pet,
  Focus, Rest, Hit, NearMiss, Celebrate, Waiting,
  // desbloqueáveis (mesma ordem de UnlockId)
  Shy, Focused, Victorious, Satisfied, Amazed, Precision,
  Count
};
const char* exprName(Expr e);        // nome em pt-BR para portal/telas
const char* exprKey(Expr e);         // chave estável para API
Expr exprFromKey(const char* k);
bool exprIsUnlockable(Expr e, int* unlockId);
Expr exprForUnlock(int unlockId);

enum class EyeKind : uint8_t { Normal, Heart, X, ArcUp, Line, Star, Ring, Crosshair };
enum class Mouth : uint8_t { None, Smile, BigSmile, Frown, O, Flat, Cat, Small, Wobble };
enum FaceExtra : uint8_t { FX_NONE = 0, FX_BLUSH = 1, FX_ZZZ = 2, FX_SPARKLE = 4, FX_HEARTS = 8, FX_SWEAT = 16, FX_DOTS = 32 };

struct EyeShape {
  int8_t w, h;        // tamanho
  int8_t lidTop;      // pálpebra superior (px cobertos)
  int8_t lidBottom;   // bochecha (px cobertos por arco inferior)
  int8_t slope;       // inclinação da pálpebra: + bravo (canto interno baixo), − triste
  int8_t dx, dy;      // deslocamento do olhar
  EyeKind kind;
};
struct FaceDef { EyeShape L, R; Mouth mouth; uint8_t extras; };
const FaceDef& faceDef(Expr e);

struct PersonalityTuning {
  uint16_t blinkMinMs, blinkMaxMs;
  uint16_t lookMinMs, lookMaxMs;
  uint8_t lookRangeX, lookRangeY;
  uint8_t doubleBlinkPct;   // chance de piscada dupla
  uint8_t mischiefPct;      // chance de microreação (piscadinha/surpresa)
};

class FaceRenderer {
 public:
  static const int kLeftX = 40, kRightX = 88, kEyeY = 27;

  void reset(ms_t now);
  void setExpression(Expr e, ms_t now, bool instant = false);
  Expr expression() const { return target_; }
  // Animações ociosas; 'animLevel' 0..2 escala a frequência.
  void update(ms_t now, Rng& rng, const PersonalityTuning& t, uint8_t animLevel);
  void draw(Framebuffer& fb, ms_t now);
  // Estado de "carinho" enquanto o sensor está pressionado (olhos relaxados).
  void setPetting(bool on) { petting_ = on; }
  bool animating(ms_t now) const;

  static void drawFace(Framebuffer& fb, const FaceDef& f, ms_t now, int lookX = 0, int lookY = 0, int blinkPermille = 0);

 private:
  Expr from_ = Expr::Happy, target_ = Expr::Happy;
  ms_t transStart_ = 0;
  uint16_t transMs_ = 0;
  ms_t nextBlink_ = 0, blinkStart_ = 0;
  bool blinking_ = false, secondBlink_ = false;
  ms_t nextLook_ = 0;
  int lookX_ = 0, lookY_ = 0, lookTX_ = 0, lookTY_ = 0;
  ms_t lookMoveStart_ = 0;
  int lookFromX_ = 0, lookFromY_ = 0;
  bool petting_ = false;
  ms_t microUntil_ = 0;
  Expr micro_ = Expr::Happy;
  bool microActive_ = false;
};

}  // namespace tb
