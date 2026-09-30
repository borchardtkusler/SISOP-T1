#!/usr/bin/env bash
# mede-desempenho.sh - Mede as versões sequencial e paralela sobre os
# MESMOS dados (matriz gerada com a mesma semente) e grava os dados
# brutos em results/medicoes.csv.
#
# Metodologia:
#  - 1 rodada de aquecimento descartada;
#  - REPETICOES rodadas; em cada rodada todas as configurações são
#    executadas intercaladas (round-robin), para que ruído momentâneo
#    da máquina afete todas as configurações de forma parecida;
#  - o tempo medido é apenas o da contagem (clock_gettime monotônico),
#    sem leitura/geração da matriz.
set -eu
cd "$(dirname "$0")/.."

SEQ=./conta-objetos-sequencial
PAR=./conta-objetos-paralelo
CSV=results/medicoes.csv
REPETICOES=${REPETICOES:-10}
THREADS=${THREADS:-"1 2 4 8"}

# nome linhas colunas densidade semente
MATRIZES=("grande 6000 6000 0.45 42" "pequena 200 200 0.45 42")

campo() { awk -v k="$1" '$1==k":"{print $2}'; }

# Registro do ambiente
{
    echo "data: $(date -Iseconds)"
    uname -srm
    (lscpu 2>/dev/null | grep -E 'Model name|^CPU\(s\)|Core\(s\) per socket|Thread\(s\) per core') \
        || sysctl -n machdep.cpu.brand_string hw.ncpu 2>/dev/null
    (free -h 2>/dev/null | head -2) || sysctl -n hw.memsize 2>/dev/null
    cc --version | head -1
} > results/ambiente.txt

echo "matriz,linhas,colunas,versao,trabalhadores,repeticao,tempo_ms,objetos,resultado_correto" > "$CSV"

for mz in "${MATRIZES[@]}"; do
    set -- $mz
    nome=$1; L=$2; C=$3; D=$4; SEM=$5
    G="-g $L $C $D $SEM"

    ref=$($SEQ $G | campo Objetos)
    echo "Matriz $nome ($L x $C, densidade $D): referencia = $ref objetos"

    # aquecimento (descartado)
    $SEQ $G > /dev/null
    for p in $THREADS; do $PAR -t "$p" $G > /dev/null; done

    for ((r = 1; r <= REPETICOES; r++)); do
        out=$($SEQ $G)
        t=$(echo "$out" | campo "Tempo(ms)"); o=$(echo "$out" | campo Objetos)
        ok=$([ "$o" = "$ref" ] && echo true || echo false)
        echo "$nome,$L,$C,sequencial,1,$r,$t,$o,$ok" >> "$CSV"
        for p in $THREADS; do
            out=$($PAR -t "$p" $G)
            t=$(echo "$out" | campo "Tempo(ms)"); o=$(echo "$out" | campo Objetos)
            ok=$([ "$o" = "$ref" ] && echo true || echo false)
            echo "$nome,$L,$C,paralela,$p,$r,$t,$o,$ok" >> "$CSV"
        done
        echo "  rodada $r/$REPETICOES concluida"
    done
done
echo "Dados brutos em $CSV"
