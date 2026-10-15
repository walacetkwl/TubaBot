// Ícones vetoriais do TubaBot v2 (desenhados com as primitivas do Canvas,
// nítidos em qualquer tamanho e em qualquer cor).
#pragma once
#include "gfx.h"

namespace tb {

enum class Icon : uint8_t {
  Face, Clock, Tomato, Timer, Stopwatch, Alarm, Games, Silent, Sparkle, Gear, Info, Breath, Level, Dice,
  Sun, Cloud, PartCloud, Rain, Storm, Snow, Fog, Moon,
  Wifi, Speaker, Brightness, Vibrate, Back, Check, Cross, Play, Pause, Skip, Reset, Plus, Minus, Heart, Bell,
  Note, Stats, Lock, Fish, Shark, Weather, Bolt, Count
};

void drawIcon(Canvas& c, Icon id, float cx, float cy, float size, Color col, int a = 255);

// Bateria com nível e raio de carga.
void drawBattery(Canvas& c, float cx, float cy, float w, int percent, bool charging, Color col, int a = 255);

}  // namespace tb
