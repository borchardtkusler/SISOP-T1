/*
 * conta-objetos-sequencial.c
 *
 * Versão SEQUENCIAL (referência de correção e de desempenho).
 * Conta os objetos (componentes conexos de células 1) de uma matriz
 * binária usando conectividade 8 e flood fill ITERATIVO.
 *
 * Uso:
 *   ./conta-objetos-sequencial <arquivo>
 *   ./conta-objetos-sequencial -g <linhas> <colunas> <densidade> <semente>
 *
 * Padrão: ANSI C (C89/C90).
 */
#include <stdio.h>
#include <stdlib.h>
#include "comum.h"

/*
 * Preenche por inundação o objeto que contém a célula 'inicio'.
 * Toda célula é marcada como visitada NO MOMENTO EM QUE É EMPILHADA;
 * assim nenhuma célula entra na pilha duas vezes e a pilha nunca
 * passa de linhas*colunas elementos.
 */
static int flood_fill(const Matriz *m, unsigned char *visitado,
                      Pilha *pilha, long inicio)
{
    long idx, lin, col, nl, nc, viz;
    int k;

    pilha->tamanho = 0;
    visitado[inicio] = 1;
    if (pilha_empilhar(pilha, inicio) != 0)
        return -1;

    while (pilha->tamanho > 0) {
        idx = pilha->itens[--pilha->tamanho];
        lin = idx / m->colunas;
        col = idx % m->colunas;

        /* Examina os 8 vizinhos: N, S, L, O e as quatro diagonais. */
        for (k = 0; k < 8; k++) {
            nl = lin + VIZ_DL[k];
            nc = col + VIZ_DC[k];
            if (nl < 0 || nl >= m->linhas || nc < 0 || nc >= m->colunas)
                continue; /* fora da matriz */
            viz = nl * m->colunas + nc;
            if (m->dados[viz] == 1 && !visitado[viz]) {
                visitado[viz] = 1;
                if (pilha_empilhar(pilha, viz) != 0)
                    return -1;
            }
        }
    }
    return 0;
}

/*
 * Percorre a matriz; cada célula 1 ainda não visitada é a "semente"
 * de um novo objeto: incrementa o contador e inunda o objeto inteiro.
 * Retorna o número de objetos ou -1 em caso de erro.
 */
static long contar_objetos_sequencial(const Matriz *m)
{
    long total, i, objetos;
    unsigned char *visitado;
    Pilha pilha;

    total = m->linhas * m->colunas;
    visitado = (unsigned char *)calloc((size_t)total, 1);
    if (visitado == NULL) {
        fprintf(stderr, "Erro: memória insuficiente (visitados).\n");
        return -1;
    }
    if (pilha_iniciar(&pilha, 1024) != 0) {
        fprintf(stderr, "Erro: memória insuficiente (pilha).\n");
        free(visitado);
        return -1;
    }

    objetos = 0;
    for (i = 0; i < total; i++) {
        if (m->dados[i] == 1 && !visitado[i]) {
            objetos++;
            if (flood_fill(m, visitado, &pilha, i) != 0) {
                fprintf(stderr, "Erro: memória insuficiente (pilha).\n");
                objetos = -1;
                break;
            }
        }
    }

    pilha_liberar(&pilha);
    free(visitado);
    return objetos;
}

static void uso(const char *prog)
{
    fprintf(stderr,
            "Uso:\n"
            "  %s <arquivo>\n"
            "  %s -g <linhas> <colunas> <densidade> <semente>\n",
            prog, prog);
}

int main(int argc, char **argv)
{
    Matriz m;
    long objetos;
    double t0, t1;

    if (argc < 2 || matriz_obter_da_linha_de_comando(argc, argv, 1, &m) != 0) {
        uso(argv[0]);
        return EXIT_FAILURE;
    }

    /* Mede apenas a contagem (a leitura/geração da matriz fica fora). */
    t0 = tempo_agora_ms();
    objetos = contar_objetos_sequencial(&m);
    t1 = tempo_agora_ms();

    if (objetos < 0) {
        matriz_liberar(&m);
        return EXIT_FAILURE;
    }

    printf("Versao: sequencial\n");
    printf("Matriz: %ld x %ld\n", m.linhas, m.colunas);
    printf("Objetos: %ld\n", objetos);
    printf("Tempo(ms): %.3f\n", t1 - t0);

    matriz_liberar(&m);
    return EXIT_SUCCESS;
}
