# specifica del compilatore (se metti g++ hai il compilatore per il c++)
CC = gcc

# -c serve a creare i file .o senza linkarli all'eseguibile
CFLAGS = -c -Wall -std=c99

# questo serve a linkare la/e libreria/e all'eseguibile specificando un percorso
# per usare questa sintassi devi chiamare la libreria lib[nome_libreria].a
LIB = -L . -lfunzioni

# file oggetto dell'eseguibile (possono essere più di 1)
OBJ = server.o

# file oggetto della libreria
LIB_OBJ = parsing.o utils.o

# questo non serve ma è buona norma
TARGET = eseguibile libfunzioni.a

all: $(TARGET)

# $^ viene sostituito dalla lista dopo i 2 punti (in questo caso: $(OBJ) libfunzioni.a)
# $@ viene sostituito dal nome del target (in questo caso: eseguibile)
# non servono ma sono comodi per non dover scrivere tanto 
eseguibile: $(OBJ) libfunzioni.a
	$(CC) $^ -o $@ $(LIB)

libfunzioni.a: $(LIB_OBJ)
	ar rvs $@ $^

server.o: server.c
	$(CC) $(CFLAGS) server.c -o server.o	

parsing.o: parsing.h parsing.c
	$(CC) $(CFLAGS) parsing.c -o parsing.o

utils.o: utils.h utils.c
	$(CC) $(CFLAGS) utils.c -o utils.o

clean:
	rm *.o *.a eseguibile
