// Sensores novos do TubaBot v2, processados no núcleo (testáveis no PC):
// - Movimento (QMI8658): sacudir, pegar no colo, virar de cara para baixo,
//   deitar, de cabeça para baixo e inclinação (os olhos seguem a gravidade).
// - Som (2 microfones, ES7210): palma, barulho alto, música (dança no ritmo).
//   Só o NÍVEL sonoro chega ao núcleo: nenhum áudio é gravado ou transmitido.
// - Energia (AXP2101): bateria, carregando, USB.
#pragma once
#include "tb_base.h"

namespace tb {

// ── Movimento ───────────────────────────────────────────────────────────────
// Entrada: vetor gravidade em mg no referencial da TELA
//   x → direita, y → baixo, z → para fora da tela (em direção a quem olha).
// Em pé sobre a mesa: (0, +1000, 0). Tela para cima: z = −1000. Tela para baixo: z = +1000.
enum class MotionEv : uint8_t { None, Shake, PickUp, FaceDown, FaceUp, OnBack, UpsideDown, Upright };

enum class Pose : uint8_t { Upright, FaceDown, OnBack, UpsideDown, Tilted };

class MotionSensor {
 public:
  void reset();
  void sample(int ax, int ay, int az, ms_t t);
  int poll(MotionEv* out, int maxOut);
  // inclinação suavizada, −1000..1000 (x: tombado p/ direita +, y: inclinado p/ trás +)
  int tiltX() const { return tiltX_; }
  int tiltY() const { return tiltY_; }
  Pose pose() const { return pose_; }
  bool atRest() const { return restSince_ != 0; }
  int lpX() const { return (int)lx_; }
  int lpY() const { return (int)ly_; }
  int lpZ() const { return (int)lz_; }
  int jerk() const { return jerk_; }
  uint32_t samples() const { return samples_; }
  bool active() const { return samples_ > 0; }

 private:
  void push(MotionEv e) { if (qn_ < 8) q_[qn_++] = e; }
  MotionEv q_[8];
  int qn_ = 0;
  float lx_ = 0, ly_ = 1000, lz_ = 0;  // passa-baixa (gravidade)
  bool init_ = false;
  ms_t lastT_ = 0;
  int jerk_ = 0;
  // sacudir
  ms_t peaks_[6] = {0};
  int peakN_ = 0;
  ms_t lastPeak_ = 0, shakeCooldown_ = 0;
  // repouso / pegar
  ms_t restSince_ = 0, calmSince_ = 0, moveSince_ = 0, pickCooldown_ = 0;
  // pose
  Pose pose_ = Pose::Upright, candPose_ = Pose::Upright;
  ms_t candSince_ = 0;
  int tiltX_ = 0, tiltY_ = 0;
  uint32_t samples_ = 0;
};

// ── Som ─────────────────────────────────────────────────────────────────────
// Entrada: nível 0..100 (≈ dBFS + 100) a cada ~40 ms.
enum class SoundEv : uint8_t { None, Clap, Loud, MusicStart, MusicStop };

class SoundSensor {
 public:
  void reset();
  void sample(int level, ms_t t);
  int poll(SoundEv* out, int maxOut);
  int level() const { return level_; }
  int baseline() const { return (int)base_; }
  bool music() const { return music_; }
  // 0..999 dentro da batida estimada (para a dança)
  int beatPhase(ms_t now) const;
  uint16_t beatMs() const { return beatMs_; }
  bool active() const { return samples_ > 0; }
  uint32_t samples() const { return samples_; }

 private:
  void push(SoundEv e) { if (qn_ < 8) q_[qn_++] = e; }
  SoundEv q_[8];
  int qn_ = 0;
  int level_ = 0, prev_ = 0;
  float base_ = 30.f;
  bool init_ = false;
  ms_t lastT_ = 0;
  // palma
  bool clapArmed_ = false;
  ms_t clapAt_ = 0, clapCooldown_ = 0;
  // barulho
  ms_t loudSince_ = 0, loudCooldown_ = 0;
  // música
  ms_t onsets_[16] = {0};
  int onsetN_ = 0;
  ms_t lastOnset_ = 0, musicCheck_ = 0;
  bool music_ = false;
  uint16_t beatMs_ = 500;
  uint32_t samples_ = 0;
  int energyAcc_ = 0, energyN_ = 0;
};

// ── Energia ─────────────────────────────────────────────────────────────────
struct PowerState {
  bool known = false;       // há leitura do AXP2101 (ou do simulador)
  bool battery = false;     // bateria presente
  int8_t percent = -1;      // 0..100, −1 desconhecido
  bool charging = false;
  bool usb = true;          // alimentado pelo USB
  uint16_t mv = 0;
};

}  // namespace tb
