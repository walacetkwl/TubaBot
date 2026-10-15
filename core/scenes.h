// Reações lendárias: pequenas cenas raras em que o Tuba usa "mãos" e objetos
// (olhar o relógio, joinha, peixe, café, meditação, escada rolante, tampar os
// ouvidos, bocejo). Não podem ser pedidas por toque: aparecem sozinhas, de vez
// em quando, conforme a afinidade cresce (regras em App::maybeLegend). Depois de
// vistas uma vez ficam na coleção "Lendárias" e podem ser repetidas para os amigos.
//
// Cada cena é uma função pura do tempo decorrido (sem estado além do início):
// o mesmo quadro sai igual no firmware, no simulador e nos testes.
#pragma once
#include "face.h"
#include "outputs.h"

namespace tb {

enum class SceneId : uint8_t {
  Relogio = 0, Joinha, PeixeOlha, PeixeCome, Cafe, Meditacao, Escada, Ouvidos, Bocejo, Count
};

struct SceneInfo {
  const char* key;     // chave estável (API, simulador)
  const char* name;    // nome em pt-BR (passa por tr())
  const char* hint;    // como ela costuma aparecer (pt-BR, mostrado só depois de descoberta)
  uint16_t ms;         // duração
  uint16_t minBond;    // afinidade mínima (0..1000)
};
const SceneInfo& sceneInfo(SceneId s);
SceneId sceneFromKey(const char* k);   // SceneId::Count se não existir
static const int SCENE_COUNT = (int)SceneId::Count;

class ScenePlayer {
 public:
  void start(SceneId s, ms_t now) { id_ = s; t0_ = now; on_ = true; cue_ = 0; }
  void stop() { on_ = false; }
  bool active() const { return on_; }
  bool finished(ms_t now) const { return on_ && now - t0_ >= sceneInfo(id_).ms; }
  SceneId id() const { return id_; }
  ms_t elapsed(ms_t now) const { return now - t0_; }
  // Som do momento (cada deixa toca uma vez); Pattern::None quando não há.
  Pattern takeSound(ms_t now);
  // Desenha a cena em tela cheia (no lugar do rosto normal).
  void draw(Canvas& c, ms_t now, const FaceStyle& st) const;

 private:
  SceneId id_ = SceneId::Relogio;
  ms_t t0_ = 0;
  bool on_ = false;
  uint8_t cue_ = 0;
};

// Medalha "Reação lendária desbloqueada!" (desenhada sobre qualquer tela).
void drawLegendMedal(Canvas& c, float k, const char* title, const char* name, Color bg, Color text, Color muted);

}  // namespace tb
