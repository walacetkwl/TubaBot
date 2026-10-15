#include "icons.h"

namespace tb {

void drawIcon(Canvas& c, Icon id, float x, float y, float s, Color col, int a) {
  const float u = s / 24.f;      // grade de 24 unidades
  const float t = fmaxf_(2.2f * u, 1.5f);  // espessura de traço
  auto X = [&](float v) { return x + (v - 12.f) * u; };
  auto Y = [&](float v) { return y + (v - 12.f) * u; };
  auto L = [&](float x0, float y0, float x1, float y1, float th) { c.line(X(x0), Y(y0), X(x1), Y(y1), th, col, a); };
  switch (id) {
    case Icon::Face:
      c.ring(x, y, 10 * u, t, col, a);
      c.fillRoundRect(X(7.6f), Y(7.5f), 2.6f * u, 4.2f * u, 1.3f * u, col, a);
      c.fillRoundRect(X(13.8f), Y(7.5f), 2.6f * u, 4.2f * u, 1.3f * u, col, a);
      c.arc(x, Y(12.5f), 4.2f * u, t, kPi - 0.95f, 1.9f, col, a);
      break;
    case Icon::Clock:
      c.ring(x, y, 10 * u, t, col, a);
      L(12, 12, 12, 6.5f, t); L(12, 12, 16, 14, t);
      break;
    case Icon::Tomato:
      c.fillCircle(x, Y(13.5f), 9 * u, col, a);
      c.fillEllipse(X(9), Y(5.2f), 3.2f * u, 1.6f * u, 0x4BD37B, a);
      c.fillEllipse(X(15), Y(5.2f), 3.2f * u, 1.6f * u, 0x4BD37B, a);
      L(12, 6, 12, 2.5f, t);
      c.fillCircle(X(8.6f), Y(10.6f), 1.6f * u, 0xFFFFFF, a * 3 / 5);
      break;
    case Icon::Timer: {
      // ampulheta
      Pt top[3] = {{X(6), Y(4)}, {X(18), Y(4)}, {X(12), Y(12)}};
      Pt bot[3] = {{X(12), Y(12)}, {X(18), Y(20)}, {X(6), Y(20)}};
      L(5, 3, 19, 3, t); L(5, 21, 19, 21, t);
      c.fillPolygon(top, 3, col, a / 3);
      c.fillPolygon(bot, 3, col, a);
      L(6, 4, 12, 12, t); L(18, 4, 12, 12, t); L(12, 12, 6, 20, t); L(12, 12, 18, 20, t);
      break;
    }
    case Icon::Stopwatch:
      c.ring(x, Y(13.5f), 8.5f * u, t, col, a);
      L(12, 13.5f, 12, 8.5f, t); L(9.5f, 2.5f, 14.5f, 2.5f, t); L(12, 2.5f, 12, 5, t); L(18.5f, 6.5f, 20, 5, t);
      break;
    case Icon::Alarm:
      c.ring(x, Y(13), 8 * u, t, col, a);
      L(12, 13, 12, 9, t); L(12, 13, 15, 15, t);
      c.arc(X(4.5f), Y(5.5f), 3 * u, t, -0.8f, 1.8f, col, a);
      c.arc(X(19.5f), Y(5.5f), 3 * u, t, -1.0f, 1.8f, col, a);
      L(7, 20, 5.5f, 22, t); L(17, 20, 18.5f, 22, t);
      break;
    case Icon::Menu:
      for (int k = 0; k < 9; k++) c.fillRoundRect(X(3.f + 6.6f * (float)(k % 3)), Y(3.f + 6.6f * (float)(k / 3)), 4.8f * u, 4.8f * u, 1.4f * u, col, a);
      break;
    case Icon::Palette:
      c.fillCircle(x, y, 9.5f * u, col, a);
      c.fillCircle(X(8), Y(8.5f), 1.8f * u, 0x000000, a);
      c.fillCircle(X(13), Y(6.5f), 1.8f * u, 0x000000, a);
      c.fillCircle(X(17), Y(10), 1.8f * u, 0x000000, a);
      c.fillCircle(X(14.5f), Y(16.5f), 2.6f * u, 0x000000, a);
      break;
    case Icon::Games:
      c.fillRoundRect(X(2), Y(7), 20 * u, 11 * u, 5.5f * u, col, a);
      c.fillRect((int)X(6.2f), (int)Y(10.2f), (int)(1.6f * u + 1), (int)(5 * u), 0x000000, a);
      c.fillRect((int)X(4.5f), (int)Y(11.9f), (int)(5 * u), (int)(1.6f * u + 1), 0x000000, a);
      c.fillCircle(X(16), Y(11), 1.4f * u, 0x000000, a);
      c.fillCircle(X(18.5f), Y(13.8f), 1.4f * u, 0x000000, a);
      break;
    case Icon::Bell:
    case Icon::Silent: {
      Pt b[6] = {{X(3.5f), Y(18)}, {X(6), Y(14.5f)}, {X(6), Y(10)}, {X(18), Y(10)}, {X(18), Y(14.5f)}, {X(20.5f), Y(18)}};
      c.fillPolygon(b, 6, col, a);
      c.fillCircle(x, Y(10), 6 * u, col, a);
      c.fillCircle(x, Y(2.8f), 1.6f * u, col, a);
      c.fillCircle(x, Y(20), 2.4f * u, col, a);
      if (id == Icon::Silent) { c.line(X(3), Y(3), X(21), Y(21), t * 2.6f, 0x000000, a); c.line(X(3), Y(3), X(21), Y(21), t, col, a); }
      break;
    }
    case Icon::Sparkle: {
      Pt v[8] = {{X(12), Y(1)}, {X(14.5f), Y(9.5f)}, {X(23), Y(12)}, {X(14.5f), Y(14.5f)}, {X(12), Y(23)}, {X(9.5f), Y(14.5f)}, {X(1), Y(12)}, {X(9.5f), Y(9.5f)}};
      c.fillPolygon(v, 8, col, a);
      break;
    }
    case Icon::Gear: {
      for (int i = 0; i < 8; i++) {
        float ang = (float)i * kPi / 4.f;
        c.line(x + 6.5f * u * fsin(ang), y - 6.5f * u * fcos(ang), x + 10.5f * u * fsin(ang), y - 10.5f * u * fcos(ang), 3.6f * u, col, a);
      }
      c.ring(x, y, 6.2f * u, 3.6f * u, col, a);
      break;
    }
    case Icon::Info:
      c.ring(x, y, 10 * u, t, col, a);
      c.fillCircle(x, Y(7.3f), 1.5f * u, col, a);
      L(12, 11, 12, 17, t * 1.1f);
      break;
    case Icon::Breath:
      L(3, 8, 14, 8, t); c.arc(X(14), Y(5), 3 * u, t, kPi * 0.5f, kPi * 1.5f, col, a);
      L(3, 12.5f, 18, 12.5f, t); c.arc(X(18), Y(9.5f), 3 * u, t, kPi * 0.5f, kPi * 1.5f, col, a);
      L(3, 17, 12, 17, t); c.arc(X(12), Y(20), 3 * u, t, -kPi * 0.5f, kPi * 1.3f, col, a);
      break;
    case Icon::Level:
      c.strokeRoundRect(X(2), Y(8), 20 * u, 8 * u, 4 * u, t, col, a);
      c.fillCircle(X(13.5f), y, 2.4f * u, col, a);
      L(9, 9.5f, 9, 14.5f, t * 0.7f); L(15, 9.5f, 15, 14.5f, t * 0.7f);
      break;
    case Icon::Dice:
      c.fillRoundRect(X(3), Y(3), 18 * u, 18 * u, 4.5f * u, col, a);
      for (int i = 0; i < 5; i++) {
        static const float P[5][2] = {{8, 8}, {16, 8}, {12, 12}, {8, 16}, {16, 16}};
        c.fillCircle(X(P[i][0]), Y(P[i][1]), 1.7f * u, 0x000000, a);
      }
      break;
    case Icon::Sun:
      c.fillCircle(x, y, 5 * u, 0xFFC93D, a);
      for (int i = 0; i < 8; i++) {
        float ang = (float)i * kPi / 4.f;
        c.line(x + 8 * u * fsin(ang), y - 8 * u * fcos(ang), x + 10.8f * u * fsin(ang), y - 10.8f * u * fcos(ang), t, 0xFFC93D, a);
      }
      break;
    case Icon::Moon: {
      float s2 = u;
      c.shade(X(2), Y(2), X(22), Y(22), col, a, [=](float px, float py) {
        float d1 = fsqrt((px - x) * (px - x) + (py - y) * (py - y)) - 9.f * s2;
        float d2 = fsqrt((px - (x + 5.5f * s2)) * (px - (x + 5.5f * s2)) + (py - (y - 4.f * s2)) * (py - (y - 4.f * s2))) - 7.f * s2;
        return fmaxf_(d1, -d2);
      });
      break;
    }
    case Icon::Cloud:
    case Icon::PartCloud:
    case Icon::Rain:
    case Icon::Storm:
    case Icon::Snow:
    case Icon::Fog:
    case Icon::Weather: {
      if (id == Icon::PartCloud || id == Icon::Weather) {
        c.fillCircle(X(15.5f), Y(7.5f), 5 * u, 0xFFC93D, a);
        for (int i = 0; i < 5; i++) {
          float ang = -1.2f + (float)i * 0.75f;
          c.line(X(15.5f) + 7 * u * fsin(ang), Y(7.5f) - 7 * u * fcos(ang), X(15.5f) + 9 * u * fsin(ang), Y(7.5f) - 9 * u * fcos(ang), t * 0.8f, 0xFFC93D, a);
        }
      }
      Color cc = id == Icon::Storm ? 0x9AA5B8 : 0xE8EEF7;
      float oy = (id == Icon::Rain || id == Icon::Storm || id == Icon::Snow) ? -2.f : 0.f;
      c.fillCircle(X(9), Y(13 + oy), 4.5f * u, cc, a);
      c.fillCircle(X(14), Y(11 + oy), 5.5f * u, cc, a);
      c.fillRoundRect(X(4.5f), Y(13 + oy), 15.5f * u, 5 * u, 2.5f * u, cc, a);
      if (id == Icon::Rain) for (int i = 0; i < 3; i++) L(7.5f + 4 * i, 19, 6.5f + 4 * i, 22, t * 0.8f);
      if (id == Icon::Snow) for (int i = 0; i < 3; i++) c.fillCircle(X(7.5f + 4.5f * i), Y(20.5f), 1.3f * u, 0xFFFFFF, a);
      if (id == Icon::Storm) { Pt b[4] = {{X(13), Y(16)}, {X(9.5f), Y(21)}, {X(12.5f), Y(20.5f)}, {X(11), Y(24)}}; c.fillPolygon(b, 4, 0xFFC93D, a); }
      if (id == Icon::Fog) for (int i = 0; i < 2; i++) L(4, 20.5f + 2.6f * i, 20, 20.5f + 2.6f * i, t * 0.8f);
      break;
    }
    case Icon::Wifi:
      c.arc(x, Y(19), 15 * u, t, -0.75f, 1.5f, col, a);
      c.arc(x, Y(19), 10 * u, t, -0.75f, 1.5f, col, a);
      c.arc(x, Y(19), 5 * u, t, -0.75f, 1.5f, col, a);
      c.fillCircle(x, Y(19), 1.8f * u, col, a);
      break;
    case Icon::Speaker: {
      Pt b[6] = {{X(3), Y(9)}, {X(7), Y(9)}, {X(12), Y(4.5f)}, {X(12), Y(19.5f)}, {X(7), Y(15)}, {X(3), Y(15)}};
      c.fillPolygon(b, 6, col, a);
      c.arc(X(12), y, 4.5f * u, t, 0.6f, 1.9f, col, a);
      c.arc(X(12), y, 8.5f * u, t, 0.7f, 1.7f, col, a);
      break;
    }
    case Icon::Brightness:
      c.fillCircle(x, y, 4.5f * u, col, a);
      for (int i = 0; i < 8; i++) {
        float ang = (float)i * kPi / 4.f;
        c.line(x + 7.5f * u * fsin(ang), y - 7.5f * u * fcos(ang), x + 10.3f * u * fsin(ang), y - 10.3f * u * fcos(ang), t, col, a);
      }
      break;
    case Icon::Vibrate:
      c.strokeRoundRect(X(7.5f), Y(3), 9 * u, 18 * u, 2.5f * u, t, col, a);
      L(3.5f, 8, 3.5f, 16, t); L(20.5f, 8, 20.5f, 16, t);
      break;
    case Icon::Back:
      L(15, 4, 7, 12, t * 1.3f); L(7, 12, 15, 20, t * 1.3f);
      break;
    case Icon::Check:
      L(4.5f, 12.5f, 10, 18, t * 1.4f); L(10, 18, 20, 6.5f, t * 1.4f);
      break;
    case Icon::Cross:
      L(5.5f, 5.5f, 18.5f, 18.5f, t * 1.4f); L(5.5f, 18.5f, 18.5f, 5.5f, t * 1.4f);
      break;
    case Icon::Play: {
      Pt v[3] = {{X(7), Y(4)}, {X(20), Y(12)}, {X(7), Y(20)}};
      c.fillPolygon(v, 3, col, a);
      break;
    }
    case Icon::Pause:
      c.fillRoundRect(X(6), Y(4), 4 * u, 16 * u, 1.5f * u, col, a);
      c.fillRoundRect(X(14), Y(4), 4 * u, 16 * u, 1.5f * u, col, a);
      break;
    case Icon::Skip: {
      Pt v[3] = {{X(4), Y(5)}, {X(15), Y(12)}, {X(4), Y(19)}};
      c.fillPolygon(v, 3, col, a);
      c.fillRoundRect(X(16), Y(5), 3.5f * u, 14 * u, 1.5f * u, col, a);
      break;
    }
    case Icon::Reset: {
      c.arc(x, y, 8 * u, t, 0.9f, 4.9f, col, a);
      float ex = x + 8 * u * fsin(0.9f), ey = y - 8 * u * fcos(0.9f);
      Pt v[3] = {{ex + 3.5f * u, ey - 4.5f * u}, {ex + 3.2f * u, ey + 3.6f * u}, {ex - 4.f * u, ey + 0.5f * u}};
      c.fillPolygon(v, 3, col, a);
      break;
    }
    case Icon::Plus: L(12, 5, 12, 19, t * 1.3f); L(5, 12, 19, 12, t * 1.3f); break;
    case Icon::Minus: L(5, 12, 19, 12, t * 1.3f); break;
    case Icon::Heart: c.heart(x, Y(12.5f), 20 * u, col, a); break;
    case Icon::Note:
      c.fillEllipse(X(7.5f), Y(18), 3.6f * u, 2.8f * u, col, a);
      c.fillEllipse(X(17.5f), Y(16), 3.6f * u, 2.8f * u, col, a);
      L(10.6f, 18, 10.6f, 5, t); L(20.6f, 16, 20.6f, 3, t); L(10.6f, 5, 20.6f, 3, t * 1.4f);
      break;
    case Icon::Stats:
      c.fillRoundRect(X(3), Y(13), 4.5f * u, 8 * u, 1.5f * u, col, a);
      c.fillRoundRect(X(9.75f), Y(7), 4.5f * u, 14 * u, 1.5f * u, col, a);
      c.fillRoundRect(X(16.5f), Y(3), 4.5f * u, 18 * u, 1.5f * u, col, a);
      break;
    case Icon::Lock:
      c.arc(x, Y(9.5f), 4.8f * u, t, -1.6f, 3.2f, col, a);
      L(7.2f, 9.5f, 7.2f, 11, t); L(16.8f, 9.5f, 16.8f, 11, t);
      c.fillRoundRect(X(4.5f), Y(11), 15 * u, 10 * u, 2.5f * u, col, a);
      break;
    case Icon::Fish: {
      c.fillEllipse(X(10.5f), y, 7.5f * u, 5 * u, col, a);
      Pt v[3] = {{X(16), Y(12)}, {X(22.5f), Y(6.5f)}, {X(22.5f), Y(17.5f)}};
      c.fillPolygon(v, 3, col, a);
      c.fillCircle(X(6.5f), Y(10.8f), 1.2f * u, 0x000000, a);
      break;
    }
    case Icon::Shark: {
      Pt fin[3] = {{X(7), Y(14)}, {X(14), Y(3)}, {X(17), Y(14)}};
      c.fillPolygon(fin, 3, col, a);
      for (int i = 0; i < 6; i++) c.fillCircle(X(2 + i * 4), Y(17.5f), 2.6f * u, 0x3B8BFF, a);
      break;
    }
    case Icon::Bolt: {
      Pt b[6] = {{X(14), Y(2)}, {X(5), Y(14)}, {X(11.5f), Y(14)}, {X(9.5f), Y(22)}, {X(19), Y(9.5f)}, {X(12.5f), Y(9.5f)}};
      c.fillPolygon(b, 6, col, a);
      break;
    }
    default: break;
  }
}

void drawBattery(Canvas& c, float cx, float cy, float w, int pct, bool charging, Color col, int a) {
  float h = w * 0.5f;
  float x = cx - w * 0.5f, y = cy - h * 0.5f;
  float t = fmaxf_(w * 0.07f, 1.5f);
  c.strokeRoundRect(x, y, w - t * 1.4f, h, h * 0.28f, t, col, a);
  c.fillRoundRect(x + w - t * 1.2f, cy - h * 0.2f, t * 1.2f, h * 0.4f, t * 0.5f, col, a);
  if (pct >= 0) {
    Color fill = pct <= 15 ? 0xFF4B4B : (pct <= 35 ? 0xFFB547 : 0x4BF0A6);
    float iw = (w - t * 1.4f - t * 2.4f) * (float)tbClamp(pct, 0, 100) / 100.f;
    if (iw > 0.5f) c.fillRoundRect(x + t * 1.2f, y + t * 1.2f, iw, h - t * 2.4f, h * 0.14f, fill, a);
  }
  if (charging) {
    float u = h / 24.f;
    Pt b[6] = {{cx + 2 * u, cy - 11 * u}, {cx - 6 * u, cy + 2 * u}, {cx - 0.5f * u, cy + 2 * u}, {cx - 2 * u, cy + 11 * u}, {cx + 6 * u, cy - 2 * u}, {cx + 0.5f * u, cy - 2 * u}};
    c.fillPolygon(b, 6, 0xFFFFFF, a);
  }
}

}  // namespace tb
