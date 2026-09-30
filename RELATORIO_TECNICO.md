# Relatório técnico - Contagem paralela de objetos em uma matriz binária

> **Disciplina:** Sistemas Operacionais - 2026/II  
> **Professor:** Prof. Filipo Novo Mór  
> **Instituição:** Pontifícia Universidade Católica do Rio Grande do Sul - Escola Politécnica  
> **Repositório:** [PREENCHER URL pública do repositório](https://github.com/USUARIO/REPOSITORIO)  
> **Versão do relatório:** 1.0  
> **Data:** [PREENCHER DD/MM/AAAA]

## Identificação

| Campo | Informação |
|---|---|
| Integrante 1 | [PREENCHER nome completo] |
| Matrícula do integrante 1 | [PREENCHER] |
| Integrante 2 | [PREENCHER nome completo ou `Não se aplica`] |
| Matrícula do integrante 2 | [PREENCHER ou `Não se aplica`] |
| Modalidade | [Individual / dupla] |
| Turma | [PREENCHER] |
| Estratégia paralela | Pthreads |
| Plataforma testada | Linux (x86_64) |
| Commit avaliado | [`PREENCHER HASH_DO_COMMIT`] |

## Resumo

Este trabalho conta os objetos de uma matriz binária, definidos como componentes conexos de células `1` sob conectividade 8. A versão sequencial percorre a matriz e, a cada célula `1` ainda não visitada, executa um *flood fill* iterativo com pilha explícita, evitando recursão profunda. A versão paralela usa Pthreads: a matriz é dividida em uma grade de blocos, distribuídos às threads por uma fila dinâmica protegida por mutex. Na primeira fase, cada thread rotula os componentes locais do bloco com um rótulo globalmente único (índice linear da semente + 1), sem coordenação. Na segunda fase, as threads examinam as fronteiras inferior e direita de cada bloco, incluindo diagonais, e registram as equivalências em uma estrutura *union-find* compartilhada, protegida por um segundo mutex. O total é a soma dos objetos locais menos o número de uniões efetivas. As duas versões produziram os resultados esperados nas cinco matrizes obrigatórias, em nove casos adicionais, em oito configurações paralelas e em 500 matrizes aleatórias comparadas com um oráculo independente (scipy). Em uma matriz 6000 × 6000, com 2 threads em máquina de 2 núcleos, a aceleração foi 1,83 (eficiência 0,92).

**Palavras-chave:** sistemas operacionais; paralelismo; processos; threads; conectividade 8; flood fill; componentes conexos.

## 1. Visão geral do problema

O programa recebe uma matriz binária na qual `0` representa o fundo e `1` representa o primeiro plano. Um objeto corresponde a um componente de células de valor `1` conectadas horizontalmente, verticalmente ou diagonalmente, conforme a **conectividade 8**.

O projeto contém duas implementações funcionalmente equivalentes:

1. uma versão sequencial, usada como referência de correção e de desempenho;
2. uma versão paralela baseada em **POSIX Threads (Pthreads)**.

### 1.1 Objetivos da implementação

- Contar corretamente os objetos com conectividade 8.
- Distribuir trabalho efetivo entre pelo menos duas unidades de execução.
- Reconhecer e unificar objetos que atravessam as divisões da matriz.
- Produzir resultados determinísticos e idênticos nas versões sequencial e paralela.
- Evitar condições de corrida, deadlocks, atualizações perdidas e contagens duplicadas.
- Avaliar correção, sobrecarga, escalabilidade, aceleração e eficiência.

### 1.2 Requisitos atendidos

| Requisito | Como foi atendido | Evidência no repositório |
|---|---|---|
| ANSI C C89/C90 | Todo o código compila com `-std=c89 -Wall -Wextra -pedantic -Werror` sem avisos: sem comentários `//`, declarações no início dos blocos, sem `long long`. | [`src/`](src/), [`results/verificacoes-qualidade.log`](results/verificacoes-qualidade.log) |
| Conectividade 8 | Vetores de deslocamento `VIZ_DL`/`VIZ_DC` com os 8 vizinhos; nas fronteiras, comparação com 3 vizinhos do outro lado (inclusive diagonais). | `src/comum.c` (linhas 14-15); `analisar_fronteiras` em `src/conta-objetos-paralelo.c` |
| Versão sequencial | Flood fill iterativo com pilha dinâmica e vetor de visitados. | [`src/conta-objetos-sequencial.c`](src/conta-objetos-sequencial.c) |
| Versão paralela | Pthreads em 3 fases (rotulação local, fronteiras, contagem). | [`src/conta-objetos-paralelo.c`](src/conta-objetos-paralelo.c) |
| Duas ou mais unidades concorrentes | `pthread_create` cria `N` threads por fase; ambas as fases fazem cálculo real. | `executar_fase` (linha 358); `make testes` usa 1 a 16 threads |
| Quantidade configurável de trabalhadores | Opção `-t N` (1 a 1024); grade de blocos configurável com `-b L C`. | `main` em `src/conta-objetos-paralelo.c` |
| Consolidação entre regiões | Union-find com rótulos globais únicos; objetos = locais − uniões. | `uf_unir` (linha 105), `analisar_fronteiras` (linha 252) |
| Tratamento horizontal, vertical e diagonal | Fronteira inferior compara com `(r+1, c-1..c+1)`; direita com `(r-1..r+1, c+1)`. | Exemplos 3 e 5, testes A3, A5, A6 |
| Verificação das chamadas POSIX | Retornos de `pthread_create`, `pthread_join`, `pthread_mutex_*`, `clock_gettime`, `fopen`, `malloc` verificados. | Seção 10.1 |
| Liberação dos recursos | Todas as threads recebem `join`; mutexes destruídos; memória liberada em um único ponto de saída (`fim:`). | `contar_objetos_paralelo` (linhas 526-537); Valgrind sem vazamentos |
| Compilação reproduzível | `Makefile` com alvos `all`, `testes`, `validacao`, `desempenho`, `graficos`, `clean`. | [`Makefile`](Makefile) |

## 2. Organização do repositório

```text
.
├── README.md
├── RELATORIO_TECNICO.md
├── Makefile
├── src/
│   ├── comum.h
│   ├── comum.c
│   ├── conta-objetos-sequencial.c
│   └── conta-objetos-paralelo.c
├── tests/
│   ├── obrigatorios/          (ex1.txt ... ex5.txt)
│   ├── adicionais/            (a1 ... a10)
│   ├── executa-testes.sh
│   └── validacao-aleatoria.py
├── results/
│   ├── medicoes.csv
│   ├── resumo.csv
│   ├── testes.csv
│   ├── grafico-tempo.png
│   ├── grafico-aceleracao.png
│   ├── grafico-eficiencia.png
│   ├── rastreio-ex3.txt, rastreio-ex4.txt, rastreio-ex5.txt
│   ├── testes.log, validacao-aleatoria.log, verificacoes-qualidade.log
│   ├── ambiente.txt
│   ├── mede-desempenho.sh
│   └── gera-graficos.py
└── slides/
    └── apresentacao.pdf
```

| Caminho | Finalidade |
|---|---|
| `src/comum.h`, `src/comum.c` | Código compartilhado: matriz, leitura de arquivo, gerador reproduzível, pilha dinâmica e medição de tempo. |
| `src/conta-objetos-sequencial.c` | Implementação sequencial de referência. |
| `src/conta-objetos-paralelo.c` | Implementação paralela com Pthreads. |
| `tests/obrigatorios/` | Cinco matrizes obrigatórias do enunciado (com o valor esperado no cabeçalho). |
| `tests/adicionais/` | Casos adicionais criados pelo grupo. |
| `tests/executa-testes.sh` | Executa todas as matrizes nas duas versões e em 8 configurações paralelas. |
| `tests/validacao-aleatoria.py` | Validação cruzada com o oráculo `scipy.ndimage.label`. |
| `results/medicoes.csv` | Dados brutos das medições de desempenho. |
| `results/resumo.csv` | Mediana, quartis, aceleração e eficiência calculados a partir dos dados brutos. |
| `results/*.png` | Gráficos gerados por `gera-graficos.py` a partir de `medicoes.csv`. |
| `results/rastreio-*.txt` | Saídas do modo `-v` mostrando a consolidação passo a passo. |
| `slides/apresentacao.pdf` | Slides utilizados na apresentação. |

## 3. Ambiente de desenvolvimento e execução

### 3.1 Hardware e software

| Item | Especificação |
|---|---|
| Processador | Intel Xeon @ 2.10 GHz (máquina virtual) |
| Núcleos físicos | 2 |
| Processadores lógicos | 2 (1 thread por núcleo) |
| Memória RAM | 7,8 GiB |
| Sistema operacional | Linux 6.18 (Ubuntu 24.04) |
| Arquitetura | x86_64 |
| Compilador | gcc 13.3.0 (`cc`) |
| Padrão da linguagem | C89/C90 |
| APIs POSIX utilizadas | `pthread_create`, `pthread_join`, `pthread_mutex_init/lock/unlock/destroy`, `clock_gettime(CLOCK_MONOTONIC)` |
| Flags de compilação | `-std=c89 -Wall -Wextra -pedantic -O2 -pthread` |

> O código evita `pthread_barrier_t`, que não existe no macOS, e usa apenas APIs disponíveis nos dois sistemas. [PREENCHER: se testar também no macOS, registre aqui.]

### 3.2 Compilação

```bash
make clean
make
```

Sem `make`:

```bash
cc -std=c89 -Wall -Wextra -pedantic -O2 src/conta-objetos-sequencial.c src/comum.c -o conta-objetos-sequencial
cc -std=c89 -Wall -Wextra -pedantic -O2 -pthread src/conta-objetos-paralelo.c src/comum.c -o conta-objetos-paralelo
```

### 3.3 Execução

```bash
./conta-objetos-sequencial <arquivo> | -g <linhas> <colunas> <densidade> <semente>
./conta-objetos-paralelo [-t threads] [-b blocos_lin blocos_col] [-v] <arquivo> | -g ...
```

**Exemplo reproduzível:**

```bash
./conta-objetos-sequencial tests/obrigatorios/ex3.txt
./conta-objetos-paralelo -t 4 -b 2 2 tests/obrigatorios/ex3.txt
```

### 3.4 Formato da entrada e da saída

A matriz pode vir de um arquivo texto ou ser gerada pelo próprio programa.

- **Arquivo:** primeira linha `linhas colunas`, seguida dos valores `0`/`1`. Espaços, tabulações, vírgulas e quebras de linha são ignorados, e linhas iniciadas por `#` antes do cabeçalho são comentários. A leitura valida dimensões, caracteres e quantidade de valores.
- **Geração (`-g L C d s`):** matriz L×C em que cada célula é `1` com probabilidade `d`. Usa um gerador *xorshift* de 32 bits próprio, e não `rand()`, para que a mesma semente produza a mesma matriz em qualquer sistema. Assim, as duas versões recebem exatamente os mesmos dados nos testes de desempenho, e o script de validação replica o gerador em Python.
- **Trabalhadores:** `-t N` define o número de threads; `-b L C` define a grade de blocos (padrão `2N × 2`).

Exemplo:

```text
$ ./conta-objetos-sequencial tests/obrigatorios/ex1.txt
Versao: sequencial
Matriz: 5 x 5
Objetos: 3
Tempo(ms): 0.003

$ ./conta-objetos-paralelo -t 4 -b 2 2 tests/obrigatorios/ex1.txt
Versao: paralela (pthreads)
Matriz: 5 x 5
Threads: 4
Blocos: 2 x 2
Objetos: 3
Tempo(ms): 0.228
```

## 4. Arquitetura da solução

### 4.1 Fluxo geral

```mermaid
flowchart TD
    A["Carregar ou gerar a matriz (thread principal)"] --> B["Validar entrada"]
    B --> C["Montar grade de blocos (fila de tarefas)"]
    C --> D["Fase 1 - paralela: rotulação local de cada bloco"]
    D --> J1["pthread_join: todos os blocos rotulados"]
    J1 --> E["Fase 2 - paralela: varredura das fronteiras"]
    E --> F["Aplicação das equivalências no union-find (mutex)"]
    F --> J2["pthread_join"]
    J2 --> G["Fase 3: objetos = soma dos locais - uniões"]
    G --> H["Destruir mutexes, liberar memória, apresentar resultado"]
```

### 4.2 Estruturas de dados principais

| Estrutura | Tipo/representação | Responsabilidade | Compartilhada? | Proteção utilizada |
|---|---|---|---|---|
| Matriz de entrada | `unsigned char *dados` contínuo, linha a linha | Armazenar `0` e `1` | Sim | Não se aplica: somente leitura durante a contagem |
| Células visitadas/rótulos | Sequencial: `unsigned char visitado[L·C]`. Paralela: `int rotulo[L·C]` | Distinguir células processadas e identificar o componente | Sim (paralela) | Particionamento: cada thread escreve só nas células do próprio bloco na fase 1; na fase 2 o vetor é apenas lido |
| Pilha do flood fill | `Pilha` dinâmica de `long` (índices lineares) | Percorrer um componente sem recursão | Não: uma por thread | Não se aplica |
| Tarefas/regiões | Vetor `Bloco[]` + contador `proxima_tarefa` | Distribuir trabalho dinamicamente | Sim | `mutex_fila` |
| Equivalências de rótulos | `int pai[L·C + 1]` (union-find) + contador `unioes` | Consolidar componentes | Sim | `mutex_uf` |
| Resultados locais | `long objetos_locais[num_blocos]` | Contagem de componentes por bloco | Sim | Particionamento: cada posição é escrita por uma única thread |

## 5. Implementação sequencial

### 5.1 Algoritmo

O vetor `visitado` é alocado com `calloc`, portanto começa todo com zero. A matriz é percorrida em ordem linear. Cada célula com valor `1` e ainda não visitada é a semente de um novo objeto: o contador é incrementado e um flood fill marca todo o componente.

O flood fill é iterativo: a semente é marcada e empilhada. Enquanto a pilha não estiver vazia, desempilha-se uma célula, calculam-se linha e coluna (`idx / colunas`, `idx % colunas`) e examinam-se os 8 vizinhos, obtidos somando os deslocamentos `VIZ_DL[k]`, `VIZ_DC[k]` (NO, N, NE, O, L, SO, S, SE). Vizinhos fora da matriz são descartados. Cada vizinho `1` não visitado é **marcado no momento em que é empilhado**. Isso garante que nenhuma célula entre na pilha duas vezes e que a pilha nunca ultrapasse `L·C` elementos.

### 5.2 Pseudocódigo

```text
FUNÇÃO contar_objetos_sequencial(matriz):
    visitado[0..L·C-1] <- 0
    objetos <- 0
    PARA i DE 0 ATÉ L·C-1:
        SE matriz[i] = 1 E NÃO visitado[i]:
            objetos <- objetos + 1
            visitado[i] <- 1 ; EMPILHA(i)
            ENQUANTO pilha não vazia:
                idx <- DESEMPILHA()
                PARA cada um dos 8 vizinhos v de idx dentro da matriz:
                    SE matriz[v] = 1 E NÃO visitado[v]:
                        visitado[v] <- 1 ; EMPILHA(v)
    RETORNA objetos
```

### 5.3 Complexidade e uso de memória

| Aspecto | Análise | Justificativa |
|---|---|---|
| Complexidade de tempo | O(L·C) | Cada célula é lida uma vez pelo laço externo e, se for `1`, empilhada e desempilhada exatamente uma vez, com 8 verificações constantes. |
| Complexidade de espaço | O(L·C) | Matriz (L·C bytes) + visitados (L·C bytes) + pilha (no pior caso L·C índices). |
| Risco de recursão excessiva | Não existe | A pilha é explícita no *heap* e cresce por duplicação (`realloc`). Uma versão recursiva, em uma matriz 6000×6000 com um objeto gigante, chegaria a milhões de chamadas aninhadas e estouraria a pilha do sistema (tipicamente 8 MB). |

## 6. Implementação paralela

### 6.1 Modelo de concorrência

| Decisão | Escolha do grupo | Justificativa |
|---|---|---|
| Unidade de execução | Thread (Pthreads) | Todas as threads precisam acessar a mesma matriz e o mesmo vetor de rótulos. Com threads isso é direto, porque compartilham o espaço de endereçamento. Com processos, seria preciso memória compartilhada (`mmap`/`shm_open`) e sincronização entre processos. Threads também são mais baratas de criar. |
| Quantidade de trabalhadores | `-t N` na linha de comando (1 a 1024; padrão 2) | Permite medir escalabilidade sem recompilar. |
| Divisão do trabalho | Grade de blocos retangulares `BL × BC` (`-b`; padrão `2N × 2`) | Blocos exercitam fronteiras horizontais, verticais e o encontro de quatro blocos, como nas grades do enunciado. |
| Escalonamento | Dinâmico (fila de tarefas) | Blocos têm custos diferentes (densidade de `1`s varia). Com mais blocos que threads, uma thread que termina cedo pega o próximo bloco, e o trabalho se equilibra sozinho. |
| Comunicação | Estruturas compartilhadas em memória (`rotulo`, `pai`, `objetos_locais`, contadores) | Comunicação natural entre threads (variáveis do processo). |
| Sincronização | 2 mutexes + `pthread_join` como barreira entre fases | Mutex para cada estrutura compartilhada mutável; `join` garante que a fase 2 só lê rótulos já definitivos. |

### 6.2 Decomposição da matriz

Os limites do bloco `(i, j)` são calculados por divisão inteira:

```text
l0 = i·L / BL      l1 = (i+1)·L / BL
c0 = j·C / BC      c1 = (j+1)·C / BC
```

Isso distribui as sobras automaticamente: quando `L` não é múltiplo de `BL`, os blocos diferem em no máximo uma linha. Por exemplo, 9 linhas em 2 faixas geram faixas de 4 e 5 linhas. Se forem pedidas mais faixas do que linhas ou colunas existentes, o programa reduz a grade (`BL ≤ L`, `BC ≤ C`), então nenhum bloco fica vazio. A quantidade de blocos pode ser maior que a de threads (caso padrão: `4N` blocos) ou menor. Threads que não conseguem tarefa terminam imediatamente.

```mermaid
flowchart LR
    M["Matriz"] --> Q["Fila de blocos (contador protegido por mutex_fila)"]
    Q --> W1["Thread 0"]
    Q --> W2["Thread 1"]
    Q --> WN["Thread N-1"]
    W1 --> R["rotulo[], pai[], objetos_locais[]"]
    W2 --> R
    WN --> R
    R --> C["Fase 2: fronteiras + union-find (mutex_uf)"]
    C --> T["Fase 3: soma dos locais - uniões"]
```

### 6.3 Paralelismo efetivo

O cálculo pesado, o flood fill de todas as células, é feito **simultaneamente** pelas threads na fase 1, cada uma em blocos diferentes. As threads não esperam umas pelas outras nessa fase: a única interação é retirar o próximo bloco da fila, o que custa alguns nanossegundos por bloco. A fase 2 também é paralela: a varredura das fronteiras só lê dados e é feita sem trava. Só a aplicação das uniões é serializada, e seu custo é proporcional ao perímetro dos blocos, não à área. A evidência de paralelismo real é a aceleração de 1,83 com 2 threads na matriz grande (seção 9).

| Etapa | Sequencial ou paralela? | Unidade responsável | Motivo |
|---|---|---|---|
| Leitura/geração da matriz | Sequencial | Thread principal | E/S de arquivo é sequencial; fica fora da medição de tempo. |
| Particionamento | Sequencial | Thread principal | O(número de blocos): custo desprezível. |
| Identificação local | **Paralela** | Threads trabalhadoras | Blocos são independentes: domina o tempo total. |
| Análise das fronteiras | **Paralela** | Threads trabalhadoras | Leitura de `rotulo`, que não muda mais. |
| Consolidação | Serializada por mutex | Threads trabalhadoras | Union-find compartilhado; um bloqueio por bloco. |
| Contagem final | Sequencial | Thread principal | Soma de `num_blocos` valores e uma subtração. |

### 6.4 Sincronização, comunicação e regiões críticas

| Recurso/dado | Risco concorrente | Mecanismo usado | Escopo da proteção | Justificativa |
|---|---|---|---|---|
| `proxima_tarefa` (fila) | Duas threads lerem o mesmo valor e processarem o mesmo bloco, ou pularem um bloco (atualização perdida) | `mutex_fila` | Só a leitura e o incremento do contador (`pegar_tarefa`) | É o mesmo problema do `x = x + 1` dos slides; a região crítica mínima reduz a contenção. |
| `pai[]` e `unioes` (union-find) | Duas uniões simultâneas sobrescreverem `pai[]` e perderem uma ligação; `unioes` contada errado | `mutex_uf` | A aplicação de todas as equivalências de um bloco (um bloqueio por bloco, não por par) | `uf_encontrar` também escreve (compressão de caminho), então nem as consultas podem ser concorrentes. |
| `ctx->erro` | Leitura e escrita simultâneas | `mutex_fila` | Leitura em `pegar_tarefa`; escrita em `sinalizar_erro` | Permite que todas as threads parem após uma falha. |
| `rotulo[]` | Duas threads escreverem na mesma célula | Particionamento + `pthread_join` | Fase 1: cada thread só escreve nas células do próprio bloco (o flood fill não sai do bloco). Fase 2: só leitura | Dispensa trava no caminho mais quente do programa. O `join` entre as fases garante que as escritas da fase 1 estão visíveis na fase 2. |
| `objetos_locais[]` | Escrita concorrente | Particionamento | Cada posição é escrita por uma única thread | O bloco `b` é entregue a exatamente uma thread. |
| Rótulos novos | Duas threads criarem o mesmo rótulo | Rótulo = índice linear da semente + 1 | — | Cada célula tem índice único, então rótulos de threads diferentes nunca colidem, **sem nenhuma comunicação**. |

**Ausência de deadlock:** nenhuma thread segura dois mutexes ao mesmo tempo. `mutex_fila` e `mutex_uf` são sempre adquiridos e liberados isoladamente, sem chamadas bloqueantes dentro das regiões críticas. Sem retenção e espera, não pode haver ciclo de espera. `pthread_join` só é chamado pela thread principal, que não segura nenhum mutex nesse momento.

## 7. Consolidação dos componentes

Somar as contagens locais estaria errado: no exemplo 3 com grade 2 × 2, a soma dos componentes locais é **8**, mas a resposta correta é **5**. O objeto central 2 × 2 ocupa os quatro blocos e seria contado quatro vezes.

### 7.1 Identificação local

Cada componente local recebe como rótulo o **índice linear da sua semente + 1** (`rot = idx + 1`), isto é, da primeira célula do componente encontrada na varredura do bloco. O valor `0` fica reservado para o fundo. Como índices lineares são únicos na matriz inteira, rótulos criados por threads diferentes em blocos diferentes já são distintos, sem contador global e sem trava. A thread que cria o rótulo também inicializa `pai[rot] = rot`: cada componente local começa como um conjunto próprio no union-find.

### 7.2 Verificação das fronteiras

Cada bloco é responsável pela sua **fronteira inferior** e pela sua **fronteira direita**. Assim, cada fronteira da grade é examinada exatamente uma vez. Pares em que alguma das células é fundo, ou em que os rótulos são iguais, são ignorados.

| Situação | Pares de células verificados | Como a equivalência é registrada |
|---|---|---|
| Fronteira horizontal (entre bloco de cima e de baixo) | Para cada `(r, c)` na última linha do bloco: `(r+1, c)` | Par `(min, max)` dos rótulos no buffer local da thread |
| Fronteira vertical (entre bloco da esquerda e da direita) | Para cada `(r, c)` na última coluna do bloco: `(r, c+1)` | Idem |
| Conexão diagonal | Fronteira inferior: `(r+1, c-1)` e `(r+1, c+1)`. Fronteira direita: `(r-1, c+1)` e `(r+1, c+1)` | Idem |
| Encontro de quatro blocos | Na célula do canto `(r, c)`, a diagonal `(r+1, c+1)` pertence ao bloco diagonal, e `(r, c+1)`/`(r+1, c)` formam a antidiagonal. Como a varredura da fronteira inferior percorre a linha inteira e olha `c-1` e `c+1`, as duas diagonais do canto são cobertas mesmo quando atravessam para o bloco da diagonal | Idem (ex.: união 28 ~ 37 no exemplo 3) |

Pares repetidos consecutivos, muito comuns quando um objeto atravessa a fronteira em várias células, são descartados ainda no buffer local.

### 7.3 Unificação e contagem global

É usada a estrutura **union-find** (conjuntos disjuntos). `uf_encontrar` segue `pai[]` até a raiz, com compressão de caminho por *halving*. `uf_unir(a, b)` liga a raiz maior à menor. Escolher sempre o menor rótulo como representante torna a estrutura final independente da ordem em que as threads fazem as uniões.

A consolidação ocorre na fase 2, depois do `pthread_join` da fase 1. Cada thread: (1) varre as fronteiras do bloco sem trava (apenas lê `rotulo`); (2) trava `mutex_uf` **uma vez**; (3) aplica todas as equivalências do bloco; (4) destrava.

A contagem final vem de uma propriedade simples: cada união **efetiva**, entre raízes diferentes, junta dois conjuntos em um e reduz o número de objetos em exatamente 1. Uniões redundantes retornam 0 e não são contadas. Logo:

```text
objetos = Σ objetos_locais[b]  −  unioes
```

Essa fórmula dispensa uma varredura final da matriz.

### 7.4 Exemplo rastreável

Exemplo 3 (8 × 8, grade 2 × 2, 2 threads). Saída completa em [`results/rastreio-ex3.txt`](results/rastreio-ex3.txt) (`./conta-objetos-paralelo -t 2 -b 2 2 -v tests/obrigatorios/ex3.txt`):

```text
Fase 1 - rotulos locais (rotulo = indice da semente + 1):
     1   1   .   .   |   .   .   .   .
     1   .   .   .   |   .   .   .   .
     .   .   .   .   |   .   .  23   .
     .   .   .  28   |  29   .  23   .
     ----------------+----------------
     .   .   .  36   |  37   .   .   .
     .   .   .   .   |   .   .   .   .
     .   .  51   .   |   .   .   .  56
     .   .  51   .   |   .   .  56  56
Objetos locais por bloco: 2 2 2 2  (soma = 8)

Fase 2 - unioes nas fronteiras:
  [bloco 0] une rotulo 28 com rotulo 36   (vertical)
  [bloco 0] une rotulo 28 com rotulo 37   (diagonal no encontro dos 4 blocos)
  [bloco 0] une rotulo 28 com rotulo 29   (horizontal)

Objetos = 8 (locais) - 3 (unioes) = 5
```

(As linhas separadoras foram acrescentadas aqui para mostrar os blocos.)

| Região | Rótulo local | Células de fronteira relevantes | Equivalência global |
|---|---|---|---|
| Bloco 0 (sup. esq.) | 1 | nenhuma (não toca fronteira) | 1 |
| Bloco 0 (sup. esq.) | 28 | (3,3): vizinha de (4,3), (4,4) e (3,4) | 28 |
| Bloco 1 (sup. dir.) | 29 | (3,4): vizinha de (3,3) | 28 |
| Bloco 1 (sup. dir.) | 23 | nenhuma | 23 |
| Bloco 2 (inf. esq.) | 36 | (4,3): vizinha de (3,3) | 28 |
| Bloco 2 (inf. esq.) | 51 | nenhuma | 51 |
| Bloco 3 (inf. dir.) | 37 | (4,4): vizinha diagonal de (3,3) | 28 |
| Bloco 3 (inf. dir.) | 56 | nenhuma | 56 |

Resultado: 5 representantes distintos, {1, 23, 28, 51, 56}. Os rastreios dos exemplos 4 (11 − 5 = 6) e 5 (13 − 6 = 7) estão em [`results/rastreio-ex4.txt`](results/rastreio-ex4.txt) e [`results/rastreio-ex5.txt`](results/rastreio-ex5.txt).

## 8. Correção e testes funcionais

### 8.1 Procedimento de validação

A validação foi automatizada em três níveis:

1. **`tests/executa-testes.sh`** (`make testes`): cada arquivo traz o valor esperado no cabeçalho (`# ... | esperado: N`). O script executa a versão sequencial e a paralela em 8 configurações — (threads, blocos) = (1, 1×1), (2, 1×2), (2, 2×2), (4, 2×2), (4, 3×3), (3, 1×5), (8, 4×4), (16, 12×12) — e depois repete a configuração (4, 3×3) 20 vezes para verificar o determinismo. Resultado: **406 execuções, 0 falhas** ([`results/testes.log`](results/testes.log), [`results/testes.csv`](results/testes.csv)).
2. **`tests/validacao-aleatoria.py`** (`make validacao`): 500 matrizes aleatórias com tamanho (1 a 60), densidade, semente, threads (1 a 8) e grade (1 a 12 × 1 a 12) sorteados. Cada caso é comparado com `scipy.ndimage.label` usando estrutura 3 × 3, uma implementação **independente** da nossa. Resultado: **500 casos, 0 falhas** ([`results/validacao-aleatoria.log`](results/validacao-aleatoria.log)).
3. **Matriz de desempenho:** em todas as 100 execuções registradas das medições, a versão paralela coincidiu com a sequencial (coluna `resultado_correto` de `medicoes.csv`).

Os valores esperados dos testes adicionais também foram calculados com o scipy, e não à mão.

### 8.2 Matrizes obrigatórias

| Exemplo | Dimensões | Objetos esperados | Resultado sequencial | Resultado paralelo | Trabalhadores | Situação | Evidência |
|---:|---:|---:|---:|---:|---:|---|---|
| 1 | 5 x 5 | 3 | 3 | 3 | 1, 2, 3, 4, 8, 16 | Aprovado | [`testes.csv`](results/testes.csv) |
| 2 | 6 x 8 | 4 | 4 | 4 | 1, 2, 3, 4, 8, 16 | Aprovado | [`testes.csv`](results/testes.csv) |
| 3 | 8 x 8 | 5 | 5 | 5 | 1, 2, 3, 4, 8, 16 | Aprovado | [`rastreio-ex3.txt`](results/rastreio-ex3.txt) |
| 4 | 9 x 12 | 6 | 6 | 6 | 1, 2, 3, 4, 8, 16 | Aprovado | [`rastreio-ex4.txt`](results/rastreio-ex4.txt) |
| 5 | 12 x 12 | 7 | 7 | 7 | 1, 2, 3, 4, 8, 16 | Aprovado | [`rastreio-ex5.txt`](results/rastreio-ex5.txt) |

As grades ilustradas no enunciado (2 × 2 para os exemplos 1-3 e 3 × 3 para os exemplos 4-5) estão entre as configurações testadas.

### 8.3 Casos de teste adicionais

| ID | Dimensões | Característica avaliada | Resultado de referência | Configurações paralelas | Resultado obtido | Situação |
|---|---:|---|---:|---|---:|---|
| A1 | 7 x 9 | Matriz vazia (somente zeros) | 0 | 8 configs + 20 repetições | 0 | Aprovado |
| A2 | 13 x 13 | Um único objeto em serpentina atravessando todos os blocos | 1 | 8 configs + 20 repetições | 1 | Aprovado |
| A3 | 10 x 10 | Tabuleiro de xadrez: conexões **somente** diagonais | 1 | 8 configs + 20 repetições | 1 | Aprovado |
| A4 | 6000 x 6000 | Matriz grande do desempenho (densidade 0,45, semente 42) | 263877 | 1, 2, 4, 8 threads × 10 execuções | 263877 | Aprovado |
| A5 | 10 x 10 | "X" de diagonais cruzando o encontro de quatro blocos | 1 | 8 configs + 20 repetições | 1 | Aprovado |
| A6 | 6 x 6 | Par antidiagonal isolado exatamente no canto de 4 blocos | 4 | 8 configs + 20 repetições | 4 | Aprovado |
| A7 | 1 x 25 | Matriz com uma única linha | 7 | 8 configs + 20 repetições | 7 | Aprovado |
| A8 | 25 x 1 | Matriz com uma única coluna | 7 | 8 configs + 20 repetições | 7 | Aprovado |
| A9 | 9 x 11 | Todas as células iguais a 1 | 1 | 8 configs + 20 repetições | 1 | Aprovado |
| A10 | 40 x 40 | Aleatória, densidade 0,45 | 23 | 8 configs + 20 repetições | 23 | Aprovado |
| A11 | 200 x 200 | Matriz pequena do desempenho | 278 | 1, 2, 4, 8 threads × 10 execuções | 278 | Aprovado |

O A6 foi desenhado para o caso mais sutil: duas células ligadas **apenas** pela antidiagonal do canto onde quatro blocos se encontram, cada uma em um bloco diferente. A7 e A8 testam a redução automática da grade quando há mais faixas do que linhas ou colunas.

### 8.4 Repetibilidade e determinismo

| Teste | Repetições | Configurações | Resultados idênticos? | Observações |
|---|---:|---|---|---|
| 14 matrizes (obrigatórias + adicionais) | 20 cada (280) | 4 threads, 3 × 3 blocos | Sim | 0 divergências. |
| Matriz grande 6000 × 6000 | 10 por configuração | 1, 2, 4, 8 threads | Sim | Sempre 263877. |
| 500 matrizes aleatórias | 1 cada | Threads e grade sorteadas | Sim | Iguais ao scipy e à versão sequencial. |

A **ordem** das uniões varia entre execuções (depende de qual thread chega primeiro ao mutex, como mostra o modo `-v`), mas o **resultado** não. O conjunto de pares examinados é sempre o mesmo, e o número de uniões efetivas necessárias para juntar um conjunto de componentes independe da ordem.

## 9. Avaliação de desempenho

### 9.1 Metodologia experimental

| Parâmetro | Valor adotado |
|---|---|
| Matriz ou conjunto de matrizes | "grande": 6000 × 6000 (36 milhões de células), densidade 0,45, semente 42 (263877 objetos). "pequena": 200 × 200, densidade 0,45, semente 42 (278 objetos). Densidade 0,45 fica acima do limiar de percolação da conectividade 8 (≈ 0,41), o que produz um objeto gigante atravessando todos os blocos e milhares de objetos pequenos: um caso difícil para a consolidação. |
| Mesmos dados em todas as versões? | Sim. Geradas pela opção `-g` com a mesma semente e o mesmo gerador determinístico. |
| Relógio/API de medição | `clock_gettime(CLOCK_MONOTONIC, ...)` |
| Trecho medido | Somente a contagem: alocação das estruturas, criação/`join` das threads, fases 1-3 e liberação. **Excluídos**: geração/leitura da matriz e impressão. |
| Aquecimentos descartados | 1 execução por configuração |
| Repetições por configuração | 10, **intercaladas** (em cada rodada todas as configurações rodam uma vez), para que o ruído momentâneo da máquina afete todas de forma parecida |
| Medida representativa | Mediana |
| Critério para dispersão | Intervalo interquartil (IQR = Q3 − Q1); mínimo e máximo em `resumo.csv` |
| Carga do sistema durante os testes | Máquina virtual dedicada, sem outras cargas do usuário. A VM apresentou ruído de até ±15 %, o que justificou a mediana. |
| Flags de otimização | `-O2` |

As medições brutas estão disponíveis em [`results/medicoes.csv`](results/medicoes.csv), e o resumo em [`results/resumo.csv`](results/resumo.csv). Para reproduzir: `make desempenho && make graficos`.

### 9.2 Métricas

A aceleração para `p` trabalhadores é calculada por:

$$
S(p) = \frac{T_{sequencial}}{T_{paralelo}(p)}
$$

A eficiência paralela é calculada por:

$$
E(p) = \frac{S(p)}{p}
$$

### 9.3 Resultados consolidados

**Matriz grande (6000 × 6000):**

| Versão | Trabalhadores (`p`) | Tempo representativo (ms) | Dispersão - IQR (ms) | Aceleração `S(p)` | Eficiência `E(p)` | Resultado correto? |
|---|---:|---:|---:|---:|---:|---|
| Sequencial | 1 | 1365,3 | 25,4 | 1,00 | 1,00 | Sim |
| Paralela | 1 | 1457,5 | 108,0 | 0,94 | 0,94 | Sim |
| Paralela | 2 | 745,8 | 49,7 | **1,83** | **0,92** | Sim |
| Paralela | 4 | 727,7 | 18,6 | 1,88 | 0,47 | Sim |
| Paralela | 8 | 718,7 | 19,9 | 1,90 | 0,24 | Sim |

**Matriz pequena (200 × 200):**

| Versão | Trabalhadores (`p`) | Tempo representativo (ms) | Dispersão - IQR (ms) | Aceleração `S(p)` | Eficiência `E(p)` | Resultado correto? |
|---|---:|---:|---:|---:|---:|---|
| Sequencial | 1 | 1,611 | 0,279 | 1,00 | 1,00 | Sim |
| Paralela | 1 | 1,826 | 0,240 | 0,88 | 0,88 | Sim |
| Paralela | 2 | 1,136 | 0,143 | 1,42 | 0,71 | Sim |
| Paralela | 4 | 1,239 | 0,082 | 1,30 | 0,33 | Sim |
| Paralela | 8 | 1,538 | 0,154 | 1,05 | 0,13 | Sim |

### 9.4 Dados brutos das repetições

Matriz grande (ms). As repetições da matriz pequena estão em `medicoes.csv`.

| Versão | p | R1 | R2 | R3 | R4 | R5 | R6 | R7 | R8 | R9 | R10 | Mediana |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Sequencial | 1 | 1369,2 | 1369,3 | 1445,0 | 1339,7 | 1378,3 | 1336,5 | 1355,6 | 1438,0 | 1349,0 | 1361,4 | 1365,3 |
| Paralela | 1 | 1552,0 | 1431,4 | 1543,7 | 1470,9 | 1398,3 | 1489,0 | 1419,0 | 1657,1 | 1444,2 | 1392,5 | 1457,5 |
| Paralela | 2 | 749,2 | 728,7 | 778,7 | 742,4 | 760,7 | 723,1 | 718,1 | 818,2 | 713,5 | 842,9 | 745,8 |
| Paralela | 4 | 741,5 | 748,0 | 759,6 | 724,9 | 741,3 | 730,5 | 723,8 | 711,0 | 722,5 | 717,4 | 727,7 |
| Paralela | 8 | 710,6 | 743,8 | 722,6 | 718,8 | 714,7 | 718,6 | 714,0 | 688,3 | 738,3 | 737,9 | 718,7 |

### 9.5 Gráfico de tempo de execução

![Tempo de execução por quantidade de trabalhadores](results/grafico-tempo.png)

**Figura 1 -** Tempo de execução da versão sequencial e das configurações paralelas. Barras de erro representam o intervalo interquartil (Q1 a Q3). Os painéis têm escalas diferentes porque os tempos diferem em três ordens de grandeza. Fonte: elaborado pelo grupo.

### 9.6 Gráfico de aceleração

![Aceleração por quantidade de trabalhadores](results/grafico-aceleracao.png)

**Figura 2 -** Aceleração observada em função da quantidade de trabalhadores. A linha ideal corresponde a `S(p) = p`; a linha pontilhada marca os 2 núcleos da máquina. Fonte: elaborado pelo grupo.

### 9.7 Gráfico de eficiência

![Eficiência por quantidade de trabalhadores](results/grafico-eficiencia.png)

**Figura 3 -** Eficiência paralela em função da quantidade de trabalhadores. Fonte: elaborado pelo grupo.

### 9.8 Análise dos resultados

**Ganho em relação à versão sequencial.** Na matriz grande, 2 threads reduziram o tempo de 1365 ms para 746 ms (S = 1,83; E = 0,92), perto do ideal para a máquina de 2 núcleos. Isso confirma que a fase 1, que domina o custo, é executada de fato em paralelo.

**Por que a versão paralela com 1 thread é 7 % mais lenta que a sequencial (S = 0,94).** Ela faz mais trabalho que a sequencial:

- usa `int rotulo[]` (4 bytes/célula, 144 MB) em vez de `unsigned char visitado[]` (1 byte/célula, 36 MB), o que quadruplica o tráfego de memória;
- aloca `pai[]` (mais 144 MB), cujas páginas causam *page faults* na primeira escrita;
- executa a fase 2 (fronteiras e union-find), que a sequencial não precisa;
- cria e espera threads duas vezes e trava mutexes a cada bloco.

Essa sobrecarga é o custo de tornar o problema decomponível. A comparação com a versão sequencial otimizada é a mais honesta, e é a usada no cálculo de `S(p)`.

**Por que S(2) = 1,83 e não 2.** Além da sobrecarga acima, a alocação e o preenchimento de 288 MB geram *page faults* tratados pelo kernel, que serializa parte desse trabalho. Os dois núcleos também disputam a mesma largura de banda de memória, e a fase 2 contém uma região crítica serializada. O IQR da configuração com 1 thread (108 ms) mostra que a própria VM varia bastante.

**Efeito da quantidade de threads (4 e 8).** A máquina tem **apenas 2 núcleos**. Com 4 ou 8 threads não há mais hardware para executá-las ao mesmo tempo: o SO as intercala nos mesmos 2 núcleos. Por isso a aceleração estabiliza em ≈ 1,9 e a eficiência cai para 0,47 e 0,24. Isso não é defeito do algoritmo: a eficiência é calculada sobre `p` threads, mas só havia 2 unidades físicas. O tempo não piorou, e chegou até a melhorar levemente (746 → 719 ms), por dois motivos: criar threads é barato, e mais threads significam mais blocos (padrão `2p × 2`), o que melhora o balanceamento dinâmico. [PREENCHER se possível: repetir `make desempenho` em uma máquina com 4+ núcleos para mostrar a escalabilidade além de 2 threads.]

**Matriz pequena.** Com 200 × 200 (40 mil células), a contagem inteira leva ~1,6 ms. Mesmo assim, 2 threads ainda ganham (S = 1,42), mas com 8 threads o ganho quase desaparece (S = 1,05). Cada execução com 8 threads cria e espera 16 threads (2 fases × 8), cada uma custando dezenas de microssegundos, e esse custo fixo passa a competir com um cálculo muito curto. Como alerta o enunciado, matrizes pequenas servem para validar correção, não para medir ganho.

**Custo da consolidação.** A fase 2 trabalha só sobre as fronteiras: com uma grade `BL × BC`, examina cerca de `(BL−1)·C + (BC−1)·L` células, frente às `L·C` da fase 1. Para 6000 × 6000 com 2 threads (grade 4 × 2), são ~24 mil células de fronteira contra 36 milhões no total (< 0,1 %). Travar o mutex uma vez por bloco, e não por par, mantém a contenção desprezível.

**Trechos que permanecem sequenciais.** Geração/leitura da matriz (fora da medição), montagem da grade (O(blocos)), a região crítica do union-find e a fase 3 (O(blocos)).

## 10. Tratamento de erros e qualidade do código

### 10.1 Chamadas e recursos POSIX

| Chamada/recurso | Erro verificado? | Ação em caso de falha | Liberação/finalização |
|---|---|---|---|
| `pthread_create` | Sim | Mensagem com `strerror(rc)`, sinaliza erro para as threads já criadas pararem, faz `join` somente nas criadas e retorna erro | `pthread_join` de todas as threads criadas |
| `pthread_join` | Sim | Mensagem e marca erro | — |
| `pthread_mutex_init` | Sim | Mensagem e aborta a contagem; só destrói os mutexes que foram inicializados (flags `mutex_*_ok`) | `pthread_mutex_destroy` (retorno verificado) |
| `pthread_mutex_lock` / `unlock` | Sim | Mensagem; a thread sinaliza erro e para | — |
| `fork` | Não se aplica | — | — |
| `clock_gettime` | Sim | `perror` | — |
| `fopen` / leitura | Sim | `perror`; validação de cabeçalho, caracteres e quantidade de valores | `fclose` em todos os caminhos |
| Memória alocada (`malloc`, `calloc`, `realloc`) | Sim | Mensagem "memória insuficiente"; a thread sinaliza erro | `free` em um único ponto de saída (`goto fim`), padrão comum em C para não duplicar limpeza |

Observação: as funções `pthread_*` não usam `errno`; retornam o código de erro, que é impresso com `strerror(rc)`.

### 10.2 Compilação e análise

| Verificação | Comando/ferramenta | Resultado |
|---|---|---|
| Compilação C89/C90 | `make OPT="-O2 -Werror"` | Sem erros |
| Avisos do compilador | `-Wall -Wextra -pedantic` | Nenhum aviso |
| Vazamentos de memória | Valgrind memcheck (`--leak-check=full`), ambas as versões | "All heap blocks were freed -- no leaks are possible"; 0 erros |
| Condições de corrida | ThreadSanitizer (`-fsanitize=thread`): 5 obrigatórias + 800 × 800 com 8 threads e 16 × 16 blocos. Valgrind Helgrind: 200 × 200, 4 threads | 0 avisos (TSan); 0 erros (Helgrind) |

Detalhes em [`results/verificacoes-qualidade.log`](results/verificacoes-qualidade.log).

Durante o desenvolvimento, o AddressSanitizer revelou um erro: vizinhos de fundo (rótulo 0) eram registrados como equivalências, e o union-find acessava `pai[0]`, nunca inicializado. A correção foi ignorar pares com rótulo 0 em `registrar_par`.

### 10.3 Separação de responsabilidades

- **Entrada, geração e utilitários** (`comum.c`/`comum.h`): leitura e validação de arquivo, gerador reproduzível, pilha dinâmica, relógio. As duas versões usam o mesmo código, o que garante dados e medição idênticos.
- **Processamento sequencial** (`contar_objetos_sequencial`, `flood_fill`).
- **Processamento paralelo**: rotulação (`rotular_bloco`), fronteiras (`analisar_fronteiras`, `registrar_par`), consolidação (`uf_encontrar`, `uf_unir`).
- **Sincronização**: concentrada em `pegar_tarefa`, `sinalizar_erro`, `executar_fase` e na região crítica de `analisar_fronteiras`.
- **Medição**: somente em `main`, em volta da chamada de contagem.
- **Testes e experimentos**: scripts separados em `tests/` e `results/`.

## 11. Limitações e decisões de projeto

| Limitação ou decisão | Impacto | Alternativa considerada | Motivo da escolha |
|---|---|---|---|
| Threads em vez de processos | Menor isolamento: um erro derruba o programa todo | Processos com `fork` + memória compartilhada (`mmap`) + semáforos POSIX | Rótulos e union-find precisam ser compartilhados; com threads isso é natural e mais barato. |
| Rótulo = índice linear + 1 (tipo `int`) | Limita a matriz a ~2,1 bilhões de células; `pai[]` tem o tamanho da matriz (4 bytes/célula) | Contador de rótulos por bloco com prefixo somado após a fase 1 (economiza memória) | Rótulos únicos sem nenhuma coordenação e sem etapa extra. Memória suficiente para os casos do trabalho. |
| Union-find com um mutex global | Uniões serializadas | Mutex por raiz ou operações atômicas (*compare-and-swap*) | O custo da fase 2 é proporcional ao perímetro dos blocos (< 0,1 % das células); operações atômicas não fazem parte do C89. |
| `pthread_join` como barreira (threads recriadas por fase) | ~dezenas de µs a mais por fase | `pthread_barrier_t`, ou barreira com mutex + variável de condição | `pthread_barrier_t` não existe no macOS. O `join` é simples, portável e seu custo é desprezível na matriz grande. |
| Grade padrão `2p × 2` | Mais blocos que threads, com mais fronteiras | 1 bloco por thread (faixas) | Balanceamento dinâmico sem aumentar muito o perímetro. Configurável com `-b`. |
| Medições em máquina de 2 núcleos | Não mostra escalabilidade além de 2 | — | Único ambiente disponível no momento das medições. [PREENCHER se medir em outra máquina.] |

## 12. Conclusão

Os objetivos foram alcançados. As versões sequencial e paralela contam corretamente objetos com conectividade 8, produzindo os valores esperados nas cinco matrizes obrigatórias, em nove casos adicionais, em oito configurações de threads e blocos, em 280 repetições de determinismo e em 500 matrizes aleatórias comparadas com uma implementação independente. ThreadSanitizer, Helgrind e Valgrind não apontaram condições de corrida nem erros de memória.

No desempenho, a versão paralela obteve aceleração de 1,83 com 2 threads na matriz 6000 × 6000 (eficiência 0,92), próximo do ideal para os 2 núcleos disponíveis. Com mais threads que núcleos, a aceleração estabiliza em ≈ 1,9, como esperado. Na matriz 200 × 200, o custo fixo de criar threads quase anula o ganho com 8 threads, o que ilustra por que matrizes pequenas não servem para avaliar desempenho.

O principal aprendizado é que paralelizar exige tornar o problema decomponível. O flood fill paralelo em si é simples; o essencial está em (1) escolher rótulos que não colidem sem comunicação, (2) restringir as escritas de cada thread à sua região, para dispensar travas no caminho mais custoso, e (3) concentrar a sincronização em uma etapa pequena, a consolidação das fronteiras. Uma melhoria futura realista seria compactar os rótulos após a fase 1, reduzindo o vetor `pai[]` ao número de componentes locais, e medir o programa em uma máquina com mais núcleos.

## 13. Vídeo de apresentação

| Campo | Informação |
|---|---|
| Plataforma | [YouTube / Vimeo] |
| Link privado ou não listado | [INSERIR URL COMPLETA] |
| Duração | [MM:SS - máximo de 10 minutos] |
| Privacidade | [Não listado / privado compartilhado com o professor / protegido por senha] |
| Senha, se aplicável | [PREENCHER ou `Não se aplica`] |
| Data da última verificação do acesso | [DD/MM/AAAA] |

> **Importante:** o vídeo deve permanecer acessível ao professor durante todo o período de avaliação. Teste o link em uma janela anônima antes da entrega.

### 13.1 Conteúdo do vídeo

- [ ] Problema e estratégia escolhida.
- [ ] Implementação sequencial e referência de correção.
- [ ] Decomposição, processos/threads e sincronização.
- [ ] Consolidação de objetos que atravessam regiões (sugestão: demonstrar `-v` no exemplo 3).
- [ ] Demonstração executável.
- [ ] Testes obrigatórios e adicionais (`make testes`).
- [ ] Resultados de desempenho.
- [ ] Conclusões.
- [ ] Participação de ambos os integrantes, quando o trabalho for em dupla.

## 14. Contribuições dos integrantes

| Atividade | Integrante 1 | Integrante 2 | Evidência/observação |
|---|---|---|---|
| Projeto da solução sequencial | [PREENCHER] | [PREENCHER] | [PREENCHER] |
| Projeto da solução paralela | [PREENCHER] | [PREENCHER] | [PREENCHER] |
| Sincronização/comunicação | [PREENCHER] | [PREENCHER] | [PREENCHER] |
| Consolidação | [PREENCHER] | [PREENCHER] | [PREENCHER] |
| Testes e medições | [PREENCHER] | [PREENCHER] | [PREENCHER] |
| Documentação e apresentação | [PREENCHER] | [PREENCHER] | [PREENCHER] |

Todos os integrantes declaram compreender integralmente o código, as estruturas de dados, a divisão do trabalho, a sincronização, a comunicação, a consolidação e os resultados apresentados.

## 15. Ferramentas, bibliotecas, referências e códigos externos

| Recurso | Finalidade | Origem/link | Licença, quando aplicável | Partes do projeto afetadas |
|---|---|---|---|---|
| POSIX Threads | Criação de threads e mutexes | Biblioteca do sistema (`-pthread`) | — | `src/conta-objetos-paralelo.c` |
| SciPy (`scipy.ndimage.label`) | Oráculo independente para validar contagens | https://scipy.org | BSD-3-Clause | `tests/validacao-aleatoria.py`, valores esperados de `tests/adicionais/` |
| NumPy | Manipulação das matrizes nos testes | https://numpy.org | BSD-3-Clause | `tests/validacao-aleatoria.py` |
| Matplotlib | Gráficos de desempenho | https://matplotlib.org | Licença Matplotlib (PSF-like) | `results/gera-graficos.py` |
| Valgrind (memcheck, helgrind) | Vazamentos e erros de sincronização | https://valgrind.org | GPL-2.0 | Verificação (seção 10.2) |
| ThreadSanitizer / AddressSanitizer (gcc) | Detecção de condições de corrida e de erros de memória | gcc | GPL-3.0 | Verificação (seção 10.2) |
| Union-find (conjuntos disjuntos) | Algoritmo clássico de consolidação | Cormen et al., *Algoritmos: Teoria e Prática*, cap. "Estruturas de dados para conjuntos disjuntos" | — | `uf_encontrar`, `uf_unir` |
| Slides da disciplina | Processos, threads, mutex | Prof. Filipo Novo Mór | — | Projeto geral |
| Claude (Anthropic), assistente de IA | [PREENCHER: descrever como foi usado e como os resultados foram verificados e compreendidos pelo grupo] | https://claude.ai | — | [PREENCHER] |

O código C não usa bibliotecas externas além da biblioteca padrão e de Pthreads.

## 16. Checklist de entrega

### Código e execução

- [x] O código segue ANSI C C89/C90.
- [x] O projeto compila em Linux ou macOS.
- [x] A compilação ocorre sem erros e os avisos foram tratados ou justificados.
- [x] As principais chamadas POSIX têm os retornos verificados.
- [x] Todos os recursos são finalizados ou liberados corretamente.
- [x] A versão sequencial conta componentes com conectividade 8.
- [x] A versão paralela distribui cálculo real entre pelo menos duas unidades.
- [x] A quantidade de processos/threads é configurável.
- [x] Conexões horizontais, verticais e diagonais são preservadas.
- [x] Componentes que atravessam regiões são consolidados sem duplicidade.
- [x] Não há condições de corrida, deadlocks ou atualizações perdidas conhecidas.

### Testes e desempenho

- [x] As cinco matrizes obrigatórias foram executadas nas duas versões.
- [x] A versão paralela produziu exatamente os mesmos resultados da sequencial.
- [x] Foi criada pelo menos uma matriz maior para o teste de desempenho.
- [x] Foram testadas pelo menos duas quantidades de processos/threads.
- [x] As medições foram repetidas e o valor representativo foi explicado.
- [x] Tempo sequencial, tempo paralelo, aceleração e eficiência foram informados.
- [x] Resultados em que a versão paralela foi mais lenta foram explicados.
- [x] Dados brutos, tabelas e gráficos estão versionados no repositório.

### Repositório e apresentação

- [ ] O repositório do GitHub está público.
- [x] `README.md` contém descrição, autoria, compilação, execução e arquitetura.
- [x] O `Makefile` ou as instruções equivalentes permitem compilação reproduzível.
- [x] As matrizes de teste e seus resultados estão incluídos.
- [x] A análise de desempenho está incluída.
- [ ] Os slides estão em `slides/apresentacao.pdf`.
- [ ] O link do vídeo está acessível e o vídeo tem até 10 minutos.
- [ ] Ferramentas, referências, bibliotecas e códigos externos foram identificados.
- [ ] O hash do commit avaliado foi registrado neste relatório.

## Apêndice A - Registro de comandos

```bash
# Informações do ambiente
uname -srm; lscpu; free -h; cc --version     # salvo em results/ambiente.txt

# Compilação
make clean && make
make OPT="-O2 -Werror"

# Execução dos testes obrigatórios e adicionais
make testes                                   # -> results/testes.log, results/testes.csv
python3 tests/validacao-aleatoria.py 500      # -> results/validacao-aleatoria.log
./conta-objetos-paralelo -t 2 -b 2 2 -v tests/obrigatorios/ex3.txt   # rastreio

# Verificações de qualidade
cc -std=c89 -g -O1 -fsanitize=thread -pthread src/conta-objetos-paralelo.c src/comum.c -o par-tsan
valgrind --leak-check=full ./conta-objetos-paralelo -t 4 -b 4 4 -g 300 300 0.45 5
valgrind --tool=helgrind ./conta-objetos-paralelo -t 4 -b 4 4 -g 200 200 0.45 5

# Execução dos testes de desempenho
make desempenho                               # -> results/medicoes.csv, results/ambiente.txt
make graficos                                 # -> results/resumo.csv, results/*.png
```

## Apêndice B - Formato dos dados brutos

`results/medicoes.csv`:

```csv
matriz,linhas,colunas,versao,trabalhadores,repeticao,tempo_ms,objetos,resultado_correto
grande,6000,6000,sequencial,1,1,1369.209,263877,true
grande,6000,6000,paralela,1,1,1552.033,263877,true
grande,6000,6000,paralela,2,1,749.151,263877,true
```

## Apêndice C - Correspondência com os critérios de avaliação

| Critério | Peso | Seções com evidências |
|---|---:|---|
| Correção sequencial e paralela, incluindo conectividade 8 | 2,0 | 5, 6, 7 e 8 |
| Decomposição do problema e paralelismo efetivo | 1,5 | 6.1, 6.2 e 6.3 |
| Sincronização, comunicação e ausência de condições de corrida | 1,5 | 6.4 e 10 |
| Consolidação de objetos que atravessam regiões | 1,5 | 7 |
| Testes obrigatórios, adicionais e análise de desempenho | 1,0 | 8 e 9 |
| Qualidade do código ANSI C e tratamento de erros | 1,0 | 3 e 10 |
| Organização do repositório e documentação | 0,5 | 2, 3 e 16 |
| Apresentação, demonstração e domínio da implementação | 1,0 | 13 e 14 |
