/**
 * @file api.c
 * @author Chiara Maggi
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#define _POSIX_C_SOURCE 200112L
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<errno.h>
#include<time.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/un.h>
#include<stdarg.h>
#include<fcntl.h>
#include <sys/stat.h>

#include "utils.h"
#include "api.h"

#define NUMBUCKETS 100

/*variabile globale per fd del client*/
static int fd_socket = 0;
struct sockaddr_un server_address;
static int connectionOn = 0;


/*Viene aperta una connessione AF_UNIX al socket file sockname. Se il server non accetta immediatamente la
richiesta di connessione, la connessione da parte del client viene ripetuta dopo ‘msec’ millisecondi e fino allo
scadere del tempo assoluto ‘abstime’ specificato come terzo argomento. Ritorna 0 in caso di successo, -1 in caso
di fallimento, errno viene settato opportunamente.*/
int openConnection(const char* sockname, int msec, const struct timespec abstime){
    if(!sockname || msec < 0){
        errno = EINVAL;
        return -1;
    }

    CHECK_EQ_EXIT((fd_socket=socket(AF_UNIX,SOCK_STREAM,0)), -1, "socket");

    /*settaggio della struttura*/
    memset(&server_address, '0', sizeof(server_address));
    server_address.sun_family = AF_UNIX;
    strncpy(server_address.sun_path, sockname, UNIX_PATH_MAX);

    int sleeptime = msec/1000;
    int connectionflag;
    time_t trytime;
    time(&trytime);

    /*gestico la situzione in cui il client faccia richesta al server che non è stato ancora svegliato*/
    while (((connectionflag = connect(fd_socket,(struct sockaddr*)&server_address, sizeof(server_address))) == -1) && (abstime.tv_sec - trytime) > 0){
        if(errno == ENOENT){
            if (connectionflag == -1){
                sleep(sleeptime); 
                trytime = trytime + sleeptime;
            }
        }
    }
    if(connectionflag == -1){
        return -1;
    }
    connectionOn = 1; //connesione attiva
    return 0;
}

/*Chiude la connessione AF_UNIX associata al socket file sockname. Ritorna 0 in caso di successo, -1 in caso di
fallimento, errno viene settato opportunamente.*/
int closeConnection(const char* sockname){
    if(!sockname){
        errno = EINVAL; //valore non validp
        return -1;
    }
    if(strncmp(server_address.sun_path, sockname, strlen(sockname)+1) ==  0){
    	type_t request = CLOSECONN;
        CHECK_EQ_RETURN((writen(fd_socket, &request, sizeof(type_t))), -1, "writen", -1);
    	CHECK_EQ_RETURN((readn(fd_socket, &request, sizeof(type_t))), -1, "readn", -1);
        return close(fd_socket);
    }else{
        errno = EFAULT; //connessione inesistente
        return -1;
    }
}

/*Richiesta di apertura o di creazione di un file. La semantica della openFile dipende dai flags passati come secondo
argomento che possono essere O_CREATE ed O_LOCK. Se viene passato il flag O_CREATE ed il file esiste già
memorizzato nel server, oppure il file non esiste ed il flag O_CREATE non è stato specificato, viene ritornato un
errore. In caso di successo, il file viene sempre aperto in lettura e scrittura, ed in particolare le scritture possono
avvenire solo in append. Se viene passato il flag O_LOCK (eventualmente in OR con O_CREATE) il file viene
aperto e/o creato in modalità locked, che vuol dire che l’unico che può leggere o scrivere il file ‘pathname’ è il
processo che lo ha aperto. Il flag O_LOCK può essere esplicitamente resettato utilizzando la chiamata unlockFile,
descritta di seguito.
Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.*/
int openFile(const char* pathname, int flags){
    if(!pathname){
        errno = EINVAL;
        return -1;
    }
    if(connectionOn != 1){
        errno = ENOTCONN;
        return -1;
    }
    type_t request;
    if(flags == 0)
        request = OPEN;
    if(flags == 01)
        request = OPENC;
    
    int len = strlen(pathname);
    CHECK_EQ_RETURN((writen(fd_socket, &request, sizeof(int))), -1, "writen", -1);    
    CHECK_EQ_RETURN((writen(fd_socket, &len, sizeof(int))), -1, "writen", -1);
    CHECK_EQ_RETURN((writen(fd_socket, pathname, len*sizeof(char))), -1, "writen", -1);

    int answer = -1;  //risposta server
    CHECK_EQ_RETURN((readn(fd_socket, &answer, sizeof(int))), -1, "readn", -1);
	if(answer == -1){
		errno = ECANCELED;
		return -1;
	}
    return 0;
}

/*Legge tutto il contenuto del file dal server (se esiste) ritornando un puntatore ad un'area allocata sullo heap nel
parametro ‘buf’, mentre ‘size’ conterrà la dimensione del buffer dati (ossia la dimensione in bytes del file letto). In
caso di errore, ‘buf‘e ‘size’ non sono validi. Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene
settato opportunamente.*/
int readFile(const char* pathname, void** buf, size_t* size){
    if(!pathname){
        errno = EINVAL;
        return -1;
    }
    if(connectionOn != 1){
        errno = ENOTCONN;
        return -1;
    }
    type_t request = READ;
    int len = strlen(pathname);
    CHECK_EQ_RETURN((writen(fd_socket, &request, sizeof(int))), -1, "writen", -1);    
    CHECK_EQ_RETURN((writen(fd_socket, &len, sizeof(int))), -1, "writen", -1);
    CHECK_EQ_RETURN((writen(fd_socket, pathname, len*sizeof(char))), -1, "writen", -1);

    char* answer;
    CHECK_EQ_RETURN((readn(fd_socket, answer, sizeof(answer))), -1, "readn", -1);


}

/*Scrive tutto il file puntato da pathname nel file server. Ritorna successo solo se la precedente operazione,
terminata con successo, è stata openFile(pathname, O_CREATE| O_LOCK). Se ‘dirname’ è diverso da NULL, il
file eventualmente spedito dal server perchè espulso dalla cache per far posto al file ‘pathname’ dovrà essere
scritto in ‘dirname’; Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.*/
//int writeFile(const char* pathname, const char* dirname);

/*Richiesta di scrivere in append al file ‘pathname‘ i ‘size‘ bytes contenuti nel buffer ‘buf’. L’operazione di append
nel file è garantita essere atomica dal file server. Se ‘dirname’ è diverso da NULL, il file eventualmente spedito
dal server perchè espulso dalla cache per far posto ai nuovi dati di ‘pathname’ dovrà essere scritto in ‘dirname’;
Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.*/
//int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);

/*In caso di successo setta il flag O_LOCK al file. Se il file era stato aperto/creato con il flag O_LOCK e la
richiesta proviene dallo stesso processo, oppure se il file non ha il flag O_LOCK settato, l’operazione termina
immediatamente con successo, altrimenti l’operazione non viene completata fino a quando il flag O_LOCK non
viene resettato dal detentore della lock. L’ordine di acquisizione della lock sul file non è specificato. Ritorna 0 in
caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.*/
//int lockFile(const char* pathname);

/*Resetta il flag O_LOCK sul file ‘pathname’. L’operazione ha successo solo se l’owner della lock è il processo che
ha richiesto l’operazione, altrimenti l’operazione termina con errore. Ritorna 0 in caso di successo, -1 in caso di
fallimento, errno viene settato opportunamente.*/
//int unlockFile(const char* pathname);

/*Richiesta di chiusura del file puntato da ‘pathname’. Eventuali operazioni sul file dopo la closeFile falliscono.
Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.*/
//int closeFile(const char* pathname);

/*Rimuove il file cancellandolo dal file storage server. L’operazione fallisce se il file non è in stato locked, o è in
stato locked da parte di un processo client diverso da chi effettua la removeFile.*/
//int removeFile(const char* pathname);
