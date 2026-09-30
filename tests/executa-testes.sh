#!/usr/bin/env bash
# executa-testes.sh - Executa as matrizes obrigatórias e adicionais nas
# versões sequencial e paralela (várias configurações) e compara com o
# valor esperado registrado no cabeçalho de cada arquivo ("esperado: N").
#
# Saídas: results/testes.csv (uma linha por execução) e resumo na tela.
set -u
cd "$(dirname "$0")/.."

SEQ=./conta-objetos-sequencial
PAR=./conta-objetos-paralelo
CSV=results/testes.csv
REPETICOES=20   # repetições para verificar determinismo

# Configurações paralelas: "threads blocos_lin blocos_col"
CONFIGS=("1 1 1" "2 1 2" "2 2 2" "4 2 2" "4 3 3" "3 1 5" "8 4 4" "16 12 12")

objetos() { awk '/^Objetos:/{print $2}'; }

echo "arquivo,esperado,versao,threads,blocos,objetos,situacao" > "$CSV"
falhas=0; total=0

for arq in tests/obrigatorios/*.txt tests/adicionais/*.txt; do
    esperado=$(head -1 "$arq" | sed -n 's/.*esperado: *\([0-9]*\).*/\1/p')
    nome=$(basename "$arq" .txt)

    s=$($SEQ "$arq" | objetos)
    sit=$([ "$s" = "$esperado" ] && echo OK || echo FALHOU)
    [ "$sit" = OK ] || falhas=$((falhas+1)); total=$((total+1))
    echo "$nome,$esperado,sequencial,1,-,$s,$sit" >> "$CSV"
    linha="$(printf '%-24s esperado=%-3s seq=%-3s par=' "$nome" "$esperado" "$s")"

    for cfg in "${CONFIGS[@]}"; do
        set -- $cfg
        p=$($PAR -t "$1" -b "$2" "$3" "$arq" | objetos)
        sit=$([ "$p" = "$esperado" ] && echo OK || echo FALHOU)
        [ "$sit" = OK ] || falhas=$((falhas+1)); total=$((total+1))
        echo "$nome,$esperado,paralela,$1,${2}x${3},$p,$sit" >> "$CSV"
        linha="$linha$p "
    done

    # Determinismo: repete a configuração 4 threads / 3x3 blocos.
    difs=0
    for ((r = 1; r <= REPETICOES; r++)); do
        p=$($PAR -t 4 -b 3 3 "$arq" | objetos)
        [ "$p" = "$esperado" ] || difs=$((difs+1))
    done
    total=$((total+REPETICOES)); falhas=$((falhas+difs))
    echo "$linha| repeticoes divergentes: $difs/$REPETICOES"
done

echo
echo "Execucoes: $total | Falhas: $falhas"
echo "Detalhes em $CSV"
[ "$falhas" -eq 0 ]
