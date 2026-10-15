// Idiomas da interface: português do Brasil (base), inglês e espanhol latino-americano.
// O texto-fonte é sempre o português: tr("Olá") devolve a tradução do idioma atual,
// ou o próprio texto quando não há tradução. Sem heap: tabela estática + cache por ponteiro.
#pragma once
#include "tb_base.h"

namespace tb {

enum class Lang : uint8_t { Pt = 0, En = 1, Es = 2, Count };

void setLang(Lang l);
Lang lang();
const char* tr(const char* pt);
const char* langKey(Lang l);    // "pt-BR", "en", "es-419" (API, perfil de fábrica)
const char* langName(Lang l);   // nome no próprio idioma: "Português", "English", "Español"
Lang langFromKey(const char* k);  // aceita pt, pt-BR, en, en-US, es, es-419...; Lang::Count se inválido

// Auditoria (testes): chamado com cada texto desenhado sem tradução (idioma ≠ pt).
extern void (*g_trMiss)(const char* text);

// testes: tradução existe para o texto (no idioma dado)?
bool trHas(const char* pt, Lang l);
int trTableSize();
const char* trTablePt(int i);

}  // namespace tb
