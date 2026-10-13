#!/usr/bin/env python3
"""Compacta web-ui/portal.html (gzip) em firmware/src/portal_gz.h para ir na flash do ESP32.
Também falha se o portal referenciar qualquer recurso externo (http/https)."""
import gzip, re, sys, pathlib
root = pathlib.Path(__file__).resolve().parent.parent
src = (root / "web-ui" / "portal.html").read_bytes()
ext = re.findall(rb'(?:src|href)\s*=\s*["\']https?://', src) + re.findall(rb'url\(\s*["\']?https?://', src) + re.findall(rb'@import', src)
if ext:
    sys.exit("ERRO: o portal referencia recursos externos: %r" % ext)
gz = gzip.compress(src, compresslevel=9, mtime=0)
out = root / "firmware" / "src" / "portal_gz.h"
lines = ["// Gerado por tools/embed_portal.py a partir de web-ui/portal.html - nao editar.",
         "#pragma once", "#include <stdint.h>", "#include <stddef.h>",
         f"static const size_t PORTAL_GZ_LEN = {len(gz)};  // original: {len(src)} bytes",
         "static const uint8_t PORTAL_GZ[] = {"]
for i in range(0, len(gz), 20):
    lines.append("  " + ",".join(f"0x{b:02x}" for b in gz[i:i+20]) + ",")
lines.append("};")
new = "\n".join(lines) + "\n"
if not out.exists() or out.read_text() != new:
    out.write_text(new)
print(f"portal: {len(src)} -> {len(gz)} bytes (gzip)")
