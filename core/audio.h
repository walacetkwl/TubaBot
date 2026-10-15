// Sintetizador do TubaBot v2 (alto-falante via ES8311).
// Gera PCM 16 bits mono a 16 kHz, sem heap e sem ponto flutuante — o mesmo
// código toca no ESP32 (tarefa de áudio) e no simulador (WebAudio).
//
// Concorrência: o laço principal chama play()/stop()/setVolume(); a tarefa de
// áudio chama render(). A comunicação é por uma fila SPSC sem travas.
#pragma once
#include "tb_base.h"

namespace tb {

static const int kAudioRate = 16000;

enum class Wave : uint8_t { Sine, Triangle, Square, Pulse, Noise };

struct Note {
  uint16_t hz;     // 0 = pausa
  uint16_t ms;     // 0 com hz 0 = fim da sequência
  Wave wave;
  uint8_t vol;     // 0..100
  int16_t slide;   // variação de frequência até o fim da nota (Hz)
};

enum class Sound : uint8_t {
  None = 0, Boot, Tap, Double, Boop, Purr, Love, Menu, TimerDone, PomodoroDone, Reminder, Alarm,
  GamePoint, GameOver, Record, Unlock, Giggle, Dizzy, Startled, Yawn, Babble, Swipe, Charge, LowBattery,
  Breath, Test, Count
};

class Synth {
 public:
  static const int kVoices = 2;  // 0 = interface/reações, 1 = alertas
  // laço principal
  void play(Sound s, uint32_t seed = 0);
  void stopAll();
  void stopChannel(int ch);
  void setVolume(int v);         // 0..100
  // tarefa de áudio
  void render(int16_t* out, int n);
  bool busy() const { return busy_; }
  uint32_t rendered() const { return rendered_; }
  static uint32_t durationMs(Sound s);  // duração total (com repetições); babble estimado
  static int channelOf(Sound s);

 private:
  struct Cmd { uint8_t type, sound; uint32_t arg; };
  struct Voice {
    const Note* seq = nullptr;
    Note gen[12];               // sequência gerada (babble)
    uint8_t idx = 0, repeats = 0;
    uint32_t pos = 0, len = 0;  // amostras na nota atual
    uint32_t phase = 0;
    bool on = false;
  };
  void apply(const Cmd& c);
  void startNote(Voice& v);
  Cmd q_[16];
  volatile uint32_t head_ = 0, tail_ = 0;
  Voice v_[kVoices];
  int gain_ = 180;    // 0..256 (curva perceptiva do volume)
  uint32_t lfsr_ = 0xACE1u;
  volatile bool busy_ = false;
  uint32_t rendered_ = 0;
};

}  // namespace tb
