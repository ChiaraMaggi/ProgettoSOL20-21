/**
 * @file server.c
 * @author Chiara Maggi
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#define _POSIX_C_SOURCE 200112L
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<sys/un.h>
#include<pthread.h>
#include<assert.h>
#include<sys/wait.h>
#include<sys/select.h>
#include<signal.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<sys/time.h>
#include<sys/uio.h>
#include<time.h>

#include "parsing.h"
#include "utils.h"
#include "icl_hash.h"


#define DEFAULT_WORKERS_THREAD 10
#define DEFAULT_MAX_FILE 100
#define DEFAULT_STORAGE_SIZE 512
#define DEFAULT_SOCKET_NAME "SOLsocket.sk"
#define NUMBUCKETS 100
#define MAX_PATH 200

#define SYSCALL_PTHREAD(c,s) \
    if((c)!=0) { perror(s);fflush(stdout);exit(EXIT_FAILURE); }
// utility exit
#define SYSCALL_EXIT(c,e) \
    if(c==-1) { perror(e);exit(EXIT_FAILURE); }
// utility break;
#define SYSCALL_BREAK(c,e) \
    if(c==-1) { perror(e);break; }

/*================================== STRUTTURE UTILI ====================================================*/
typedef struct File{
    char pathname[MAX_PATH];
    int fdcreator;
    QueueNode_t* openby;
    void* contenuto;
    int size;
}File_t;

typedef struct Serverstate
{
   int num_file;
   size_t used_space;
   size_t free_space;
}Serverstate_t;

typedef enum {OPEN, OPENC, CLOSECONN, WRITE, APPEND, READ, CLOSE}type_t;

/*================================== VARIABILI GLOBALI ==================================================*/
Hashtable_t* hashtable = NULL;
Info_t* informations;
int listenfd;
QueueNode_t* clientQueue;
pthread_t* workers;
static int pipefd[2];
int active_threads = 0;
int clients =0;
int n_client_online;
volatile sig_atomic_t term = 0; //FLAG SETTATO DAL GESTORE DEI SEGNALI DI TERMINAZIONE
pthread_mutex_t lockcoda = PTHREAD_MUTEX_INITIALIZER;		
pthread_cond_t notempty = PTHREAD_COND_INITIALIZER;

/*=================================== FUNZIONI UTILI ====================================================*/
void setDefault(Info_t* info){
    info->workers_thread = DEFAULT_WORKERS_THREAD;
    info->max_file = DEFAULT_MAX_FILE;
    info->storage_size = DEFAULT_STORAGE_SIZE;
    strcpy(info->socket_name, DEFAULT_SOCKET_NAME);
}

void cleanup(Info_t* info) {
    unlink(info->socket_name);
}

// ritorno l'indice massimo tra i descrittori attivi
int updatemax(fd_set set, int fdmax) {
    for(int i=(fdmax-1);i>=0;--i)
	if (FD_ISSET(i, &set)) return i;
    assert(1==0);
    return -1;
}

//--------UTILITY PER GESTIONE SERVER----------//

//INSERIMENTO IN TESTA
void insertNode (QueueNode_t** list, int data) {
    //printf("Inserisco in coda\n");
    //fflush(stdout);
    //PRENDO LOCK
    SYSCALL_PTHREAD(pthread_mutex_lock(&lockcoda),"Lock coda");
    QueueNode_t* new = malloc (sizeof(QueueNode_t));
    if (new==NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    new->data = data;
    new->next = *list;

    //INSERISCI IN TESTA
    *list = new;
    //INVIO SIGNAL
    SYSCALL_PTHREAD(pthread_cond_signal(&notempty),"Signal coda");
    //RILASCIO LOCK
    pthread_mutex_unlock(&lockcoda);
    
}

//RIMOZIONE IN CODA 
int removeNode (QueueNode_t** list) {
    //PRENDO LOCK
    SYSCALL_PTHREAD(pthread_mutex_lock(&lockcoda),"Lock coda");
    //ASPETTO CONDIZIONE VERIFICATA 
    while (clientQueue==NULL) {
        pthread_cond_wait(&notempty,&lockcoda);
        //printf("Consumatore Svegliato\n");
        //fflush(stdout);
    }
    int data;
    QueueNode_t* curr = *list;
    QueueNode_t* prev = NULL;
    while (curr->next != NULL) {
        prev = curr;
        curr = curr->next;
    }
    data = curr->data;
    //LO RIMUOVO
    if (prev == NULL) {
        free(curr);
        *list = NULL;
    }else{
        prev->next = NULL;
        free(curr);
    }
    //RILASCIO LOCK
    pthread_mutex_unlock(&lockcoda);
    return data;
}
	size_t datasize;



void* workerFunction(void* args){
    int pfd = *((int*) args);
    int cfd;
    while(1){
        cfd = removeNode(&clientQueue);
        if(cfd == -1) break;
        type_t request;
        int answer;
        CHECK_EQ_RETURN(readn(cfd, &request, sizeof(type_t)), -1, "readn request from client", -1);
        switch (request){
            case OPEN:
                break;
            case OPENC:
                break;
            case CLOSECONN:
                answer = 0;
                CHECK_EQ_RETURN((writen(cfd, &answer, sizeof(int))), -1, "writen answer closeConnection", -1);
                close(cfd);
                break;
            case WRITE:
                break;
            case APPEND:
                break;
            case READ:
                break;
            case CLOSE:
                break;
            default:
                break;
        }
    }
    return 0;
}

static void gestore_term (int signum) {
    if (signum==SIGINT || signum==SIGQUIT) {
        term = 1;
    } else if (signum==SIGHUP) {
        //gestisci terminazione soft 
        term = 2;
    } 
}

/*===================================================== MAIN ==========================================*/
int main(int argc, char* argv[]){
    //--------GESTIONE SEGNALI---------//
    struct sigaction s;
    sigset_t sigset;
    SYSCALL_EXIT(sigfillset(&sigset),"sigfillset");
    SYSCALL_EXIT(pthread_sigmask(SIG_SETMASK,&sigset,NULL),"pthread_sigmask");
    memset(&s,0,sizeof(s));
    s.sa_handler = gestore_term;

    //SYSCALL_EXIT(sigaction(SIGINT,&s,NULL),"sigaction");
    SYSCALL_EXIT(sigaction(SIGQUIT,&s,NULL),"sigaction");
    SYSCALL_EXIT(sigaction(SIGHUP,&s,NULL),"sigaction"); //TERMINAZIONE SOFT

    //ignoro SIGPIPE
    s.sa_handler = SIG_IGN;
    SYSCALL_EXIT(sigaction(SIGPIPE,&s,NULL),"sigaction");

    SYSCALL_EXIT(sigemptyset(&sigset),"sigemptyset");
    SYSCALL_PTHREAD(pthread_sigmask(SIG_SETMASK,&sigset,NULL),"pthread_sigmask");

    /*-----------CONFIGUARZIONE SERVER-----------------*/
    hashtable = HASHTABLE_INIT(NUMBUCKETS);
    informations = initInfo();

    /*caso in cui il valori passati non sono corretti o incompleti*/
    if((argc == 3 && strcmp(argv[1], "-f")) || argc == 2){
        printf("use: %s [-f pathconfigurationfile]\n", argv[0]);
        return EXIT_FAILURE;
    }

    /*caso in cui il file di congifigurazione non è passato*/
    if(argc == 1){
        printf("server default configuration (configuration file is not passed)\n");
        setDefault(informations);
    }
    else{ /*caso corretto*/
        if(parsConfiguration(argv[2], informations) == -1){
            /*se qualcosa va storto nel parsing setto valori di default*/
            fprintf(stderr, "ERROR: configuration file is not parsed correctly, server will be setted with default values\n");
            setDefault(informations);
        }    
    }
    
    //THREAD POOL
    CHECK_EQ_EXIT((workers = (pthread_t*)malloc(sizeof(pthread_t) * informations->workers_thread)), NULL, "Creazione Thread Pool\n");
   
    for (int i=0; i<informations->workers_thread; i++)
        SYSCALL_PTHREAD(pthread_create(&workers[i],NULL,workerFunction,(void*)(&pipefd[1])),"Errore creazione thread pool");

    unlink(informations->socket_name);

    /*---------------------------------CREAZIONE SOCKET-----------------------------------*/
    int max_fd = 0;
    fd_set set;
    fd_set tmpset;
	
    struct sockaddr_un sa;
    memset(&sa, '0', sizeof(sa));
	sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, informations->socket_name, UNIX_PATH_MAX);

    int listenfd;
	CHECK_EQ_EXIT((listenfd = socket(AF_UNIX, SOCK_STREAM, 0)), -1, "socket");
    unlink(informations->socket_name);

	CHECK_EQ_EXIT(bind(listenfd, (struct sockaddr*)&sa, sizeof(sa)), -1, "bind");
	CHECK_EQ_EXIT(listen(listenfd, SOMAXCON), -1, "listen");

    if(listenfd > max_fd) max_fd = listenfd;

    //azzero master set e set temporaneo
    FD_ZERO(&set);
    FD_SET(listenfd, &set);
    printf("%d\n", listenfd);

    //inserisco fd della pipe e del socket nel mastr set
    if(pipefd[0] > max_fd) max_fd = pipefd[0];
    FD_SET(pipefd[0], &set);

	fprintf(stdout, "SERVER: listening...\n");
    while(1){
        tmpset = set;
        if(select(max_fd+1, &tmpset, NULL, NULL, NULL) == -1){
            if (term==1) break;
            else if (term==2) { 
                if (clients==0) break;
                else {
                        printf("Chiusura Soft...\n");
                        FD_CLR(listenfd, &set);
                        if (listenfd == max_fd) max_fd = updatemax(set,max_fd);
                        close(listenfd);
                        tmpset = set;
                        SYSCALL_BREAK(select(max_fd+1,&tmpset,NULL,NULL,NULL),"Errore select"); 
                }
            }else {
                perror("select");
                break;
            }
        }
        //capiamo da chi abbiamo ricevuto una richiesta
        int cfd;
        for(int i=0; i<=max_fd; i++){
            if(FD_ISSET(i, &tmpset)){ 
                if(i == listenfd){ //nuova richiesta di connessione
                    printf("new request from new client\n");
                    CHECK_EQ_EXIT((cfd = accept(listenfd, (struct sockaddr*)NULL, NULL)), -1, "accept");
                    FD_SET(cfd, &set); //aggiungo descrittore al master set
                    if(cfd > max_fd) max_fd = cfd;
                    clients++;
                    printf("new connection accepted: client %d\n", cfd);

                }else if(i == pipefd[0]){ //è una scrittura sulla pipe
                    int fdfrompipe;
                    int len;
                    int flag;
                    if((len = read(pipefd[0], &fdfrompipe, sizeof(fdfrompipe))) > 0){
                        CHECK_EQ_EXIT(read(pipefd[0], &flag, sizeof(flag)), -1, "read pipe");
                        if(flag == -1){
                            printf("closing connection with client\n");
                            FD_CLR(fdfrompipe, &set);
                            if(fdfrompipe == max_fd) max_fd = updatemax(set, max_fd); //se fd rimosso era il max devo trovare un nuovo max
                            close(fdfrompipe); 
                            clients--;
                        }else{
                            FD_SET(fdfrompipe, &set);
                            if(fdfrompipe > max_fd) max_fd = fdfrompipe;
                        }
                    }else if(len == -1){
                        perror("read pipe");
                        exit(EXIT_FAILURE);
                    }
                }else{
                    printf("client pronto in read\n");
                    insertNode(&clientQueue, i);
                    FD_CLR(i, &set);
                }
            }
        }
    }
    for (int i=0;i<informations->workers_thread;i++) {
        SYSCALL_PTHREAD(pthread_join(workers[i],NULL),"Errore join thread");
    }

    return 0;
}