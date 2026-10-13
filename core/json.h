// JSON mínimo, sem alocação: leitor de objetos rasos/aninhados e escritor em buffer.
// Usado pela API local compartilhada entre firmware e simulador.
#pragma once
#include "tb_base.h"

namespace tb {

// Trecho (span) de um valor JSON dentro do texto original.
struct JsonVal {
  const char* p = nullptr;
  size_t n = 0;
  bool ok() const { return p != nullptr; }
  bool isString() const { return ok() && n >= 2 && p[0] == '"'; }
  bool isObject() const { return ok() && p[0] == '{'; }
  bool isArray() const { return ok() && p[0] == '['; }
  bool isBool() const { return ok() && (p[0] == 't' || p[0] == 'f'); }
  bool isNumber() const { return ok() && (p[0] == '-' || (p[0] >= '0' && p[0] <= '9')); }
  bool isNull() const { return ok() && p[0] == 'n'; }

  // Busca chave em objeto (apenas no primeiro nível deste objeto).
  JsonVal get(const char* key) const;
  // i-ésimo elemento de array.
  JsonVal at(int i) const;
  int arrayLen() const;
  bool asBool(bool& out) const;
  bool asInt(int64_t& out) const;  // rejeita frações e números fora de faixa de int64
  // Decodifica string (escapes e \uXXXX -> UTF-8). Retorna false se não couber/for inválida.
  bool asString(char* out, size_t cap) const;
};

// Valida a sintaxe do documento inteiro (profundidade limitada) e devolve a raiz.
bool jsonParse(const char* s, size_t n, JsonVal& root);

// Escritor
struct JsonOut {
  StrBuf& sb;
  bool needComma[8] = {false};
  int depth = 0;
  explicit JsonOut(StrBuf& b) : sb(b) {}
  JsonOut& beginObj(const char* key = nullptr);
  JsonOut& endObj();
  JsonOut& beginArr(const char* key = nullptr);
  JsonOut& endArr();
  JsonOut& kstr(const char* key, const char* v);
  JsonOut& kint(const char* key, int64_t v);
  JsonOut& kbool(const char* key, bool v);
  JsonOut& knull(const char* key);
  JsonOut& kraw(const char* key, const char* raw);
  JsonOut& vstr(const char* v);
  JsonOut& vint(int64_t v);
 private:
  void sep();
  void keyName(const char* key);
};
void jsonEscape(StrBuf& sb, const char* s);

}  // namespace tb
