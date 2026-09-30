#!/usr/bin/env python3
"""
validacao-aleatoria.py - Validação cruzada com um oráculo independente.

Gera muitas matrizes aleatórias (tamanhos, densidades e configurações de
threads/blocos sorteados), executa as duas versões em C e compara com
scipy.ndimage.label usando estrutura 3x3 (conectividade 8).

O gerador xorshift32 abaixo é idêntico ao de src/comum.c, então a opção
-g dos programas produz exatamente a mesma matriz que este script.

Uso: python3 tests/validacao-aleatoria.py [quantidade]
"""
import random
import subprocess
import sys

import numpy as np
from scipy.ndimage import label

SEQ = "./conta-objetos-sequencial"
PAR = "./conta-objetos-paralelo"


def gerar(linhas, colunas, densidade, semente):
    """Réplica em Python de matriz_gerar() (src/comum.c)."""
    total = linhas * colunas
    x = semente & 0xFFFFFFFF or 2463534242
    limiar = int(densidade * 4294967295.0)
    saida = np.empty(total, dtype=np.uint8)
    for i in range(total):
        x ^= (x << 13) & 0xFFFFFFFF
        x ^= x >> 17
        x ^= (x << 5) & 0xFFFFFFFF
        saida[i] = 1 if x < limiar else 0
    return saida.reshape(linhas, colunas)


def objetos(cmd):
    out = subprocess.run(cmd, capture_output=True, text=True, check=True).stdout
    for linha in out.splitlines():
        if linha.startswith("Objetos:"):
            return int(linha.split()[1])
    raise RuntimeError(out)


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    rnd = random.Random(2026)
    falhas = 0
    for caso in range(1, n + 1):
        l, c = rnd.randint(1, 60), rnd.randint(1, 60)
        d = rnd.choice([0.05, 0.2, 0.35, 0.4, 0.45, 0.5, 0.6, 0.8, 1.0])
        s = rnd.randint(1, 10**6)
        t = rnd.randint(1, 8)
        bl, bc = rnd.randint(1, 12), rnd.randint(1, 12)
        g = ["-g", str(l), str(c), str(d), str(s)]

        esperado = label(gerar(l, c, d, s), structure=np.ones((3, 3)))[1]
        seq = objetos([SEQ] + g)
        par = objetos([PAR, "-t", str(t), "-b", str(bl), str(bc)] + g)
        if not (esperado == seq == par):
            falhas += 1
            print(f"FALHA caso {caso}: {l}x{c} d={d} s={s} t={t} "
                  f"b={bl}x{bc} -> scipy={esperado} seq={seq} par={par}")
    print(f"Casos aleatorios: {n} | Falhas: {falhas}")
    sys.exit(1 if falhas else 0)


if __name__ == "__main__":
    main()
