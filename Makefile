# Makefile - Contagem paralela de objetos em matriz binaria
# Uso: make | make testes | make desempenho | make graficos | make clean

CC      ?= cc
CSTD     = -std=c89 -Wall -Wextra -pedantic
OPT     ?= -O2
CFLAGS   = $(CSTD) $(OPT)
LDFLAGS  = -pthread

SEQ = conta-objetos-sequencial
PAR = conta-objetos-paralelo

all: $(SEQ) $(PAR)

$(SEQ): src/conta-objetos-sequencial.c src/comum.c src/comum.h
	$(CC) $(CFLAGS) src/conta-objetos-sequencial.c src/comum.c -o $@

$(PAR): src/conta-objetos-paralelo.c src/comum.c src/comum.h
	$(CC) $(CFLAGS) -pthread src/conta-objetos-paralelo.c src/comum.c -o $@ $(LDFLAGS)

testes: all
	./tests/executa-testes.sh

validacao: all
	python3 tests/validacao-aleatoria.py

desempenho: all
	./results/mede-desempenho.sh

graficos:
	python3 results/gera-graficos.py

clean:
	rm -f $(SEQ) $(PAR) *.o

.PHONY: all testes validacao desempenho graficos clean
