#include "sensors.h"
#include "fmath.h"

namespace tb {

// ════════ Movimento ════════
void MotionSensor::reset() { *this = MotionSensor(); }

int MotionSensor::poll(MotionEv* out, int maxOut) {
  int n = 0;
  for (int i = 0; i < qn_ && n < maxOut; i++) out[n++] = q_[i];
  qn_ = 0;
  return n;
}

void MotionSensor::sample(int ax, int ay, int az, ms_t t) {
  samples_++;
  if (!init_) { lx_ = (float)ax; ly_ = (float)ay; lz_ = (float)az; init_ = true; lastT_ = t; restSince_ = t; return; }
  ms_t dt = t > lastT_ ? t - lastT_ : 1;
  if (dt > 500) dt = 500;
  lastT_ = t;
  // passa-baixa de ~0,25 s para a gravidade
  float k = fclamp((float)dt / 250.f, 0.02f, 1.f);
  lx_ += ((float)ax - lx_) * k; ly_ += ((float)ay - ly_) * k; lz_ += ((float)az - lz_) * k;
  float hx = (float)ax - lx_, hy = (float)ay - ly_, hz = (float)az - lz_;
  jerk_ = iround(fsqrt(hx * hx + hy * hy + hz * hz));

  // sacudir: 3 picos fortes em 1 s
  if (jerk_ > 650 && t - lastPeak_ >= 70) {
    lastPeak_ = t;
    if (peakN_ == 6) { for (int i = 1; i < 6; i++) peaks_[i - 1] = peaks_[i]; peakN_ = 5; }
    peaks_[peakN_++] = t;
    int recent = 0;
    for (int i = 0; i < peakN_; i++) if (t - peaks_[i] <= 1000) recent++;
    if (recent >= 3 && t >= shakeCooldown_) { push(MotionEv::Shake); shakeCooldown_ = t + 1500; peakN_ = 0; pickCooldown_ = t + 2000; }
  }

  // repouso -> pegar no colo
  if (jerk_ < 60) {
    if (!calmSince_) calmSince_ = t;
    if (!restSince_ && t - calmSince_ >= 1500) restSince_ = t;
    moveSince_ = 0;
  } else {
    calmSince_ = 0;
    if (jerk_ > 220) {
      if (!moveSince_) moveSince_ = t;
      if (restSince_ && t - restSince_ >= 2000 && t - moveSince_ >= 60 && t >= pickCooldown_) {
        push(MotionEv::PickUp); pickCooldown_ = t + 5000;
      }
      restSince_ = 0;
    }
  }

  // pose (com histerese de tempo)
  float g = fsqrt(lx_ * lx_ + ly_ * ly_ + lz_ * lz_);
  if (g < 300.f) g = 1000.f;
  float nx = lx_ / g, ny = ly_ / g, nz = lz_ / g;
  Pose p = Pose::Tilted;
  if (nz > 0.8f) p = Pose::FaceDown;
  else if (nz < -0.8f) p = Pose::OnBack;
  else if (ny < -0.7f) p = Pose::UpsideDown;
  else if (ny > 0.6f) p = Pose::Upright;
  if (p != candPose_) { candPose_ = p; candSince_ = t; }
  ms_t need = p == Pose::FaceDown ? 700 : p == Pose::OnBack ? 1500 : p == Pose::UpsideDown ? 600 : 400;
  if (p != pose_ && t - candSince_ >= need && p != Pose::Tilted) {
    Pose old = pose_;
    pose_ = p;
    if (p == Pose::FaceDown) push(MotionEv::FaceDown);
    else if (p == Pose::OnBack) push(MotionEv::OnBack);
    else if (p == Pose::UpsideDown) push(MotionEv::UpsideDown);
    else if (p == Pose::Upright) { if (old == Pose::FaceDown) push(MotionEv::FaceUp); push(MotionEv::Upright); }
  }
  // inclinação para os olhos (±30° ≈ fundo de escala)
  tiltX_ = iround(fclamp(nx * 2.f, -1.f, 1.f) * 1000.f);
  tiltY_ = iround(fclamp(-nz * 2.f, -1.f, 1.f) * 1000.f);
}

// ════════ Som ════════
void SoundSensor::reset() { *this = SoundSensor(); }

int SoundSensor::poll(SoundEv* out, int maxOut) {
  int n = 0;
  for (int i = 0; i < qn_ && n < maxOut; i++) out[n++] = q_[i];
  qn_ = 0;
  return n;
}

int SoundSensor::beatPhase(ms_t now) const {
  if (!music_ || !lastOnset_ || !beatMs_) return 0;
  return (int)(((now - lastOnset_) % beatMs_) * 1000 / beatMs_);
}

void SoundSensor::sample(int level, ms_t t) {
  samples_++;
  level = tbClamp(level, 0, 100);
  if (!init_) { init_ = true; base_ = (float)level; prev_ = level; lastT_ = t; level_ = level; return; }
  ms_t dt = t > lastT_ ? t - lastT_ : 1;
  if (dt > 1000) dt = 1000;
  lastT_ = t;
  prev_ = level_;
  level_ = level;
  // linha de base: sobe devagar (1 dB/s), desce rápido
  if ((float)level < base_) base_ += ((float)level - base_) * fclamp((float)dt / 400.f, 0.f, 1.f);
  else base_ += fminf_((float)level - base_, (float)dt / 1000.f);

  // palma: salto grande e curto acima da base
  int over = level - (int)base_;
  if (!clapArmed_ && over >= 24 && level - prev_ >= 14 && t >= clapCooldown_) { clapArmed_ = true; clapAt_ = t; }
  if (clapArmed_) {
    if (level - (int)base_ < 10) {
      clapArmed_ = false;
      if (t - clapAt_ <= 350) { push(SoundEv::Clap); clapCooldown_ = t + 600; }
    } else if (t - clapAt_ > 350) clapArmed_ = false;  // longo demais: não é palma
  }

  // barulho alto contínuo
  if (level >= 82) {
    if (!loudSince_) loudSince_ = t;
    if (t - loudSince_ >= 1500 && t >= loudCooldown_) { push(SoundEv::Loud); loudCooldown_ = t + 8000; }
  } else loudSince_ = 0;

  // música: ataques regulares com energia sustentada
  if (level - prev_ >= 6 && level >= (int)base_ + 6 && t - lastOnset_ >= 220) {
    if (onsetN_ == 16) { for (int i = 1; i < 16; i++) onsets_[i - 1] = onsets_[i]; onsetN_ = 15; }
    onsets_[onsetN_++] = t;
    lastOnset_ = t;
  }
  energyAcc_ += level; energyN_++;
  if (t >= musicCheck_) {
    musicCheck_ = t + 1000;
    int recent = 0;
    for (int i = 0; i < onsetN_; i++) if (t - onsets_[i] <= 6000) recent++;
    int avg = energyN_ ? energyAcc_ / energyN_ : 0;
    energyAcc_ = energyN_ = 0;
    // intervalo típico entre ataques (mediana simples dos últimos)
    uint16_t iv[15]; int ni = 0;
    for (int i = 1; i < onsetN_; i++) { ms_t d = onsets_[i] - onsets_[i - 1]; if (d >= 250 && d <= 1200 && t - onsets_[i] <= 6000) iv[ni++] = (uint16_t)d; }
    for (int i = 1; i < ni; i++) for (int j = i; j > 0 && iv[j] < iv[j - 1]; j--) { uint16_t x = iv[j]; iv[j] = iv[j - 1]; iv[j - 1] = x; }
    bool regular = false;
    if (ni >= 4) {
      uint16_t med = iv[ni / 2];
      int close = 0;
      for (int i = 0; i < ni; i++) if (tbAbs((int)iv[i] - (int)med) * 100 <= (int)med * 25) close++;
      regular = close * 100 >= ni * 60;
      if (regular) beatMs_ = med;
    }
    bool want = recent >= 6 && regular && avg >= (int)base_ + 4;
    bool keep = music_ && recent >= 3 && avg >= (int)base_ + 2;
    if (want && !music_) { music_ = true; push(SoundEv::MusicStart); }
    else if (music_ && !want && !keep) { music_ = false; push(SoundEv::MusicStop); }
  }
}

}  // namespace tb
