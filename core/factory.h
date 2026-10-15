// Perfil de fábrica da unidade — separado das preferências do usuário.
//
// É gravado por nós ao preparar cada TubaBot (partição "fabrica" de 4 KB, que
// nem o usuário, nem a OTA, nem o "restaurar padrão" apagam). Define o que é
// propriedade da unidade (modelo Lite/Pro, cor física) e os PADRÕES de fábrica
// das preferências (tema, cor das expressões, volume...). O usuário personaliza
// depois; "Restaurar padrão de fábrica" volta exatamente a estes valores.
//
// Formato texto (UTF-8), gerado por tools/fabrica.py:
//
//   TUBABOT-FABRICA 1
//   modelo=pro                 lite | pro
//   cor=vermelho               cor física: vermelho | azul | branco | grafite
//   expressao=vermelho         cor padrão das expressões (padrão: a da cor física)
//   tema=escuro                escuro | claro
//   volume=60                  0..100
//   brilho=80                  10..100 (%)
//   personalidade=tranquilo    tranquilo | brincalhao
//   mostrar_hora=0             0 | 1   (idem mostrar_clima, mostrar_bateria)
//   formato_24h=1              0 | 1
//   fuso=America/Sao_Paulo     fuso IANA da tabela
//   serie=TB-2026-000123       identificação livre (até 23 caracteres)
//   crc=1A2B3C4D               CRC-32 (zlib) de todos os bytes antes desta linha
//
// Chaves desconhecidas são ignoradas (versões futuras podem acrescentar padrões).
#pragma once
#include "tb_base.h"
#include "settings.h"

namespace tb {

enum class Model : uint8_t { Lite = 0, Pro = 1 };

struct FactoryConfig {
  bool present = false;   // perfil gravado e válido
  Model model = Model::Lite;
  uint8_t body = 3;       // índice em BODIES (grafite)
  uint8_t exprColor = 3;  // índice em EXPR_COLORS
  uint8_t theme = 0;      // 0 escuro, 1 claro
  uint8_t volume = 60;
  uint8_t brightnessPct = 80;
  uint8_t personality = 0;
  uint8_t showClock = 0, showWeather = 0, showBattery = 0;
  uint8_t clock24h = 1;
  uint8_t tzIndex = 0;
  char serial[24] = {0};
};

static const size_t FACTORY_MAX = 1024;

// Lê o texto (pode terminar em 0x00 ou 0xFF de flash apagada). Retorna false se
// ausente/corrompido; 'why' recebe o motivo. Em falha, 'out' fica com os padrões.
bool parseFactory(const char* text, size_t len, FactoryConfig& out, const char** why = nullptr);
// Gera o texto com CRC (mesmo formato do tools/fabrica.py). Retorna o tamanho.
size_t formatFactory(const FactoryConfig& f, char* out, size_t cap);
// Aplica os padrões de fábrica sobre preferências recém-criadas.
void applyFactoryDefaults(const FactoryConfig& f, Settings& s);
const char* modelName(Model m);

}  // namespace tb
