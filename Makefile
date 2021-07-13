# specifica del compilatore (se metti g++ hai il compilatore per il c++)
CC = gcc

# -c serve a creare i file .o senza linkarli all'eseguibile
CFLAGS = -std=c99 -std=gnu99 -Wall -c

# questo serve a linkare la/e libreria/e all'eseguibile specificando un percorso
# per usare questa sintassi devi chiamare la libreria lib[nome_libreria].a
LIB = -L . -l/libfunzioniserver -lpthread

LIB2 = -L . -l/libfunzioniclient -lpthread

# file oggetto dell'eseguibile
OBJ1 = obj/server.o

OBJ2 = obj/client.o

# file oggetto della libreria
LIB_OBJ = obj/parsing.o obj/utils.o obj/icl_hash.o

LIB_OBJ2 = obj/utils.o obj/api.o

TARGET = server client

.PHONY = all clean cleanall

all: $(TARGET)

# $^ viene sostituito dalla lista dopo i 2 punti (in questo caso: $(OBJ) libfunzioni.a)
# $@ viene sostituito dal nome del target (in questo caso: eseguibile)
# non servono ma sono comodi per non dover scrivere tanto 
server: $(OBJ1) lib/libfunzioniserver.a
	$(CC) $^ -o $@ $(LIB)

client: $(OBJ2) lib/libfunzioniclient.a
	$(CC) $^ -o $@ $(LIB2)

lib/libfunzioniserver.a: $(LIB_OBJ)
	ar rvs $@ $^

lib/libfunzioniclient.a: $(LIB_OBJ2)
	ar rvs $@ $^

obj/parsing.o: parsing.h parsing.c
	$(CC) $(CFLAGS) parsing.c -o obj/parsing.o

obj/utils.o: utils.h utils.c
	$(CC) $(CFLAGS) utils.c -o obj/utils.o

obj/api.o: api.h api.c
	$(CC) $(CFLAGS) api.c -o obj/api.o

obj/icl_hash.o: icl_hash.h icl_hash.c
	$(CC) $(CFLAGS) icl_hash.c -o obj/icl_hash.o
	
obj/server.o: server.c
	$(CC) $(CFLAGS) server.c -o obj/server.o

obj/client.o: client.c
	$(CC) $(CFLAGS) client.c -o obj/client.o

clean:
	-rm -f $(TARGET)

cleanall:
	-rm -f obj/*.o lib/*.a *~ *.sk $(TARGET)

test1: $(TARGET)
	valgrind --leak-check=full ./server -f configfile/configtest1.txt & bash ./script/test1.sh; kill -1 $$!
	@sleep 1
	@printf "\ntest1 terminato\n"

test2: $(TARGET)
	./server -f configfile/configtest2.txt & bash ./script/test2.sh; kill -1 $$!
	@sleep 1
	@printf "\ntest2 terminato\n"