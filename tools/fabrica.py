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

  # unidade de produção: número de série automático (registrado em firmware/fabrica/registro.csv)
  python3 tools/fabrica.py --modelo pro --cor azul --idioma es-419 --serie auto

  # gravar direto pela USB (precisa do esptool: pip install esptool)
  python3 tools/fabrica.py --modelo pro --cor azul --gravar --porta /dev/cu.usbmodem1101

Saída: firmware/fabrica/fabrica-<modelo>-<cor>.bin (+ .txt legível).
Gravação pelo navegador (https://espressif.github.io/esptool-js/):
  0x0       firmware/dist/tubabot-v3.1.0-completo.bin
  0xFEF000  firmware/fabrica/fabrica-pro-vermelho.bin
"""
import argparse
import csv
import datetime
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
IDIOMAS = ["pt-BR", "en", "es-419"]
ORDEM = ["modelo", "cor", "expressao", "tema", "volume", "brilho", "personalidade",
         "mostrar_hora", "mostrar_clima", "mostrar_bateria", "formato_24h", "fuso", "idioma", "telemetria", "serie"]
PADRAO = {"tema": "escuro", "volume": "60", "brilho": "80", "personalidade": "tranquilo",
          "mostrar_hora": "0", "mostrar_clima": "0", "mostrar_bateria": "0", "formato_24h": "1",
          "fuso": "America/Sao_Paulo", "idioma": "pt-BR", "telemetria": "1"}
REGISTRO = os.path.join(OUT_DIR, "registro.csv")
CAMPOS_REGISTRO = ["serie", "modelo", "cor", "expressao", "tema", "idioma", "gravado_em", "arquivo"]


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
    if p.get("idioma") not in IDIOMAS:
        erros.append("idioma deve ser um de: " + ", ".join(IDIOMAS))
    for k in ("mostrar_hora", "mostrar_clima", "mostrar_bateria", "formato_24h", "telemetria"):
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


def proxima_serie(modelo, cor, registro=REGISTRO):
    """Série única: TB3-<P|L><cor>-<AAMM>-<NNNNN> (sequência global do registro)."""
    n = 0
    if os.path.exists(registro):
        with open(registro, encoding="utf-8") as f:
            for row in csv.DictReader(f):
                try:
                    n = max(n, int(row["serie"].rsplit("-", 1)[1]))
                except (KeyError, ValueError, IndexError):
                    pass
    cor_cod = {"vermelho": "VM", "azul": "AZ", "branco": "BR", "grafite": "GF"}[cor]
    return f"TB3-{'P' if modelo == 'pro' else 'L'}{cor_cod}-{datetime.date.today():%y%m}-{n + 1:05d}"


def registrar(p, binp, registro=REGISTRO):
    """Acrescenta a unidade ao registro (base do painel: 'não ativados' = registrados que nunca deram sinal)."""
    novo = not os.path.exists(registro)
    os.makedirs(os.path.dirname(registro), exist_ok=True)
    with open(registro, "a", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=CAMPOS_REGISTRO)
        if novo:
            w.writeheader()
        w.writerow({"serie": p["serie"], "modelo": p["modelo"], "cor": p["cor"], "expressao": p["expressao"],
                    "tema": p["tema"], "idioma": p["idioma"], "gravado_em": datetime.datetime.now().isoformat(timespec="seconds"),
                    "arquivo": os.path.relpath(binp, ROOT)})


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
    ap.add_argument("--idioma", choices=IDIOMAS, help="idioma inicial da interface")
    ap.add_argument("--telemetria", choices=["0", "1"], help="estatísticas anônimas (padrão 1; o usuário pode desligar)")
    ap.add_argument("--serie", help="número de série; 'auto' gera o próximo e registra em firmware/fabrica/registro.csv")
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
        for k in ("modelo", "cor", "expressao", "tema", "volume", "brilho", "personalidade", "idioma", "telemetria", "serie"):
            v = getattr(a, k)
            if v is not None:
                p[k] = v
        if not p.get("modelo") or not p.get("cor"):
            ap.error("informe --modelo e --cor (ou --arquivo, ou --todos)")
        perfis = [p]

    for p in perfis:
        auto = p.get("serie") == "auto"
        if auto:
            p["serie"] = proxima_serie(p["modelo"], p["cor"])
        full, texto = montar(p)
        binp = salvar(full, texto, a.saida)
        if auto:
            registrar(full, binp)
            print(f"    série {full['serie']} registrada em {os.path.relpath(REGISTRO, ROOT)}")
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
