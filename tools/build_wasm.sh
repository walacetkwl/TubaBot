#!/usr/bin/env bash
# Compila o núcleo C++ (core/) para WebAssembly e gera simulator/tubabot_core.wasm.js
# (o .wasm embutido em base64, para o simulador abrir até por duplo clique).
#
# Requer um clang com alvo wasm32 e o wasm-ld (no Mac: brew install llvm lld;
# o clang da Apple NÃO inclui o alvo WebAssembly).
set -euo pipefail
cd "$(dirname "$0")/.."

CLANG="${CLANG:-}"
if [ -z "$CLANG" ]; then
  for c in /opt/homebrew/opt/llvm/bin/clang++ /usr/local/opt/llvm/bin/clang++ clang++; do
    if command -v "$c" >/dev/null 2>&1 && echo 'int x;' | "$c" --target=wasm32 -x c++ -c - -o /dev/null 2>/dev/null; then CLANG="$c"; break; fi
  done
fi
if [ -z "$CLANG" ]; then
  echo "Nenhum clang com alvo wasm32 encontrado. No Mac: brew install llvm lld" >&2
  echo "(O simulador já vem com o .wasm compilado; isto só é necessário após alterar core/.)" >&2
  exit 1
fi
LLVM_BIN="$(dirname "$(command -v "$CLANG")")"
export PATH="$LLVM_BIN:/opt/homebrew/opt/lld/bin:/usr/local/opt/lld/bin:$PATH"

OUT=simulator/tubabot_core.wasm
"$CLANG" --target=wasm32 -std=c++17 -O2 -ffreestanding -fno-builtin -nostdlib \
  -fno-exceptions -fno-rtti -fno-threadsafe-statics -fvisibility=hidden \
  -Wall -Wno-unused-parameter \
  -Wl,--no-entry -Wl,--strip-all -Wl,--initial-memory=2097152 -Wl,-z,stack-size=262144 \
  -Wl,--allow-undefined-file=simulator/imports.txt \
  -o "$OUT" core/*.cpp simulator/sim_main.cpp

python3 - "$OUT" <<'EOF' 2>/dev/null || node -e '
const fs=require("fs");const b=fs.readFileSync(process.argv[1]);
fs.writeFileSync("simulator/tubabot_core.wasm.js","// Gerado por tools/build_wasm.sh - nao editar\nwindow.TUBABOT_WASM_B64=\""+b.toString("base64")+"\";\n");' "$OUT"
import base64, sys
b = open(sys.argv[1], 'rb').read()
open('simulator/tubabot_core.wasm.js', 'w').write('// Gerado por tools/build_wasm.sh - nao editar\nwindow.TUBABOT_WASM_B64="' + base64.b64encode(b).decode() + '";\n')
EOF
echo "OK: $OUT ($(wc -c < "$OUT") bytes) e simulator/tubabot_core.wasm.js"
