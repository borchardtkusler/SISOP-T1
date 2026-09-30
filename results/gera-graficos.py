#!/usr/bin/env python3
"""
gera-graficos.py - Lê results/medicoes.csv (dados brutos) e gera:
  results/resumo.csv            mediana, quartis, aceleração e eficiência
  results/grafico-tempo.png     tempo (mediana + intervalo interquartil)
  results/grafico-aceleracao.png
  results/grafico-eficiencia.png

Medida representativa: MEDIANA (robusta a execuções atípicas).
Dispersão: intervalo interquartil (IQR = Q3 - Q1).
"""
import csv
import os
import statistics
from collections import defaultdict

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

BASE = os.path.dirname(os.path.abspath(__file__))
CSV = os.path.join(BASE, "medicoes.csv")

AZUL, LARANJA = "#2a78d6", "#eb6834"
TEXTO, TEXTO2, GRADE, FUNDO = "#0b0b0b", "#52514e", "#e4e3df", "#fcfcfb"

plt.rcParams.update({
    "figure.facecolor": FUNDO, "axes.facecolor": FUNDO,
    "axes.edgecolor": TEXTO2, "axes.labelcolor": TEXTO,
    "xtick.color": TEXTO2, "ytick.color": TEXTO2, "text.color": TEXTO,
    "axes.grid": True, "grid.color": GRADE, "grid.linewidth": 0.8,
    "axes.spines.top": False, "axes.spines.right": False,
    "font.size": 11, "axes.titlesize": 13, "axes.titleweight": "bold",
})


def quartis(v):
    q = statistics.quantiles(v, n=4, method="inclusive")
    return q[0], statistics.median(v), q[2]


def carregar():
    dados = defaultdict(list)  # (matriz, versao, p) -> [tempos]
    dims = {}
    with open(CSV) as f:
        for r in csv.DictReader(f):
            if r["resultado_correto"] != "true":
                raise SystemExit(f"Resultado incorreto no CSV: {r}")
            dados[(r["matriz"], r["versao"], int(r["trabalhadores"]))].append(
                float(r["tempo_ms"]))
            dims[r["matriz"]] = f'{r["linhas"]} x {r["colunas"]}'
    return dados, dims


def resumir(dados):
    linhas = []
    for (mz, ver, p), tempos in sorted(dados.items(),
                                       key=lambda k: (k[0][0], k[0][1] != "sequencial", k[0][2])):
        q1, med, q3 = quartis(tempos)
        tseq = statistics.median(dados[(mz, "sequencial", 1)])
        s = tseq / med
        linhas.append(dict(matriz=mz, versao=ver, trabalhadores=p, n=len(tempos),
                           mediana_ms=med, q1_ms=q1, q3_ms=q3, iqr_ms=q3 - q1,
                           min_ms=min(tempos), max_ms=max(tempos),
                           aceleracao=s, eficiencia=s / p))
    with open(os.path.join(BASE, "resumo.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(linhas[0].keys()))
        w.writeheader()
        for l in linhas:
            w.writerow({k: (f"{v:.3f}" if isinstance(v, float) else v) for k, v in l.items()})
    return linhas


def serie(linhas, mz, campo):
    par = [l for l in linhas if l["matriz"] == mz and l["versao"] == "paralela"]
    return [l["trabalhadores"] for l in par], [l[campo] for l in par]


def salvar(fig, nome):
    fig.tight_layout()
    fig.savefig(os.path.join(BASE, nome), dpi=150)
    plt.close(fig)


def main():
    dados, dims = carregar()
    linhas = resumir(dados)
    matrizes = [("grande", AZUL), ("pequena", LARANJA)]

    # ---- Tempo: um painel por matriz (escalas muito diferentes) ----
    fig, eixos = plt.subplots(1, 2, figsize=(11, 4.4))
    for ax, (mz, cor) in zip(eixos, matrizes):
        sel = [l for l in linhas if l["matriz"] == mz]
        rot = ["sequencial"] + [f"paralela\np={l['trabalhadores']}" for l in sel if l["versao"] == "paralela"]
        med = [l["mediana_ms"] for l in sel]
        err = [[l["mediana_ms"] - l["q1_ms"] for l in sel], [l["q3_ms"] - l["mediana_ms"] for l in sel]]
        cores = [TEXTO2] + [cor] * (len(sel) - 1)
        ax.bar(rot, med, color=cores, width=0.6, yerr=err, capsize=4,
               error_kw=dict(ecolor=TEXTO, lw=1))
        for x, (v, l) in enumerate(zip(med, sel)):
            ax.annotate(f"{v:.0f}" if v >= 10 else f"{v:.2f}", (x, l["q3_ms"]),
                        xytext=(0, 6), textcoords="offset points",
                        ha="center", fontsize=9, color=TEXTO)
        ax.set_title(f"Matriz {mz} ({dims[mz]})")
        ax.set_ylabel("Tempo (ms, mediana)")
        ax.set_ylim(0, max(l["q3_ms"] for l in sel) * 1.18)
        ax.grid(axis="x", visible=False)
    fig.suptitle("Tempo de execução por configuração (barras de erro = Q1–Q3)",
                 fontweight="bold")
    salvar(fig, "grafico-tempo.png")

    # ---- Aceleração ----
    fig, ax = plt.subplots(figsize=(7.5, 4.6))
    pmax = max(l["trabalhadores"] for l in linhas)
    ps = sorted({l["trabalhadores"] for l in linhas if l["versao"] == "paralela"})
    ax.plot(ps, ps, ls="--", color=TEXTO2, lw=1.5, label="Ideal  S(p) = p")
    for mz, cor in matrizes:
        x, y = serie(linhas, mz, "aceleracao")
        ax.plot(x, y, marker="o", ms=8, lw=2, color=cor,
                markeredgecolor=FUNDO, markeredgewidth=2, label=f"Matriz {mz} ({dims[mz]})")
        ax.annotate(f"{y[-1]:.2f}", (x[-1], y[-1]), xytext=(8, 0),
                    textcoords="offset points", va="center", color=TEXTO, fontsize=9)
    ax.axhline(1, color=TEXTO, lw=0.8)
    nucleos = os.cpu_count() or 2
    ax.axvline(nucleos, color=TEXTO2, lw=1, ls=":")
    ax.annotate(f"{nucleos} núcleos\nna máquina", (nucleos, 0.3), xytext=(6, 0),
                textcoords="offset points", color=TEXTO2, fontsize=9)
    ax.set_xticks(ps)
    ax.set_xlim(0.5, pmax + 1)
    ax.set_ylim(0, max(pmax, 2) + 0.5)
    ax.set_xlabel("Threads (p)")
    ax.set_ylabel("Aceleração S(p) = Tseq / Tpar")
    ax.set_title("Aceleração em relação à versão sequencial")
    ax.legend(frameon=False, loc="upper left")
    salvar(fig, "grafico-aceleracao.png")

    # ---- Eficiência ----
    fig, ax = plt.subplots(figsize=(7.5, 4.6))
    ax.axhline(1, ls="--", color=TEXTO2, lw=1.5, label="Ideal  E(p) = 1")
    for mz, cor in matrizes:
        x, y = serie(linhas, mz, "eficiencia")
        ax.plot(x, y, marker="o", ms=8, lw=2, color=cor,
                markeredgecolor=FUNDO, markeredgewidth=2, label=f"Matriz {mz} ({dims[mz]})")
    ax.set_xticks(ps)
    ax.set_ylim(0, 1.2)
    ax.set_xlabel("Threads (p)")
    ax.set_ylabel("Eficiência E(p) = S(p) / p")
    ax.set_title("Eficiência paralela")
    ax.legend(frameon=False, loc="lower left")
    salvar(fig, "grafico-eficiencia.png")

    print(f"{'matriz':8} {'versao':10} {'p':>2} {'mediana':>10} {'IQR':>8} {'S(p)':>6} {'E(p)':>6}")
    for l in linhas:
        print(f"{l['matriz']:8} {l['versao']:10} {l['trabalhadores']:>2} "
              f"{l['mediana_ms']:>10.3f} {l['iqr_ms']:>8.3f} {l['aceleracao']:>6.2f} {l['eficiencia']:>6.2f}")


if __name__ == "__main__":
    main()
