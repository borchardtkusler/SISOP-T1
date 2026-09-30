/*
 * comum.c - Implementação das rotinas compartilhadas.
 */
#define _POSIX_C_SOURCE 199309L /* necessário para clock_gettime em C89 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include "comum.h"

/* Ordem: NO, N, NE, O, L, SO, S, SE */
const int VIZ_DL[8] = { -1, -1, -1,  0, 0,  1, 1, 1 };
const int VIZ_DC[8] = { -1,  0,  1, -1, 1, -1, 0, 1 };

/* Tamanho máximo aceito: o maior índice linear precisa caber em
 * 'int', pois os rótulos da versão paralela são do tipo int. */
static int dimensoes_validas(long linhas, long colunas)
{
    if (linhas <= 0 || colunas <= 0) {
        fprintf(stderr, "Erro: dimensões devem ser positivas.\n");
        return 0;
    }
    if (linhas > (long)(INT_MAX - 1) / colunas) {
        fprintf(stderr, "Erro: matriz grande demais (%ld x %ld).\n",
                linhas, colunas);
        return 0;
    }
    return 1;
}

/*
 * Formato do arquivo:
 *   primeira linha: <linhas> <colunas>
 *   depois: linhas*colunas valores 0 ou 1.
 * Espaços e quebras de linha entre os valores são ignorados, então
 * tanto "1 0 1" quanto "101" são aceitos. Linhas iniciadas por '#'
 * são comentários.
 */
int matriz_carregar_arquivo(const char *caminho, Matriz *m)
{
    FILE *f;
    long total, lidos;
    int c;

    m->dados = NULL;
    f = fopen(caminho, "r");
    if (f == NULL) {
        perror(caminho);
        return -1;
    }

    /* Ignora comentários antes do cabeçalho. */
    while ((c = fgetc(f)) == '#') {
        while ((c = fgetc(f)) != EOF && c != '\n')
            ;
    }
    if (c != EOF)
        ungetc(c, f);

    if (fscanf(f, "%ld %ld", &m->linhas, &m->colunas) != 2) {
        fprintf(stderr, "Erro: cabeçalho inválido em %s.\n", caminho);
        fclose(f);
        return -1;
    }
    if (!dimensoes_validas(m->linhas, m->colunas)) {
        fclose(f);
        return -1;
    }

    total = m->linhas * m->colunas;
    m->dados = (unsigned char *)malloc((size_t)total);
    if (m->dados == NULL) {
        fprintf(stderr, "Erro: memória insuficiente para a matriz.\n");
        fclose(f);
        return -1;
    }

    lidos = 0;
    while (lidos < total && (c = fgetc(f)) != EOF) {
        if (c == '0' || c == '1') {
            m->dados[lidos++] = (unsigned char)(c - '0');
        } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r'
                   || c == ',') {
            continue;
        } else {
            fprintf(stderr, "Erro: caractere inválido '%c' em %s.\n",
                    c, caminho);
            free(m->dados);
            m->dados = NULL;
            fclose(f);
            return -1;
        }
    }
    fclose(f);

    if (lidos != total) {
        fprintf(stderr, "Erro: esperados %ld valores, lidos %ld em %s.\n",
                total, lidos, caminho);
        free(m->dados);
        m->dados = NULL;
        return -1;
    }
    return 0;
}

/*
 * Gera uma matriz pseudoaleatória reproduzível.
 * Usa um gerador xorshift de 32 bits próprio (e não rand()) para que a
 * mesma semente gere exatamente a mesma matriz em qualquer sistema,
 * garantindo que as versões sequencial e paralela recebam dados iguais.
 */
int matriz_gerar(Matriz *m, long linhas, long colunas,
                 double densidade, unsigned long semente)
{
    long i, total;
    unsigned long x, limiar;

    m->dados = NULL;
    if (!dimensoes_validas(linhas, colunas))
        return -1;
    if (densidade < 0.0 || densidade > 1.0) {
        fprintf(stderr, "Erro: densidade deve estar entre 0 e 1.\n");
        return -1;
    }

    m->linhas = linhas;
    m->colunas = colunas;
    total = linhas * colunas;
    m->dados = (unsigned char *)malloc((size_t)total);
    if (m->dados == NULL) {
        fprintf(stderr, "Erro: memória insuficiente para a matriz.\n");
        return -1;
    }

    x = (semente & 0xFFFFFFFFUL);
    if (x == 0)
        x = 2463534242UL;
    limiar = (unsigned long)(densidade * 4294967295.0);

    for (i = 0; i < total; i++) {
        x ^= (x << 13) & 0xFFFFFFFFUL;
        x ^= (x >> 17);
        x ^= (x << 5) & 0xFFFFFFFFUL;
        m->dados[i] = (unsigned char)(x < limiar ? 1 : 0);
    }
    return 0;
}

void matriz_liberar(Matriz *m)
{
    free(m->dados);
    m->dados = NULL;
}

/*
 * Interpreta a fonte da matriz a partir de argv[inicio]:
 *   <arquivo>
 *   -g <linhas> <colunas> <densidade> <semente>
 */
int matriz_obter_da_linha_de_comando(int argc, char **argv, int inicio,
                                     Matriz *m)
{
    char *fim;
    long l, c;
    double d;
    unsigned long s;

    if (inicio >= argc)
        return -1;

    if (strcmp(argv[inicio], "-g") == 0) {
        if (inicio + 4 >= argc) {
            fprintf(stderr, "Erro: -g exige <linhas> <colunas> "
                            "<densidade> <semente>.\n");
            return -1;
        }
        l = strtol(argv[inicio + 1], &fim, 10);
        if (*fim != '\0') return -1;
        c = strtol(argv[inicio + 2], &fim, 10);
        if (*fim != '\0') return -1;
        d = strtod(argv[inicio + 3], &fim);
        if (*fim != '\0') return -1;
        s = strtoul(argv[inicio + 4], &fim, 10);
        if (*fim != '\0') return -1;
        return matriz_gerar(m, l, c, d, s);
    }
    return matriz_carregar_arquivo(argv[inicio], m);
}

/* ---------- Pilha dinâmica ---------- */

int pilha_iniciar(Pilha *p, long capacidade_inicial)
{
    if (capacidade_inicial < 16)
        capacidade_inicial = 16;
    p->itens = (long *)malloc((size_t)capacidade_inicial * sizeof(long));
    if (p->itens == NULL)
        return -1;
    p->tamanho = 0;
    p->capacidade = capacidade_inicial;
    return 0;
}

/* Empilha; dobra a capacidade quando necessário (custo amortizado O(1)). */
int pilha_empilhar(Pilha *p, long valor)
{
    long *novo;
    if (p->tamanho == p->capacidade) {
        novo = (long *)realloc(p->itens,
                               (size_t)(p->capacidade * 2) * sizeof(long));
        if (novo == NULL)
            return -1;
        p->itens = novo;
        p->capacidade *= 2;
    }
    p->itens[p->tamanho++] = valor;
    return 0;
}

void pilha_liberar(Pilha *p)
{
    free(p->itens);
    p->itens = NULL;
    p->tamanho = p->capacidade = 0;
}

/* ---------- Tempo ---------- */

/* Relógio monotônico: não sofre ajustes de data/hora do sistema. */
double tempo_agora_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        perror("clock_gettime");
        return 0.0;
    }
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}
