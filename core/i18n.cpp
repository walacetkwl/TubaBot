#include "i18n.h"

namespace tb {

struct TrEntry { const char* pt; const char* en; const char* es; };

#include "i18n_table.inc"

static const int kTrCount = (int)(sizeof TR_TABLE / sizeof TR_TABLE[0]);
static Lang g_lang = Lang::Pt;

// Índice por hash do conteúdo (o mesmo texto pode vir de um literal ou de um buffer
// copiado, ex.: rótulos de listas). Montado uma vez, na primeira consulta.
static const int kSlots = 2048;
static uint16_t g_idx[kSlots];   // 0 = vazio; senão índice + 1
static bool g_built = false;

static uint32_t hashStr(const char* s) {
  uint32_t h = 2166136261u;
  while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
  return h;
}

static void build() {
  for (int i = 0; i < kSlots; i++) g_idx[i] = 0;
  for (int i = 0; i < kTrCount; i++) {
    uint32_t h = hashStr(TR_TABLE[i].pt) & (kSlots - 1);
    while (g_idx[h]) h = (h + 1) & (kSlots - 1);
    g_idx[h] = (uint16_t)(i + 1);
  }
  g_built = true;
}

void setLang(Lang l) {
  if ((int)l >= (int)Lang::Count) l = Lang::Pt;
  g_lang = l;
}
Lang lang() { return g_lang; }

static const TrEntry* find(const char* pt) {
  if (!g_built) build();
  uint32_t h = hashStr(pt) & (kSlots - 1);
  while (g_idx[h]) {
    const TrEntry* e = &TR_TABLE[g_idx[h] - 1];
    if (tbStreq(e->pt, pt)) return e;
    h = (h + 1) & (kSlots - 1);
  }
  return nullptr;
}

void (*g_trMiss)(const char* text) = nullptr;

const char* tr(const char* pt) {
  if (g_lang == Lang::Pt || !pt || !pt[0]) return pt;
  const TrEntry* e = find(pt);
  if (!e) {
    if (g_trMiss) {   // auditoria: ignora textos que já são traduções (desenho passa por tr() duas vezes)
      bool out = false;
      for (int i = 0; i < kTrCount && !out; i++) out = tbStreq(g_lang == Lang::En ? TR_TABLE[i].en : TR_TABLE[i].es, pt);
      if (!out) g_trMiss(pt);
    }
    return pt;
  }
  const char* v = g_lang == Lang::En ? e->en : e->es;
  return v && v[0] ? v : pt;
}

const char* langKey(Lang l) { return l == Lang::En ? "en" : l == Lang::Es ? "es-419" : "pt-BR"; }
const char* langName(Lang l) { return l == Lang::En ? "English" : l == Lang::Es ? "Español" : "Português"; }
Lang langFromKey(const char* k) {
  if (!k) return Lang::Count;
  if ((k[0] == 'p' || k[0] == 'P') && (k[1] == 't' || k[1] == 'T') && (k[2] == 0 || k[2] == '-' || k[2] == '_')) return Lang::Pt;
  if ((k[0] == 'e' || k[0] == 'E') && (k[1] == 'n' || k[1] == 'N') && (k[2] == 0 || k[2] == '-' || k[2] == '_')) return Lang::En;
  if ((k[0] == 'e' || k[0] == 'E') && (k[1] == 's' || k[1] == 'S') && (k[2] == 0 || k[2] == '-' || k[2] == '_')) return Lang::Es;
  if (tbStreq(k, "portugues") || tbStreq(k, "português")) return Lang::Pt;
  if (tbStreq(k, "ingles") || tbStreq(k, "inglês") || tbStreq(k, "english")) return Lang::En;
  if (tbStreq(k, "espanhol") || tbStreq(k, "español") || tbStreq(k, "espanol")) return Lang::Es;
  return Lang::Count;
}

bool trHas(const char* pt, Lang l) {
  if (l == Lang::Pt) return true;
  const TrEntry* e = find(pt);
  if (!e) return false;
  const char* v = l == Lang::En ? e->en : e->es;
  return v && v[0];
}
int trTableSize() { return kTrCount; }
const char* trTablePt(int i) { return (unsigned)i < (unsigned)kTrCount ? TR_TABLE[i].pt : nullptr; }

}  // namespace tb
