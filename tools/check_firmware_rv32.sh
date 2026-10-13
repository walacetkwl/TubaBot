#!/usr/bin/env bash
# VERIFICAÇÃO DE COMPILAÇÃO (não é o build oficial).
# Compila cada .cpp do firmware e do núcleo para objeto RISC-V rv32imc (a ISA do
# ESP32-C3) usando os headers REAIS do Arduino-ESP32 2.0.17 / ESP-IDF 4.4.7.
# Serve para pegar erros de tipo/API quando o PlatformIO não está disponível.
# Limitações: usa riscv64-unknown-elf-gcc 13 + headers do newlib/libstdc++ genéricos
# (não o toolchain riscv32-esp), e NÃO linka — não gera .bin.
#
# Uso: tools/check_firmware_rv32.sh <pasta-do-arduino-esp32-2.0.17> [full|lite]
set -euo pipefail
cd "$(dirname "$0")/.."
FW="${1:?informe a pasta do arduino-esp32 2.0.17}"
MODEL="${2:-full}"
CXX="${CXX:-riscv64-unknown-elf-g++}"
LIBSTDCXX="${LIBSTDCXX:-/usr/lib/arm-none-eabi/include/c++/13.2.1}"
OUT="${OUT:-build-check/$MODEL}"
mkdir -p "$OUT"

INCS=$(python3 - "$FW" <<'EOF'
import re, sys, os
fw = sys.argv[1]
src = open(os.path.join(fw, "tools", "platformio-build-esp32c3.py")).read()
block = src[src.index("CPPPATH=["):]
block = block[:block.index("],")]
out = []
for m in re.finditer(r'join\(FRAMEWORK_DIR,(.*?)\)\s*,?\s*\n', block):
    parts = re.findall(r'"([^"]*)"', m.group(1))
    if 'memory_type' in m.group(1):
        parts = ["tools", "sdk", "esp32c3", "qio_qspi", "include"]
    out.append(os.path.join(fw, *parts))
out.append(os.path.join(fw, "tools", "sdk", "esp32c3", "qio_qspi", "include"))
out.append(os.path.join(fw, "cores", "esp32"))
out.append(os.path.join(fw, "variants", "esp32c3"))
for lib in sorted(os.listdir(os.path.join(fw, "libraries"))):
    p = os.path.join(fw, "libraries", lib, "src")
    if os.path.isdir(p): out.append(p)
print(" ".join("-I" + p for p in out if os.path.isdir(p)))
EOF
)
GCCINC=$("$CXX" -print-file-name=include)
DEFS="-DHAVE_CONFIG_H -DMBEDTLS_CONFIG_FILE=\"mbedtls/esp_config.h\" -DUNITY_INCLUDE_CONFIG_H -DWITH_POSIX -D_GNU_SOURCE \
 -DIDF_VER=\"v4.4.7\" -DESP_PLATFORM -D_POSIX_READER_WRITER_LOCKS -DARDUINO_ARCH_ESP32 -DESP32 -DF_CPU=160000000L -DARDUINO=10812 \
 -DARDUINO_VARIANT=\"esp32c3\" -DARDUINO_BOARD=\"ESP32-C3-DevKitM-1\" -DARDUINO_PARTITION_partitions_tubabot \
 -DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1 -DCORE_DEBUG_LEVEL=1"
if [ "$MODEL" = lite ]; then DEFS="$DEFS -DTB_MODEL_FULL=0 -DTB_ENV_NAME=\"tubabot_lite\""; else DEFS="$DEFS -DTB_MODEL_FULL=1 -DTB_ENV_NAME=\"tubabot\""; fi
# newlib do IDF primeiro (platform_include já está no CPPPATH), depois newlib genérico, libstdc++ (newlib) e builtins do gcc
SYS="-nostdinc -nostdinc++ -isystem $LIBSTDCXX -isystem $LIBSTDCXX/arm-none-eabi -isystem $GCCINC -isystem /usr/include/newlib"
FLAGS="-march=rv32imc_zicsr_zifencei -mabi=ilp32 -std=gnu++17 -Os -ffunction-sections -fdata-sections -fno-rtti -fexceptions \
 -Wall -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -fno-jump-tables -fno-tree-switch-conversion"

python3 tools/embed_portal.py >/dev/null
fail=0
for f in core/*.cpp firmware/src/*.cpp; do
  o="$OUT/$(basename "${f%.cpp}").o"
  if "$CXX" $FLAGS $SYS $DEFS -Icore -Ifirmware/include $INCS -c "$f" -o "$o" 2> "$o.log"; then
    w=$(grep -c "warning:" "$o.log" || true)
    printf "ok   %-34s %7s bytes .text  (%s avisos)\n" "$f" "$(riscv64-unknown-elf-size -A "$o" | awk '/^\.text|^\.literal/ {s+=$2} END{print s+0}')" "$w"
  else
    echo "ERRO $f"; sed -n 1,40p "$o.log"; fail=1
  fi
done
echo "modelo: $MODEL  |  objetos em $OUT"
exit $fail
