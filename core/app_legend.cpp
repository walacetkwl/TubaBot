// Reações lendárias: quando aparecem, o que contam como "descoberta", medalha e replay.
//
// Regras (pedido da dona do projeto): não se pede uma lendária com toques — elas
// vêm naturalmente com a afinidade (bond). Aparecem de vez em quando, nem todo
// dia, no máximo uma por dia, e só quando há alguém por perto (interação nos
// últimos 30 min, mas não logo depois de um toque). Na primeira vez aparece a
// medalha "Reação lendária desbloqueada!"; depois ela fica no menu Lendárias.
#include "app.h"
#include "app_ids.h"

namespace tb {

int64_t App::localDay(ms_t now) const {
  if (!clock_.valid()) return -1 - (int64_t)(now / 86400000ull);   // sem hora: "dias" desde que ligou
  int64_t ep = clock_.epoch(now) + (int64_t)TZ_TABLE[set_.tzIndex].offsetMin * 60;
  return ep / 86400;
}

int App::localMinute(ms_t now) const {
  if (!clock_.valid()) return -1;
  int64_t ep = clock_.epoch(now) + (int64_t)TZ_TABLE[set_.tzIndex].offsetMin * 60;
  return (int)((ep / 60) % 1440);
}

int App::legendsSeen() const {
  const Progress& Pr = testMode_ ? progTest_ : prog_;
  int n = 0;
  for (int i = 0; i < SCENE_COUNT; i++) if ((Pr.legendMask >> i) & 1u) n++;
  return n;
}

// Interações do dia e dos últimos 7 dias (telemetria). Grava pouco: só na virada do dia
// ou a cada 25 interações (o resto vai junto com o próximo salvamento).
void App::noteActivity(ms_t now) {
  Progress& Pr = P();
  uint16_t d = (uint16_t)(localDay(now) + 1);
  if (Pr.actDay != d) {
    int diff = (int)(uint16_t)(d - Pr.actDay);
    Pr.actMask = (uint8_t)(Pr.actDay == 0 || diff >= 8 || diff < 0 ? 0 : Pr.actMask << diff);
    Pr.actDay = d; Pr.interToday = 0;
    Pr.actMask |= 1;
    markProgress(now);
  }
  Pr.actMask |= 1;
  if (Pr.interToday < 65000) Pr.interToday++;
  if (Pr.interToday % 25 == 0) markProgress(now);
}

SceneId App::chooseLegend(ms_t now, bool soundTriggered) {
  const Progress& Pr = P();
  SceneId all[SCENE_COUNT], fresh[SCENE_COUNT];
  int na = 0, nf = 0;
  int m = localMinute(now);
  int toBed = 9999;
  bool night = bedtimeNow(now, &toBed);
  for (int i = 0; i < SCENE_COUNT; i++) {
    SceneId s = (SceneId)i;
    if (Pr.bond < sceneInfo(s).minBond) continue;
    if ((s == SceneId::Ouvidos) != soundTriggered) continue;              // grito só por som; o resto só por tempo
    if (s == SceneId::Cafe && !(m >= 6 * 60 && m < 11 * 60)) continue;     // cafezinho: só de manhã
    if (s == SceneId::Bocejo && !(night || (toBed >= 0 && toBed <= 40))) continue;
    if (s != SceneId::Bocejo && s != SceneId::Ouvidos && night) continue;  // de noite, só bocejo
    if (s == SceneId::PeixeCome && !((Pr.legendMask >> (int)SceneId::PeixeOlha) & 1u)) continue;  // primeiro só olha
    all[na++] = s;
    if (!((Pr.legendMask >> i) & 1u)) fresh[nf++] = s;
  }
  if (!na) return SceneId::Count;
  if (nf && rng_.range(0, 99) < 70) return fresh[rng_.range(0, nf - 1)];   // prefere as que ainda não viu
  return all[rng_.range(0, na - 1)];
}

void App::maybeLegend(ms_t now) {
  if (scene_.active() || now < nextLegendCheck_) return;
  nextLegendCheck_ = now + 60000;            // uma avaliação por minuto
  if (alertN_ || input_.pressed() || exprUntil_ || danceSince_ || dockVisible(now)) return;
  if (pomo_.phase() == PomoPhase::Focus && pomo_.state() == RunState::Running) return;  // não distrai o foco
  ms_t quiet = now - lastInteraction_;
  if (quiet < 30000 || quiet > 30ull * 60000) return;   // nem logo após um toque, nem sem ninguém por perto
  Progress& Pr = P();
  int64_t day = localDay(now);
  if (Pr.legendDay == (uint16_t)(day + 1)) return;      // no máximo uma por dia
  if (day != legendRollDay_) {
    // sorteio do dia: nem todo dia tem lendária; a chance cresce com a afinidade (18% → 55%)
    legendRollDay_ = day;
    Rng r((uint32_t)day * 2246822519u ^ rng_.next() ^ 0x1E6Du);
    legendToday_ = (int)r.range(0, 99) < 18 + (int)Pr.bond * 37 / 1000;
    legendAtMin_ = (int)r.range(7 * 60, 21 * 60 + 30);   // a partir de quando (hora local)
  }
  if (!legendToday_) return;
  int m = localMinute(now);
  if (m >= 0 && m < legendAtMin_ && !(m >= bedMin_ || m < wakeMin_)) return;
  if (m < 0 && rng_.range(0, 99) >= 10) return;          // sem hora: ~1 chance em 10 por minuto de plateia
  SceneId s = chooseLegend(now, false);
  if (s == SceneId::Count) return;
  Pr.legendDay = (uint16_t)(day + 1);
  markProgress(now);
  playLegend(s, now, true);
}

// natural = apareceu sozinha (conta como descoberta). Replay do menu: natural = false.
void App::playLegend(SceneId s, ms_t now, bool natural, Screen back) {
  if ((int)s >= SCENE_COUNT) return;
  Progress& Pr = P();
  sceneNew_ = false;
  if (natural && !((Pr.legendMask >> (int)s) & 1u)) {
    Pr.legendMask |= (uint16_t)(1u << (int)s);
    markProgress(now);
    sceneNew_ = true;
  }
  if (screen_ != Screen::Face) { transDir_ = 0; gotoScreen(Screen::Face, now); }
  dockUntil_ = 0;
  medalAt_ = 0;
  sceneBack_ = back;
  scene_.start(s, now);
  char b[48]; StrBuf e(b, sizeof b); e.put("lendária: ").put(sceneInfo(s).key); event(b);
  forceFrame_ = true;
}

void App::endScene(ms_t now) {
  if (!scene_.active()) return;
  SceneId s = scene_.id();
  scene_.stop();
  face_.setExpression(baseExpr_, now, true);
  exprUntil_ = 0;
  nextIdleExpr_ = now + 60000;
  if (sceneNew_) {
    sceneNew_ = false;
    medalAt_ = now; medalScene_ = s;
    out_.play(Pattern::Legend, now);
    event("reação lendária desbloqueada");
  }
  if (sceneBack_ != Screen::Face && screen_ == Screen::Face) {
    Screen b = sceneBack_;
    sceneBack_ = Screen::Face;
    gotoScreen(b, now);
  }
  forceFrame_ = true;
}

// Cena interrompida (outra tela, alerta): a descoberta continua valendo e a medalha aparece.
void App::abortScene(ms_t now) {
  if (!scene_.active()) return;
  SceneId s = scene_.id();
  scene_.stop();
  sceneBack_ = Screen::Face;
  if (sceneNew_) {
    sceneNew_ = false;
    medalAt_ = now; medalScene_ = s;
    out_.play(Pattern::Legend, now);
  }
}

void App::drawMedal(ms_t now) {
  if (!medalVisible(now)) return;
  float t = (float)(now - medalAt_);
  float k = t < 500.f ? t / 500.f : (t > (float)kMedalMs - 450.f ? ((float)kMedalMs - t) / 450.f : 1.f);
  if (t >= 500.f && t <= (float)kMedalMs - 450.f) k = 1.f;
  drawLegendMedal(canvas_, fclamp(k, 0.f, 1.f), tr("Reação lendária desbloqueada!"), tr(sceneInfo(medalScene_).name), ui::SURF, ui::TEXT, ui::MUTED);
  // um brilho girando na medalha
  float a = t / 260.f;
  canvas_.star(56.f + 20.f * fcos(a), 12.f + 16.f * fsin(a) + 30.f, 3.f, 1.3f, 0xFFFFFF, (int)(200.f * fclamp(k, 0.f, 1.f)));
  if (k > 0.6f) hitRect(B_MEDAL, 24.f, 12.f, 272.f, 60.f);
}

}  // namespace tb
