/*
 * conta-objetos-paralelo.c
 *
 * Versão PARALELA com POSIX Threads (Pthreads).
 *
 * Ideia geral (três fases):
 *
 *   Fase 1 (paralela)  - A matriz é dividida em uma grade de blocos.
 *                        Cada bloco é uma tarefa em uma fila dinâmica.
 *                        As threads retiram blocos da fila e fazem
 *                        flood fill SOMENTE dentro do bloco, rotulando
 *                        cada componente local.
 *
 *   Fase 2 (paralela)  - Cada thread examina as fronteiras inferior e
 *                        direita dos blocos que retirar da fila,
 *                        procurando pares de células 1 vizinhas (inclusive
 *                        na diagonal) com rótulos diferentes. Esses pares
 *                        são equivalências: pertencem ao mesmo objeto
 *                        global. As equivalências são aplicadas em uma
 *                        estrutura union-find compartilhada, protegida
 *                        por mutex.
 *
 *   Fase 3 (sequencial)- objetos = soma dos objetos locais
 *                                 - número de uniões efetivas.
 *
 * Uso:
 *   ./conta-objetos-paralelo [-t threads] [-b blocos_lin blocos_col] [-v]
 *                            <arquivo>
 *   ./conta-objetos-paralelo [-t threads] [-b blocos_lin blocos_col] [-v]
 *                            -g <linhas> <colunas> <densidade> <semente>
 *
 * Padrão: ANSI C (C89/C90) + Pthreads.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "comum.h"

#define MAX_THREADS 1024
#define LIMITE_VERBOSO 40 /* só imprime mapas de rótulos até 40x40 */

/* Região retangular [l0, l1) x [c0, c1) da matriz. */
typedef struct {
    long l0, l1, c0, c1;
} Bloco;

/* Estado compartilhado por todas as threads. */
typedef struct {
    const Matriz *m;

    /* rotulo[i] = 0 se a célula é fundo; caso contrário, (índice linear
     * da semente do componente local) + 1. Como cada célula tem um
     * índice linear único, os rótulos criados por threads diferentes
     * NUNCA colidem, sem nenhuma coordenação entre elas. */
    int *rotulo;

    /* Union-find: pai[r] é o representante provisório do rótulo r.
     * Só as posições correspondentes a rótulos existentes são usadas. */
    int *pai;

    Bloco *blocos;
    int num_blocos;
    int blocos_lin, blocos_col;

    /* Resultado local de cada bloco (fase 1). Cada posição é escrita
     * por UMA única thread (a que processou o bloco): sem corrida. */
    long *objetos_locais;

    /* Fila dinâmica de tarefas: índice do próximo bloco a entregar. */
    int proxima_tarefa;
    int fase;
    int erro;
    pthread_mutex_t mutex_fila;

    /* Consolidação: protege 'pai' e 'unioes'. */
    pthread_mutex_t mutex_uf;
    long unioes;

    int verboso;
} Contexto;

typedef struct {
    Contexto *ctx;
    int id;
} ArgThread;

/* ------------------------------------------------------------------ */
/* Union-find (sempre chamado com mutex_uf travado)                    */
/* ------------------------------------------------------------------ */

/* Busca o representante com compressão de caminho por "halving". */
static int uf_encontrar(int *pai, int x)
{
    while (pai[x] != x) {
        pai[x] = pai[pai[x]];
        x = pai[x];
    }
    return x;
}

/* Une os conjuntos de a e b. O representante é sempre o MENOR rótulo,
 * então a estrutura final não depende da ordem das uniões.
 * Retorna 1 se dois conjuntos distintos foram unidos, 0 caso contrário. */
static int uf_unir(int *pai, int a, int b)
{
    int ra = uf_encontrar(pai, a);
    int rb = uf_encontrar(pai, b);
    if (ra == rb)
        return 0;
    if (ra < rb)
        pai[rb] = ra;
    else
        pai[ra] = rb;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Fila dinâmica de tarefas                                            */
/* ------------------------------------------------------------------ */

/* Retira o próximo bloco da fila. Retorna -1 se a fila acabou ou se
 * alguma thread sinalizou erro. A região crítica é mínima: só a
 * leitura e o incremento do contador. */
static int pegar_tarefa(Contexto *ctx)
{
    int tarefa;
    int rc;

    rc = pthread_mutex_lock(&ctx->mutex_fila);
    if (rc != 0) {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(rc));
        return -1;
    }
    if (ctx->erro || ctx->proxima_tarefa >= ctx->num_blocos)
        tarefa = -1;
    else
        tarefa = ctx->proxima_tarefa++;
    rc = pthread_mutex_unlock(&ctx->mutex_fila);
    if (rc != 0) {
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(rc));
        return -1;
    }
    return tarefa;
}

static void sinalizar_erro(Contexto *ctx)
{
    if (pthread_mutex_lock(&ctx->mutex_fila) == 0) {
        ctx->erro = 1;
        pthread_mutex_unlock(&ctx->mutex_fila);
    }
}

/* ------------------------------------------------------------------ */
/* Fase 1: rotulação local de um bloco                                 */
/* ------------------------------------------------------------------ */

static int rotular_bloco(Contexto *ctx, int b, Pilha *pilha)
{
    const Matriz *m = ctx->m;
    const Bloco *bl = &ctx->blocos[b];
    long C = m->colunas;
    long i, j, idx, lin, col, nl, nc, viz;
    long locais = 0;
    int k, rot;

    /* Zera os rótulos do próprio bloco (cada thread inicializa só a
     * sua parte, o que também é feito em paralelo). */
    for (i = bl->l0; i < bl->l1; i++)
        for (j = bl->c0; j < bl->c1; j++)
            ctx->rotulo[i * C + j] = 0;

    for (i = bl->l0; i < bl->l1; i++) {
        for (j = bl->c0; j < bl->c1; j++) {
            idx = i * C + j;
            if (m->dados[idx] != 1 || ctx->rotulo[idx] != 0)
                continue;

            /* Nova semente: rótulo = índice linear + 1 (único global). */
            rot = (int)(idx + 1);
            ctx->pai[rot] = rot; /* cada semente é seu próprio conjunto */
            locais++;

            ctx->rotulo[idx] = rot;
            pilha->tamanho = 0;
            if (pilha_empilhar(pilha, idx) != 0)
                return -1;

            while (pilha->tamanho > 0) {
                idx = pilha->itens[--pilha->tamanho];
                lin = idx / C;
                col = idx % C;
                for (k = 0; k < 8; k++) {
                    nl = lin + VIZ_DL[k];
                    nc = col + VIZ_DC[k];
                    /* Restrito ao BLOCO, não à matriz inteira: o que
                     * está além da fronteira é tratado na fase 2. */
                    if (nl < bl->l0 || nl >= bl->l1 ||
                        nc < bl->c0 || nc >= bl->c1)
                        continue;
                    viz = nl * C + nc;
                    if (m->dados[viz] == 1 && ctx->rotulo[viz] == 0) {
                        ctx->rotulo[viz] = rot;
                        if (pilha_empilhar(pilha, viz) != 0)
                            return -1;
                    }
                }
            }
        }
    }
    ctx->objetos_locais[b] = locais;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Fase 2: análise das fronteiras de um bloco                          */
/* ------------------------------------------------------------------ */

/* Registra o par (a, b) no buffer local da thread, ignorando células
 * de fundo (rótulo 0), pares iguais e repetições consecutivas (muito
 * comuns ao longo de uma fronteira atravessada pelo mesmo objeto). */
static int registrar_par(Pilha *pares, int a, int b)
{
    int t;
    if (a == 0 || b == 0 || a == b)
        return 0;
    if (a > b) { t = a; a = b; b = t; }
    if (pares->tamanho >= 2 &&
        pares->itens[pares->tamanho - 2] == a &&
        pares->itens[pares->tamanho - 1] == b)
        return 0;
    if (pilha_empilhar(pares, a) != 0) return -1;
    if (pilha_empilhar(pares, b) != 0) return -1;
    return 0;
}

/*
 * Cada bloco é responsável pela sua fronteira INFERIOR e pela sua
 * fronteira DIREITA. Assim toda fronteira da grade é examinada
 * exatamente uma vez.
 *
 *  - Fronteira inferior: cada célula (r, c) da última linha do bloco
 *    é comparada com (r+1, c-1), (r+1, c) e (r+1, c+1).
 *    Isso cobre as ligações verticais e as duas diagonais, inclusive
 *    as que cruzam o canto onde quatro blocos se encontram (c-1 ou
 *    c+1 podem pertencer ao bloco vizinho da diagonal).
 *
 *  - Fronteira direita: cada célula (r, c) da última coluna do bloco
 *    é comparada com (r-1, c+1), (r, c+1) e (r+1, c+1).
 */
static int analisar_fronteiras(Contexto *ctx, int b, Pilha *pares)
{
    const Matriz *m = ctx->m;
    const Bloco *bl = &ctx->blocos[b];
    long L = m->linhas, C = m->colunas;
    long r, c, nr, nc;
    int a, d, rc;
    long i;

    pares->tamanho = 0;

    if (bl->l1 < L) { /* existe bloco abaixo */
        r = bl->l1 - 1;
        for (c = bl->c0; c < bl->c1; c++) {
            a = ctx->rotulo[r * C + c];
            if (a == 0) continue;
            for (d = -1; d <= 1; d++) {
                nc = c + d;
                if (nc < 0 || nc >= C) continue;
                if (registrar_par(pares, a, ctx->rotulo[(r + 1) * C + nc]) != 0)
                    return -1;
            }
        }
    }

    if (bl->c1 < C) { /* existe bloco à direita */
        c = bl->c1 - 1;
        for (r = bl->l0; r < bl->l1; r++) {
            a = ctx->rotulo[r * C + c];
            if (a == 0) continue;
            for (d = -1; d <= 1; d++) {
                nr = r + d;
                if (nr < 0 || nr >= L) continue;
                if (registrar_par(pares, a, ctx->rotulo[nr * C + c + 1]) != 0)
                    return -1;
            }
        }
    }

    if (pares->tamanho == 0)
        return 0;

    /* A varredura acima só LÊ 'rotulo' (que não muda mais na fase 2),
     * então roda em paralelo sem trava. Somente a aplicação das
     * equivalências no union-find compartilhado é região crítica.
     * Travamos UMA vez por bloco (e não uma vez por par) para reduzir
     * a contenção. */
    rc = pthread_mutex_lock(&ctx->mutex_uf);
    if (rc != 0) {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(rc));
        return -1;
    }
    for (i = 0; i < pares->tamanho; i += 2) {
        int x = (int)pares->itens[i], y = (int)pares->itens[i + 1];
        if (uf_unir(ctx->pai, x, y)) {
            ctx->unioes++;
            if (ctx->verboso)
                printf("  [bloco %d] une rotulo %d com rotulo %d\n", b, x, y);
        }
    }
    rc = pthread_mutex_unlock(&ctx->mutex_uf);
    if (rc != 0) {
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(rc));
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Função executada por cada thread                                    */
/* ------------------------------------------------------------------ */

static void *trabalhador(void *arg)
{
    ArgThread *a = (ArgThread *)arg;
    Contexto *ctx = a->ctx;
    Pilha pilha; /* privada: cada thread tem a sua, sem compartilhamento */
    int tarefa;

    if (pilha_iniciar(&pilha, 1024) != 0) {
        fprintf(stderr, "Thread %d: memória insuficiente.\n", a->id);
        sinalizar_erro(ctx);
        return NULL;
    }

    while ((tarefa = pegar_tarefa(ctx)) >= 0) {
        int falhou;
        if (ctx->fase == 1)
            falhou = rotular_bloco(ctx, tarefa, &pilha);
        else
            falhou = analisar_fronteiras(ctx, tarefa, &pilha);
        if (falhou) {
            fprintf(stderr, "Thread %d: falha no bloco %d.\n", a->id, tarefa);
            sinalizar_erro(ctx);
            break;
        }
    }

    pilha_liberar(&pilha);
    return NULL;
}

/* Cria 'n' threads para executar a fase indicada e espera todas.
 * O pthread_join funciona como barreira entre as fases: a fase 2 só
 * começa depois que TODOS os blocos foram rotulados.
 * (pthread_barrier_t não existe no macOS, por isso não foi usado.) */
static int executar_fase(Contexto *ctx, int fase, int n,
                         pthread_t *threads, ArgThread *args)
{
    int i, criadas, rc;

    ctx->fase = fase;
    ctx->proxima_tarefa = 0;

    criadas = 0;
    for (i = 0; i < n; i++) {
        args[i].ctx = ctx;
        args[i].id = i;
        rc = pthread_create(&threads[i], NULL, trabalhador, &args[i]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(rc));
            sinalizar_erro(ctx);
            break;
        }
        criadas++;
    }

    /* Espera as que foram criadas, mesmo se houve erro (sem vazamento
     * de threads). */
    for (i = 0; i < criadas; i++) {
        rc = pthread_join(threads[i], NULL);
        if (rc != 0) {
            fprintf(stderr, "pthread_join: %s\n", strerror(rc));
            ctx->erro = 1;
        }
    }
    return ctx->erro ? -1 : 0;
}

/* ------------------------------------------------------------------ */
/* Modo verboso: mapas de rótulos para matrizes pequenas               */
/* ------------------------------------------------------------------ */

static void imprimir_rotulos(const Contexto *ctx, int consolidado)
{
    const Matriz *m = ctx->m;
    long i, j;
    int r;
    for (i = 0; i < m->linhas; i++) {
        printf("  ");
        for (j = 0; j < m->colunas; j++) {
            r = ctx->rotulo[i * m->colunas + j];
            if (r != 0 && consolidado)
                r = uf_encontrar(ctx->pai, r);
            if (r == 0)
                printf("   .");
            else
                printf("%4d", r);
        }
        printf("\n");
    }
}

/* ------------------------------------------------------------------ */
/* Contagem paralela                                                   */
/* ------------------------------------------------------------------ */

static void montar_blocos(Contexto *ctx)
{
    const Matriz *m = ctx->m;
    int i, j, b = 0;
    for (i = 0; i < ctx->blocos_lin; i++) {
        for (j = 0; j < ctx->blocos_col; j++) {
            /* Divisão com sobras distribuídas: os blocos diferem em no
             * máximo uma linha/coluna. */
            ctx->blocos[b].l0 = (long)i * m->linhas / ctx->blocos_lin;
            ctx->blocos[b].l1 = (long)(i + 1) * m->linhas / ctx->blocos_lin;
            ctx->blocos[b].c0 = (long)j * m->colunas / ctx->blocos_col;
            ctx->blocos[b].c1 = (long)(j + 1) * m->colunas / ctx->blocos_col;
            b++;
        }
    }
}

static long contar_objetos_paralelo(const Matriz *m, int num_threads,
                                    int blocos_lin, int blocos_col,
                                    int verboso)
{
    Contexto ctx;
    pthread_t *threads = NULL;
    ArgThread *args = NULL;
    long total, soma_locais, resultado = -1;
    int b, rc;
    int mutex_fila_ok = 0, mutex_uf_ok = 0;

    memset(&ctx, 0, sizeof(ctx));
    ctx.m = m;
    ctx.blocos_lin = blocos_lin;
    ctx.blocos_col = blocos_col;
    ctx.num_blocos = blocos_lin * blocos_col;
    ctx.verboso = verboso;
    total = m->linhas * m->colunas;

    /* 'rotulo' e 'pai' usam malloc (e não calloc) porque cada thread
     * inicializa apenas as posições que utiliza. */
    ctx.rotulo = (int *)malloc((size_t)total * sizeof(int));
    ctx.pai = (int *)malloc((size_t)(total + 1) * sizeof(int));
    ctx.blocos = (Bloco *)malloc((size_t)ctx.num_blocos * sizeof(Bloco));
    ctx.objetos_locais = (long *)malloc((size_t)ctx.num_blocos * sizeof(long));
    threads = (pthread_t *)malloc((size_t)num_threads * sizeof(pthread_t));
    args = (ArgThread *)malloc((size_t)num_threads * sizeof(ArgThread));
    if (!ctx.rotulo || !ctx.pai || !ctx.blocos || !ctx.objetos_locais ||
        !threads || !args) {
        fprintf(stderr, "Erro: memória insuficiente.\n");
        goto fim;
    }

    rc = pthread_mutex_init(&ctx.mutex_fila, NULL);
    if (rc != 0) {
        fprintf(stderr, "pthread_mutex_init: %s\n", strerror(rc));
        goto fim;
    }
    mutex_fila_ok = 1;
    rc = pthread_mutex_init(&ctx.mutex_uf, NULL);
    if (rc != 0) {
        fprintf(stderr, "pthread_mutex_init: %s\n", strerror(rc));
        goto fim;
    }
    mutex_uf_ok = 1;

    montar_blocos(&ctx);

    if (verboso) {
        printf("Blocos (linhas [l0,l1) x colunas [c0,c1)):\n");
        for (b = 0; b < ctx.num_blocos; b++)
            printf("  bloco %d: [%ld,%ld) x [%ld,%ld)\n", b,
                   ctx.blocos[b].l0, ctx.blocos[b].l1,
                   ctx.blocos[b].c0, ctx.blocos[b].c1);
    }

    /* ---------- Fase 1: rotulação local (paralela) ---------- */
    if (executar_fase(&ctx, 1, num_threads, threads, args) != 0)
        goto fim;

    soma_locais = 0;
    for (b = 0; b < ctx.num_blocos; b++)
        soma_locais += ctx.objetos_locais[b];

    if (verboso && m->linhas <= LIMITE_VERBOSO && m->colunas <= LIMITE_VERBOSO) {
        printf("\nFase 1 - rotulos locais (rotulo = indice da semente + 1):\n");
        imprimir_rotulos(&ctx, 0);
        printf("Objetos locais por bloco:");
        for (b = 0; b < ctx.num_blocos; b++)
            printf(" %ld", ctx.objetos_locais[b]);
        printf("  (soma = %ld)\n", soma_locais);
        printf("\nFase 2 - unioes nas fronteiras (a ordem varia entre execucoes):\n");
    }

    /* ---------- Fase 2: fronteiras + union-find (paralela) ---------- */
    if (executar_fase(&ctx, 2, num_threads, threads, args) != 0)
        goto fim;

    /* ---------- Fase 3: contagem global (sequencial, O(1)) ----------
     * Cada união efetiva junta dois conjuntos em um, então reduz a
     * contagem em exatamente 1. */
    resultado = soma_locais - ctx.unioes;

    if (verboso && m->linhas <= LIMITE_VERBOSO && m->colunas <= LIMITE_VERBOSO) {
        printf("\nFase 3 - rotulos globais apos a consolidacao:\n");
        imprimir_rotulos(&ctx, 1);
        printf("Objetos = %ld (locais) - %ld (unioes) = %ld\n\n",
               soma_locais, ctx.unioes, resultado);
    }

fim:
    if (mutex_uf_ok && (rc = pthread_mutex_destroy(&ctx.mutex_uf)) != 0)
        fprintf(stderr, "pthread_mutex_destroy: %s\n", strerror(rc));
    if (mutex_fila_ok && (rc = pthread_mutex_destroy(&ctx.mutex_fila)) != 0)
        fprintf(stderr, "pthread_mutex_destroy: %s\n", strerror(rc));
    free(args);
    free(threads);
    free(ctx.objetos_locais);
    free(ctx.blocos);
    free(ctx.pai);
    free(ctx.rotulo);
    return resultado;
}

/* ------------------------------------------------------------------ */

static void uso(const char *prog)
{
    fprintf(stderr,
            "Uso:\n"
            "  %s [-t threads] [-b blocos_lin blocos_col] [-v] <arquivo>\n"
            "  %s [-t threads] [-b blocos_lin blocos_col] [-v] "
            "-g <linhas> <colunas> <densidade> <semente>\n"
            "Padrao: 2 threads; grade de (2*threads) x 2 blocos.\n",
            prog, prog);
}

static int ler_inteiro(const char *s, int minimo, int maximo, int *saida)
{
    char *fim;
    long v = strtol(s, &fim, 10);
    if (*s == '\0' || *fim != '\0' || v < minimo || v > maximo)
        return -1;
    *saida = (int)v;
    return 0;
}

int main(int argc, char **argv)
{
    Matriz m;
    int num_threads = 2, blocos_lin = -1, blocos_col = -1, verboso = 0;
    int i = 1;
    long objetos;
    double t0, t1;

    while (i < argc && argv[i][0] == '-' && strcmp(argv[i], "-g") != 0) {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            if (ler_inteiro(argv[i + 1], 1, MAX_THREADS, &num_threads) != 0) {
                fprintf(stderr, "Erro: -t deve estar entre 1 e %d.\n",
                        MAX_THREADS);
                return EXIT_FAILURE;
            }
            i += 2;
        } else if (strcmp(argv[i], "-b") == 0 && i + 2 < argc) {
            if (ler_inteiro(argv[i + 1], 1, 100000, &blocos_lin) != 0 ||
                ler_inteiro(argv[i + 2], 1, 100000, &blocos_col) != 0) {
                fprintf(stderr, "Erro: -b exige dois inteiros positivos.\n");
                return EXIT_FAILURE;
            }
            i += 3;
        } else if (strcmp(argv[i], "-v") == 0) {
            verboso = 1;
            i++;
        } else {
            uso(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (i >= argc || matriz_obter_da_linha_de_comando(argc, argv, i, &m) != 0) {
        uso(argv[0]);
        return EXIT_FAILURE;
    }

    if (blocos_lin < 0) {
        blocos_lin = 2 * num_threads;
        blocos_col = 2;
    }
    /* Não faz sentido ter mais faixas do que linhas/colunas. */
    if (blocos_lin > m.linhas) blocos_lin = (int)m.linhas;
    if (blocos_col > m.colunas) blocos_col = (int)m.colunas;

    t0 = tempo_agora_ms();
    objetos = contar_objetos_paralelo(&m, num_threads, blocos_lin,
                                      blocos_col, verboso);
    t1 = tempo_agora_ms();

    if (objetos < 0) {
        matriz_liberar(&m);
        return EXIT_FAILURE;
    }

    printf("Versao: paralela (pthreads)\n");
    printf("Matriz: %ld x %ld\n", m.linhas, m.colunas);
    printf("Threads: %d\n", num_threads);
    printf("Blocos: %d x %d\n", blocos_lin, blocos_col);
    printf("Objetos: %ld\n", objetos);
    printf("Tempo(ms): %.3f\n", t1 - t0);

    matriz_liberar(&m);
    return EXIT_SUCCESS;
}
