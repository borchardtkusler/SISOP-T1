/*
 * comum.h - Estruturas e funções compartilhadas pelas versões
 *           sequencial e paralela.
 *
 * Tudo que NÃO é o algoritmo de contagem fica aqui: representação
 * da matriz, leitura de arquivo, geração de matriz aleatória
 * reproduzível, pilha dinâmica e medição de tempo. Assim as duas
 * versões recebem exatamente os mesmos dados e são medidas da
 * mesma forma.
 *
 * Padrão: ANSI C (C89/C90) + POSIX.
 */
#ifndef COMUM_H
#define COMUM_H

#include <stddef.h>

/* Matriz binária armazenada em um único vetor contínuo (linha a linha).
 * A célula (i, j) fica em dados[i * colunas + j].
 * Um vetor contínuo é mais amigável à cache do que um vetor de
 * ponteiros para linhas e permite usar o índice linear como
 * identificador único de cada célula. */
typedef struct {
    long linhas;
    long colunas;
    unsigned char *dados; /* 0 = fundo, 1 = primeiro plano */
} Matriz;

/* Pilha dinâmica de índices lineares, usada pelo flood fill
 * iterativo (evita recursão profunda e estouro da pilha do sistema). */
typedef struct {
    long *itens;
    long tamanho;
    long capacidade;
} Pilha;

/* Deslocamentos dos 8 vizinhos (conectividade 8). */
extern const int VIZ_DL[8];
extern const int VIZ_DC[8];

/* ---------- Matriz ---------- */
int  matriz_carregar_arquivo(const char *caminho, Matriz *m);
int  matriz_gerar(Matriz *m, long linhas, long colunas,
                  double densidade, unsigned long semente);
void matriz_liberar(Matriz *m);
int  matriz_obter_da_linha_de_comando(int argc, char **argv, int inicio,
                                      Matriz *m);

/* ---------- Pilha ---------- */
int  pilha_iniciar(Pilha *p, long capacidade_inicial);
int  pilha_empilhar(Pilha *p, long valor);
void pilha_liberar(Pilha *p);

/* ---------- Tempo ---------- */
double tempo_agora_ms(void);

#endif
