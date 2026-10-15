#!/usr/bin/env python3
"""Ferramenta de fábrica do TubaBot — prepara o perfil de cada unidade.

O firmware é o MESMO para todas as unidades. O que muda (Lite/Pro, cor física,
cor das expressões, tema inicial e outros padrões) fica num perfil de 4 KB
gravado na partição "fabrica" (endereço 0xFEF000), que não é apagada por
atualizações, regravação do firmware nem por "Restaurar padrão de fábrica".

Exemplos:
  # um TubaBot Pro vermelho, tema escuro (a expressão segue a cor do corpo)
  python3 tools/fabrica.py --modelo pro --cor vermelho

  # Lite branco com tema claro e número de série
  python3 tools/fabrica.py --modelo lite --cor branco --tema claro --serie TB-2026-000124

  # os 8 perfis padrão (Lite/Pro × 4 cores) de uma vez
  python3 tools/fabrica.py --todos

  # a partir de um arquivo de texto (chave=valor), ex.: firmware/fabrica/exemplo.txt
  python3 tools/fabrica.py --arquivo meu_perfil.txt

  # gravar direto pela USB (precisa do esptool: pip install esptool)
  python3 tools/fabrica.py --modelo pro --cor azul --gravar --porta /dev/cu.usbmodem1101

Saída: firmware/fabrica/fabrica-<modelo>-<cor>.bin (+ .txt legível).
Gravação pelo navegador (https://espressif.github.io/esptool-js/):
  0x0       firmware/dist/tubabot-v3.0.0-completo.bin
  0xFEF000  firmware/fabrica/fabrica-pro-vermelho.bin
"""
import argparse
import os
import subprocess
import sys
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "firmware", "fabrica")
OFFSET = 0xFEF000          # igual a firmware/partitions_tubabot.csv
SIZE = 0x1000
MAGIC = "TUBABOT-FABRICA 1"

MODELOS = ["lite", "pro"]
CORES = ["vermelho", "azul", "branco", "grafite"]          # cores físicas (BODIES no núcleo)
EXPRESSOES = ["vermelho", "azul", "branco", "grafite", "ciano", "verde", "lilas", "rosa", "amarelo", "laranja"]
TEMAS = ["escuro", "claro"]
ORDEM = ["modelo", "cor", "expressao", "tema", "volume", "brilho", "personalidade",
         "mostrar_hora", "mostrar_clima", "mostrar_bateria", "formato_24h", "fuso", "serie"]
PADRAO = {"tema": "escuro", "volume": "60", "brilho": "80", "personalidade": "tranquilo",
          "mostrar_hora": "0", "mostrar_clima": "0", "mostrar_bateria": "0", "formato_24h": "1",
          "fuso": "America/Sao_Paulo"}


def validar(p):
    erros = []
    if p.get("modelo") not in MODELOS:
        erros.append("modelo deve ser lite ou pro")
    if p.get("cor") not in CORES:
        erros.append("cor deve ser uma de: " + ", ".join(CORES))
    if p.get("expressao") not in EXPRESSOES:
        erros.append("expressao deve ser uma de: " + ", ".join(EXPRESSOES))
    if p.get("tema") not in TEMAS:
        erros.append("tema deve ser escuro ou claro")
    for k, lo, hi in (("volume", 0, 100), ("brilho", 10, 100)):
        try:
            v = int(p[k])
            assert lo <= v <= hi
        except Exception:
            erros.append(f"{k} deve ser um número de {lo} a {hi}")
    if p.get("personalidade") not in ("tranquilo", "brincalhao"):
        erros.append("personalidade deve ser tranquilo ou brincalhao")
    for k in ("mostrar_hora", "mostrar_clima", "mostrar_bateria", "formato_24h"):
        if p.get(k) not in ("0", "1"):
            erros.append(f"{k} deve ser 0 ou 1")
    if len(p.get("serie", "").encode()) > 23:
        erros.append("serie com no máximo 23 caracteres")
    if erros:
        raise SystemExit("perfil inválido:\n  - " + "\n  - ".join(erros))


def montar(p):
    p = dict(PADRAO, **{k: v for k, v in p.items() if v is not None})
    if "expressao" not in p or not p["expressao"]:
        p["expressao"] = p.get("cor")       # padrão: a cor das expressões acompanha o corpo
    if p.get("modelo") == "lite":
        p["mostrar_bateria"] = "0"          # Lite não tem bateria
    validar(p)
    linhas = [MAGIC] + [f"{k}={p[k]}" for k in ORDEM if p.get(k)]
    corpo = ("\n".join(linhas) + "\n").encode("utf-8")
    texto = corpo + f"crc={zlib.crc32(corpo) & 0xFFFFFFFF:08X}\n".encode()
    if len(texto) > SIZE:
        raise SystemExit("perfil grande demais")
    return p, texto


def ler_arquivo(caminho):
    p = {}
    with open(caminho, encoding="utf-8") as f:
        for ln in f:
            ln = ln.strip()
            if not ln or ln.startswith("#") or ln.startswith(MAGIC) or ln.startswith("crc="):
                continue
            if "=" not in ln:
                raise SystemExit(f"linha sem '=': {ln}")
            k, v = ln.split("=", 1)
            p[k.strip()] = v.strip()
    return p


def salvar(p, texto, saida):
    os.makedirs(saida, exist_ok=True)
    nome = f"fabrica-{p['modelo']}-{p['cor']}"
    if p.get("serie"):
        nome += "-" + "".join(c for c in p["serie"] if c.isalnum() or c in "-_")
    binp = os.path.join(saida, nome + ".bin")
    with open(binp, "wb") as f:
        f.write(texto + b"\xff" * (SIZE - len(texto)))
    with open(os.path.join(saida, nome + ".txt"), "wb") as f:
        f.write(texto)
    return binp


def main():
    ap = argparse.ArgumentParser(description="Gera o perfil de fábrica de um TubaBot.")
    ap.add_argument("--modelo", choices=MODELOS)
    ap.add_argument("--cor", choices=CORES, help="cor física do corpo")
    ap.add_argument("--expressao", choices=EXPRESSOES, help="cor padrão das expressões (padrão: a do corpo)")
    ap.add_argument("--tema", choices=TEMAS)
    ap.add_argument("--volume")
    ap.add_argument("--brilho")
    ap.add_argument("--personalidade", choices=["tranquilo", "brincalhao"])
    ap.add_argument("--serie")
    ap.add_argument("--arquivo", help="perfil em texto chave=valor")
    ap.add_argument("--todos", action="store_true", help="gera os 8 perfis padrão (Lite/Pro × 4 cores)")
    ap.add_argument("--saida", default=OUT_DIR)
    ap.add_argument("--gravar", action="store_true", help="grava na placa com o esptool")
    ap.add_argument("--porta", help="porta serial para --gravar")
    a = ap.parse_args()

    perfis = []
    if a.todos:
        perfis = [{"modelo": m, "cor": c} for m in MODELOS for c in CORES]
    else:
        p = ler_arquivo(a.arquivo) if a.arquivo else {}
        for k in ("modelo", "cor", "expressao", "tema", "volume", "brilho", "personalidade", "serie"):
            v = getattr(a, k)
            if v is not None:
                p[k] = v
        if not p.get("modelo") or not p.get("cor"):
            ap.error("informe --modelo e --cor (ou --arquivo, ou --todos)")
        perfis = [p]

    for p in perfis:
        full, texto = montar(p)
        binp = salvar(full, texto, a.saida)
        print(f"OK  {os.path.relpath(binp, ROOT)}  ({full['modelo']}, {full['cor']}, expressões {full['expressao']}, tema {full['tema']})")
        if a.gravar:
            cmd = [sys.executable, "-m", "esptool", "--chip", "esp32s3"]
            if a.porta:
                cmd += ["--port", a.porta]
            cmd += ["write_flash", hex(OFFSET), binp]
            print("    $ " + " ".join(cmd))
            subprocess.check_call(cmd)
    if not a.gravar:
        print(f"\nGrave no endereço {hex(OFFSET)} junto com o firmware (0x0). Veja docs/FABRICA.md.")


if __name__ == "__main__":
    main()
