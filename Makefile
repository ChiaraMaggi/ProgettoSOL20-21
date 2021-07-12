# specifica del compilatore (se metti g++ hai il compilatore per il c++)
CC = gcc

# -c serve a creare i file .o senza linkarli all'eseguibile
CFLAGS = -std=c99 -std=gnu99 -Wall -c

# questo serve a linkare la/e libreria/e all'eseguibile specificando un percorso
# per usare questa sintassi devi chiamare la libreria lib[nome_libreria].a
LIB = -L . -lfunzioniserver -lpthread

LIB2 = -L . -lfunzioniclient -lpthread

# file oggetto dell'eseguibile (possono essere più di 1)
OBJ1 = server.o

OBJ2 = client.o

# file oggetto della libreria
LIB_OBJ = parsing.o utils.o icl_hash.o

LIB_OBJ2 = utils.o api.o

TARGET = server client

.PHONY = all clean cleanall

all: $(TARGET)

# $^ viene sostituito dalla lista dopo i 2 punti (in questo caso: $(OBJ) libfunzioni.a)
# $@ viene sostituito dal nome del target (in questo caso: eseguibile)
# non servono ma sono comodi per non dover scrivere tanto 
server: $(OBJ1) libfunzioniserver.a
	$(CC) $^ -o $@ $(LIB)

client: $(OBJ2) libfunzioniclient.a
	$(CC) $^ -o $@ $(LIB2)

libfunzioniserver.a: $(LIB_OBJ)
	ar rvs $@ $^

libfunzioniclient.a: $(LIB_OBJ2)
	ar rvs $@ $^

parsing.o: parsing.h parsing.c
	$(CC) $(CFLAGS) parsing.c -o parsing.o

utils.o: utils.h utils.c
	$(CC) $(CFLAGS) utils.c -o utils.o

api.o: api.h api.c
	$(CC) $(CFLAGS) api.c -o api.o

icl_hash.o: icl_hash.h icl_hash.c
	$(CC) $(CFLAGS) icl_hash.c -o icl_hash.o
	
server.o: server.c
	$(CC) $(CFLAGS) server.c -o server.o

client.o: client.c
	$(CC) $(CFLAGS) client.c -o client.o

clean:
	-rm -f $(TARGET)

cleanall:
	-rm -f *.o *.a *~ *.sk $(TARGET)

test1: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all ./server -f configtest1.txt & bash ./test1.sh; kill -1 $$!
	sleep 1
	@printf "\ntest1 terminato\n"

test2: $(TARGET)
	clear
	./server -f configtest2.txt &
	./test2.sh
	@printf "\ntest2 terminato\n"