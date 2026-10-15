#!/usr/bin/env bash
# Gera firmware/dist: imagem completa (bootloader + partições + app) para gravar
# no endereço 0x0 (inclusive pelo navegador) e a imagem de app para OTA pelo portal.
set -euo pipefail
cd "$(dirname "$0")/.."
VER=$(sed -n 's/#define TB_VERSION "\(.*\)"/\1/p' core/tb_base.h)
E=~/.platformio/packages/tool-esptoolpy/esptool.py
B=$(find ~/.platformio/packages/framework-arduinoespressif32 -name boot_app0.bin | head -1)
mkdir -p firmware/dist
rm -f firmware/dist/*.bin firmware/dist/SHA256SUMS.txt
for env in tubabot tubabot_vibra; do
  python3 "$E" --chip esp32s3 merge_bin -o "firmware/dist/${env}-v${VER}-completo.bin" --flash_mode dio --flash_freq 80m --flash_size 16MB \
    0x0 ".pio/build/$env/bootloader.bin" 0x8000 ".pio/build/$env/partitions.bin" 0xe000 "$B" 0x10000 ".pio/build/$env/firmware.bin"
  cp ".pio/build/$env/firmware.bin" "firmware/dist/${env}-v${VER}-app-ota.bin"
done
(cd firmware/dist && sha256sum *.bin > SHA256SUMS.txt)
ls -la firmware/dist
