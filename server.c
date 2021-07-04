/**
 * @file server.c
 * @author Chiara Maggi
 * 
 * @copyright Copyright (c) 2021
 * 
 */
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
#define DEFAULT_SOCKET_NAME "./SOLsocket.sk"
#define NUMBUCKETS 100

/*================================== STRUTTURE UTILI ====================================================*/
typedef struct QueueNode{
	int data;
	struct QueueNode* next;
}QueueNode_t;

//campo "data" nella struttura del nodo della hashtable
typedef struct File{
    int fdcreator;
    int operationdonebycreator;
    QueueNode_t* openby;
    void* contenuto;
    int size;
}File_t;

typedef struct Serverstate{
   int num_file;
   size_t used_space;
   size_t free_space;
}Serverstate_t;

typedef struct Statistics{
    int max_saved_file;
    int max_mbytes;
    int switches;
}Statistics_t;

/*================================== VARIABILI GLOBALI ==================================================*/
icl_hash_t* storage_server;
Info_t* informations;
Serverstate_t* serverstate;
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
void setDefault(Info_t* info);
Serverstate_t* initServerstate(long size);
File_t* createFile(char* pathname, int fd);
void cleanup(Info_t* info);
int updatemax(fd_set set, int fdmax); 
void insertNode (QueueNode_t** list, int data);  
int removeNode (QueueNode_t** list);
int findNode(QueueNode_t** list, int data);
void freeFile(File_t* file);
void removeNodeByKey(QueueNode_t** list, int data);
int length(QueueNode_t** list);
static void gestore_term (int signum);
void* workerFunction(void* args);   
int opn(type_t req, int cfd, char pathname[]); 
int wrt(int cfd, char pathname[]);
int rd(int cfd, char pathname[]);
int cls(int cfd, char pathname[]);

/*===================================================== MAIN ==========================================*/
int main(int argc, char* argv[]){
    //--------GESTIONE SEGNALI---------//
    struct sigaction s;
    sigset_t sigset;
    CHECK_EQ_EXIT(sigfillset(&sigset),-1,"sigfillset");
    CHECK_EQ_EXIT(pthread_sigmask(SIG_SETMASK,&sigset,NULL), -1, "pthread_sigmask");
    memset(&s,0,sizeof(s));
    s.sa_handler = gestore_term;

    //SYSCALL_EXIT(sigaction(SIGINT,&s,NULL),"sigaction");
    CHECK_EQ_EXIT(sigaction(SIGQUIT,&s,NULL), -1, "sigaction");
    CHECK_EQ_EXIT(sigaction(SIGHUP,&s,NULL), -1, "sigaction"); //TERMINAZIONE SOFT

    //ignoro SIGPIPE
    s.sa_handler = SIG_IGN;
    CHECK_EQ_EXIT(sigaction(SIGPIPE,&s,NULL), -1, "sigaction");

    CHECK_EQ_EXIT(sigemptyset(&sigset), -1, "sigemptyset");
    SYSCALL_PTHREAD(pthread_sigmask(SIG_SETMASK,&sigset,NULL),"pthread_sigmask");

    /*-----------CONFIGUARZIONE SERVER-----------------*/
    storage_server = icl_hash_create(NUMBUCKETS, hash_pjw, string_compare);
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
    
    serverstate = initServerstate(informations->storage_size);

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
    CHECK_EQ_EXIT(pipe(pipefd), -1, "pipe");

    if(listenfd > max_fd) max_fd = listenfd;

    //azzero master set e set temporaneo
    FD_ZERO(&set);
    FD_SET(listenfd, &set);

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
                    if((len = readn(pipefd[0], &fdfrompipe, sizeof(int))) != -1){
                        //CHECK_EQ_EXIT(read(pipefd[0], &flag, sizeof(flag)), -1, "read pipe");
                        /*if(flag == -1){
                            printf("closing connection with client\n");
                            FD_CLR(fdfrompipe, &set);
                            if(fdfrompipe == max_fd) max_fd = updatemax(set, max_fd); //se fd rimosso era il max devo trovare un nuovo max
                            close(fdfrompipe); 
                            clients--;
                        }else{*/
                            FD_SET(fdfrompipe, &set);
                            if(fdfrompipe > max_fd) max_fd = fdfrompipe;
                       /* }*/
                    }else if(len == -1){
                        perror("read pipe");
                        exit(EXIT_FAILURE);
                    }
                }else{
                    // mettere sotto accept
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

void setDefault(Info_t* info){
    info->workers_thread = DEFAULT_WORKERS_THREAD;
    info->max_file = DEFAULT_MAX_FILE;
    info->storage_size = DEFAULT_STORAGE_SIZE;
    strcpy(info->socket_name, DEFAULT_SOCKET_NAME);
}

Serverstate_t* initServerstate(long size){
    Serverstate_t* serverstate = malloc(sizeof(Serverstate_t));
    serverstate->num_file = 0;
    serverstate->free_space = size;
    serverstate->used_space = 0;
    return serverstate;
}

File_t* createFile(char* pathname, int fd){
    File_t* file = malloc(sizeof(File_t));
    file->fdcreator = fd;
    file->operationdonebycreator++;
    insertNode(&file->openby, fd);
    file->contenuto = NULL;
    file->size = 0;
    return file;
}

void freeFile(File_t* file){
    free(file->contenuto);
    QueueNode_t* curr = file->openby;
    QueueNode_t* prec;
    while(curr != NULL){
        prec = curr;
        curr = curr->next;
        free(prec);
    }
    free(file);
}

void cleanup(Info_t* info) {
    unlink(info->socket_name);
}

int updatemax(fd_set set, int fdmax) {
    for(int i=(fdmax-1);i>=0;--i)
	if (FD_ISSET(i, &set)) return i;
    assert(1==0);
    return -1;
}

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

int findNode (QueueNode_t** list, int data){
    SYSCALL_PTHREAD(pthread_mutex_lock(&lockcoda),"Lock coda");
    QueueNode_t* curr = *list;
    while (curr != NULL) {
        if(curr->data == data){
            pthread_mutex_unlock(&lockcoda);
            return 0;
        }
        curr = curr->next;
    }
    //RILASCIO LOCK
    pthread_mutex_unlock(&lockcoda);
    return -1;
}

void removeNodeByKey(QueueNode_t** list, int data){
    //PRENDO LOCK
    SYSCALL_PTHREAD(pthread_mutex_lock(&lockcoda),"Lock coda");
    //ASPETTO CONDIZIONE VERIFICATA 
    QueueNode_t* curr = *list;
    QueueNode_t* prev = NULL;
    while (curr != NULL) {
        if(curr->data == data){
            if(prev == NULL){
                *list = curr->next;
            }else prev->next = curr->next;
            free(curr);
            pthread_mutex_unlock(&lockcoda);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
    //RILASCIO LOCK
    pthread_mutex_unlock(&lockcoda);
}

int length(QueueNode_t** list){
    SYSCALL_PTHREAD(pthread_mutex_lock(&lockcoda),"Lock coda");
    QueueNode_t* curr = *list;
    int c = 0;
    while(curr != NULL){
        c++;
        curr = curr->next;
    }
    pthread_mutex_unlock(&lockcoda);
    return c;
}

static void gestore_term (int signum) {
    if (signum==SIGINT || signum==SIGQUIT) {
        term = 1;
    } else if (signum==SIGHUP) {
        //gestisci terminazione soft 
        term = 2;
    } 
}

void* workerFunction(void* args){
    int cfd;
    while(1){
        cfd = removeNode(&clientQueue);
        if(cfd == -1) break;
        request_t request;
        int answer;
        if(readn(cfd, &request, sizeof(request_t)) == -1){
            request.req = -1;
        }
        printf("REQUEST: %d eseguita da worker %lu\n", request.req, pthread_self());
        switch (request.req){
            case OPEN:
                printf("APERTURA FILE\n");
                opn(OPEN, cfd, request.pathname);
                break;
            case OPENC:
                printf("CREAZIONE FILE\n");
                answer = opn(OPENC, cfd, request.pathname);
                break;
            case CLOSECONN:
                printf("CHIUSURA CONNESSIONE\n");
                writen(cfd, &answer, sizeof(int));
                close(cfd);
                break;
            case WRITE:
                printf("SCRITTURA FILE\n");
                answer = wrt(cfd, request.pathname);
                break;
            case APPEND:
                break;
            case READ:
                printf("LETTURA FILE\n");
                answer = rd(cfd, request.pathname);
                break;
            case CLOSE:
                printf("CHIUSURA FILE\n");
                answer = cls(cfd, request.pathname);
                break;
            default:
                fprintf(stderr, "invalid request\n");
                break;
        }
        if(answer == -1) fprintf(stderr, "impossible to satisfy the request %d\n", request.req);
        if(request.req != CLOSECONN){
            writen(pipefd[1], &cfd, sizeof(int));
        }
    }
    return 0;
}

int opn(type_t req, int cfd, char pathname[]){
    int answer = 0;
    File_t* tmp;
    if(req == OPENC){
        if((tmp = icl_hash_find(storage_server, pathname)) != NULL){
            fprintf(stderr,"impossible to satisfy the request, file still in the storage\n");
            answer = -1;
        }else{
            if(serverstate->num_file < informations->max_file){
                File_t* file = createFile(pathname, cfd);
                icl_hash_insert(storage_server, pathname, file);
                //printf("%d\n", storage_server->nentries);
                serverstate->num_file++;
            }else{
                //politica di rimpiazzo
            }
        }
    }
    if(req == OPEN){
        if((tmp = icl_hash_find(storage_server, pathname)) == NULL){
            fprintf(stderr, "impossible to satisfy the request, file not present in the storage\n");
            answer = -1;
        }
        else{
            insertNode(&tmp->openby, cfd);
        }
    }
    CHECK_EQ_RETURN(writen(cfd, &answer, sizeof(int)), -1, "writen opn", -1);
    return 0;
}

int wrt(int cfd, char pathname[]){
    int answer = 0;

    int filesize;
    CHECK_EQ_RETURN(readn(cfd, &filesize, sizeof(int)), -1, "readn wrt", -1);

    char* filebuffer = malloc((filesize+1)*sizeof(char));
    if(!filebuffer){
        perror("malloc");
        return -1;
    }

    CHECK_EQ_RETURN(readn(cfd, filebuffer, filesize+1), -1, "readn wrt", -1);
    //printf("%s", filebuffer);
    File_t* tmp;
    if((tmp = icl_hash_find(storage_server, pathname)) != NULL){
        int flag = findNode(&tmp->openby, cfd);
        if(tmp->fdcreator == cfd && tmp->operationdonebycreator == 1 && flag == 0){ //controllo che sia il creatore e abbia fatto come ultima operazione openFile con O_CREATE
            if(serverstate->free_space >= filesize){
                tmp->size = filesize;
                tmp->contenuto = malloc(filesize*sizeof(char));
                strcpy(tmp->contenuto, filebuffer);
                tmp->operationdonebycreator++;

                serverstate->free_space = serverstate->free_space - filesize;
                serverstate->used_space = serverstate->used_space + filesize;
            }
            else{
                //politica di rimpiazzo
            }
        }else{
            fprintf(stderr, "operation not authorized for client %d\n", cfd);
            answer = -1;
        }
    }else{
        fprintf(stderr, "file not found in the storage\n");
        answer = -1;
    }
    CHECK_EQ_RETURN(writen(cfd, &answer, sizeof(int)), -1, "writen wrt", -1);
    return 0;
}

int cls(int cfd, char pathname[]){
    int answer = 0;
    File_t* tmp;
    if((tmp = icl_hash_find(storage_server, pathname))== NULL){
        fprintf(stderr,"file not present\n");
        answer = -1;
    }else{
        if(findNode(&tmp->openby, cfd) == -1){
            fprintf(stderr, "the client %d doesnt't have the file open\n", cfd);
            answer = -1;
        }else{
            removeNodeByKey(&tmp->openby, cfd);
        }
    }
    CHECK_EQ_RETURN(writen(cfd, &answer, sizeof(int)), -1, "writen cls", -1);
    return 0;
}

int rd(int cfd, char pathname[]){
    int answer = 0;
    File_t* tmp;
    int size = -1;
    if((tmp = icl_hash_find(storage_server, pathname) )!= NULL){
        if(findNode(&tmp->openby, cfd) == 0){
            size = tmp->size;
        }
        else answer = -1;
    }else answer = -1;
    CHECK_EQ_RETURN(writen(cfd, &size, sizeof(int)), -1, "writen rd", -1);
    CHECK_EQ_RETURN(writen(cfd, tmp->contenuto, tmp->size), -1, "writen rd", -1);
    return answer;
}