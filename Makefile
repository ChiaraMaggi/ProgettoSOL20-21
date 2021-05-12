# specifica del compilatore (se metti g++ hai il compilatore per il c++)
CC = gcc

# -c serve a creare i file .o senza linkarli all'eseguibile
CFLAGS = -std=c99 -Wall -c

# questo serve a linkare la/e libreria/e all'eseguibile specificando un percorso
# per usare questa sintassi devi chiamare la libreria lib[nome_libreria].a
LIB = -L . -lfunzioni -lpthread

# file oggetto dell'eseguibile (possono essere più di 1)
OBJ1 = server.o

OBJ2 = client.o

# file oggetto della libreria
LIB_OBJ = parsing.o utils.o queue.o

# questo non serve ma è buona norma
TARGET = eseguibile1 eseguibile2 libfunzioni.a

all: $(TARGET)

# $^ viene sostituito dalla lista dopo i 2 punti (in questo caso: $(OBJ) libfunzioni.a)
# $@ viene sostituito dal nome del target (in questo caso: eseguibile)
# non servono ma sono comodi per non dover scrivere tanto 
eseguibile1: $(OBJ1) libfunzioni.a
	$(CC) $^ -o $@ $(LIB)

eseguibile2: $(OBJ2) libfunzioni.a
	$(CC) $^ -o $@ $(LIB)

libfunzioni.a: $(LIB_OBJ)
	ar rvs $@ $^

parsing.o: parsing.h parsing.c
	$(CC) $(CFLAGS) parsing.c -o parsing.o

utils.o: utils.h utils.c
	$(CC) $(CFLAGS) utils.c -o utils.o

queue.o: queue.h queue.c
	$(CC) $(CFLAGS) queue.c -o queue.o

server.o: server.c
	$(CC) $(CFLAGS) server.c -o server.o

client.o: client.c
	$(CC) $(CFLAGS) client.c -o client.o

clean:
	rm *.o *.a *.sk eseguibile1 eseguibile2
