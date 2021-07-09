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
#include<libgen.h>

#include "parsing.h"
#include "utils.h"
#include "icl_hash.h"


#define DEFAULT_WORKERS_THREAD 10
#define DEFAULT_MAX_FILE 100
#define DEFAULT_STORAGE_SIZE 512
#define DEFAULT_SOCKET_NAME "./SOLsocket.sk"
#define DEFAULT_NUM_BUCKETS 100

/*================================== STRUTTURE UTILI ====================================================*/
typedef struct queueNode{
	int data;
	struct queueNode* next;
}queueNode_t;

//campo "data" nella struttura del nodo della hashtable
typedef struct file{
    int fdcreator;
    int operationdonebycreator;
    queueNode_t* openby;
    char* contents;
    int size;
}file_t;

typedef struct serverstate{
   int num_file;
   size_t used_space;
   size_t free_space;
}serverstate_t;

typedef struct statistics{
    int max_saved_file;
    int max_mbytes;
    int switches;
}statistics_t;

/*================================== VARIABILI GLOBALI ==================================================*/
Hashtable_t* storage_server;
Info_t* info;
serverstate_t* storage_state;
int listenfd;
queueNode_t* clientQueue;
pthread_t* workers;
static int pipefd[2];
int active_threads = 0;
int clients =0;
int n_client_online;
volatile sig_atomic_t term = 0; //FLAG SETTATO DAL GESTORE DEI SEGNALI DI TERMINAZIONE
pthread_cond_t notempty = PTHREAD_COND_INITIALIZER;
pthread_mutex_t queueclientmtx = PTHREAD_MUTEX_INITIALIZER;	
pthread_mutex_t servermtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t serverstatemtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t serverfilemtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t serverbucketsmtx = PTHREAD_MUTEX_INITIALIZER;

/*=================================== FUNZIONI UTILI ====================================================*/
void setDefault(Info_t* info);
serverstate_t* initServerstate(long size);
file_t* createFile(int fd);
void cleanup(Info_t* info);
int updatemax(fd_set set, int fdmax); 
void insertNode (queueNode_t** list, int data);  
int removeNode (queueNode_t** list);
int findNode(queueNode_t** list, int data);
void freeFile(void* file);
void removeNodeByKey(queueNode_t** list, int data);
int length(queueNode_t** list);
static void gestore_term (int signum);
void* workerFunction(void* args);   
int opn(type_t req, int cfd, char pathname[]); 
int wrt(int cfd, char pathname[]);
int rd(int cfd, char pathname[]);
int cls(int cfd, char pathname[]);
int rm(int cfd, char pathname[]);
int rdn(int cfd, char info[]);

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
    info = initInfo();

    /*caso in cui il valori passati non sono corretti o incompleti*/
    if((argc == 3 && strcmp(argv[1], "-f")) || argc == 2){
        printf("use: %s [-f pathconfigurationfile]\n", argv[0]);
        return EXIT_FAILURE;
    }

    /*caso in cui il file di congifigurazione non è passato*/
    if(argc == 1){
        printf("server default configuration (configuration file is not passed)\n");
        setDefault(info);
    }
    else{ /*caso corretto*/
        if(parsConfiguration(argv[2], info) == -1){
            /*se qualcosa va storto nel parsing setto valori di default*/
            fprintf(stderr, "ERROR: configuration file is not parsed correctly, server will be setted with default values\n");
            setDefault(info);
        }    
    }
    
    storage_server = hashtableInit(info->num_buckets, hashFunction, hashCompare);
    storage_state = initServerstate(info->storage_size);

    //THREAD POOL
    CHECK_EQ_EXIT((workers = (pthread_t*)malloc(sizeof(pthread_t) * info->workers_thread)), NULL, "Creazione Thread Pool\n");
   
    for (int i=0; i<info->workers_thread; i++)
        SYSCALL_PTHREAD(pthread_create(&workers[i],NULL,workerFunction,(void*)(&pipefd[1])),"Errore creazione thread pool");

    unlink(info->socket_name);

    /*---------------------------------CREAZIONE SOCKET-----------------------------------*/
    int max_fd = 0;
    fd_set set;
    fd_set tmpset;
	
    struct sockaddr_un sa;
    memset(&sa, '0', sizeof(sa));
	sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, info->socket_name, UNIX_PATH_MAX);

    int listenfd;
	CHECK_EQ_EXIT((listenfd = socket(AF_UNIX, SOCK_STREAM, 0)), -1, "socket");
    unlink(info->socket_name);

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
    for (int i=0;i<info->workers_thread;i++) {
        SYSCALL_PTHREAD(pthread_join(workers[i],NULL),"Errore join thread");
    }

    return 0;
}

void setDefault(Info_t* info){
    info->workers_thread = DEFAULT_WORKERS_THREAD;
    info->max_file = DEFAULT_MAX_FILE;
    info->storage_size = DEFAULT_STORAGE_SIZE;
    strcpy(info->socket_name, DEFAULT_SOCKET_NAME);
    info->num_buckets = DEFAULT_NUM_BUCKETS;
}

serverstate_t* initServerstate(long size){
    serverstate_t* serverstate = malloc(sizeof(serverstate_t));
    serverstate->num_file = 0;
    serverstate->free_space = size;
    serverstate->used_space = 0;
    return serverstate;
}

file_t* createFile(int fd){
    file_t* file = malloc(sizeof(file_t));
    file->fdcreator = fd;
    file->operationdonebycreator++;
    insertNode(&file->openby, fd);
    file->contents = NULL;
    file->size = 0;
    return file;
}

void freeFile(void* file){
    file_t* data = file;
    free(data->contents);
    queueNode_t* curr = data->openby;
    queueNode_t* prec;
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

void insertNode (queueNode_t** list, int data) {
    //PRENDO LOCK
    SYSCALL_PTHREAD(pthread_mutex_lock(&queueclientmtx),"Lock coda");
    queueNode_t* new = malloc (sizeof(queueNode_t));
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
    pthread_mutex_unlock(&queueclientmtx);
    
}

int removeNode (queueNode_t** list) {
    //PRENDO LOCK
    SYSCALL_PTHREAD(pthread_mutex_lock(&queueclientmtx),"Lock coda");
    //ASPETTO CONDIZIONE VERIFICATA 
    while (clientQueue==NULL) {
        pthread_cond_wait(&notempty,&queueclientmtx);
    }
    int data;
    queueNode_t* curr = *list;
    queueNode_t* prev = NULL;
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
    pthread_mutex_unlock(&queueclientmtx);
    return data;
}

int findNode (queueNode_t** list, int data){
    SYSCALL_PTHREAD(pthread_mutex_lock(&queueclientmtx),"Lock coda");
    queueNode_t* curr = *list;
    while (curr != NULL) {
        if(curr->data == data){
            pthread_mutex_unlock(&queueclientmtx);
            return 0;
        }
        curr = curr->next;
    }
    //RILASCIO LOCK
    pthread_mutex_unlock(&queueclientmtx);
    return -1;
}

void removeNodeByKey(queueNode_t** list, int data){
    //PRENDO LOCK
    SYSCALL_PTHREAD(pthread_mutex_lock(&queueclientmtx),"Lock coda");
    //ASPETTO CONDIZIONE VERIFICATA 
    queueNode_t* curr = *list;
    queueNode_t* prev = NULL;
    while (curr != NULL) {
        if(curr->data == data){
            if(prev == NULL){
                *list = curr->next;
            }else prev->next = curr->next;
            free(curr);
            pthread_mutex_unlock(&queueclientmtx);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
    //RILASCIO LOCK
    pthread_mutex_unlock(&queueclientmtx);
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
        printf("REQUEST %d: ", request.req);
        switch (request.req){
            case OPEN:
                printf("APERTURA FILE\n");
                opn(OPEN, cfd, request.info);
                break;
            case OPENC:
                printf("CREAZIONE FILE\n");
                answer = opn(OPENC, cfd, request.info);
                break;
            case CLOSECONN:
                printf("CHIUSURA CONNESSIONE\n");
                writen(cfd, &answer, sizeof(int));
                close(cfd);
                break;
            case WRITE:
                printf("SCRITTURA FILE\n");
                answer = wrt(cfd, request.info);
                break;
            case APPEND:
                break;
            case READ:
                printf("LETTURA FILE\n");
                answer = rd(cfd, request.info);
                break;
            case CLOSE:
                printf("CHIUSURA FILE\n");
                answer = cls(cfd, request.info);
                break;
            case REMOVE:
                printf("RIMOZIONE FILE\n");
                answer = rm(cfd, request.info);
                break;
            case READN:
                printf("LETTURA N FILE\n");
                answer = rdn(cfd, request.info);
                break;
            default:
                fprintf(stderr, "invalid request\n");
                break;
        }
        if(answer != 0) fprintf(stderr, "impossible to satisfy the request %d\n", request.req);
        if(request.req != CLOSECONN){
            writen(pipefd[1], &cfd, sizeof(int));
        }
        hashtablePrint(storage_server);
        printf("\n");
    }
    return 0;
}

int opn(type_t req, int cfd, char pathname[]){
    int answer = 0;
    file_t* tmp;
    if(req == OPENC){
        if((tmp = hashtableFind(storage_server, pathname)) != NULL){
            fprintf(stderr,"file still in the storage\n");
            answer = -3; //EEXIST
        }else{
            if(storage_state->num_file < info->max_file){
                file_t* file = createFile(cfd);
                hashtableInsert(storage_server, pathname, strlen(pathname)*sizeof(char), file, sizeof(file_t));
                storage_state->num_file++;
            }else{
                //politica di rimpiazzo
            }
        }
    }
    if(req == OPEN){
        if((tmp = hashtableFind(storage_server, pathname)) == NULL){
            fprintf(stderr, "file not present in the storage\n");
            answer = -1; //ENOENT
        }
        else{
            insertNode(&tmp->openby, cfd);
        }
    }   
    CHECK_EQ_RETURN(writen(cfd, &answer, sizeof(int)), -1, "writen opn", -1);
    return answer;
}

int wrt(int cfd, char pathname[]){
    int answer = 0;
    int filesize;
    CHECK_EQ_EXIT(readn(cfd, &filesize, sizeof(int)), -1, "readn wrt");

    char* filebuffer = malloc((filesize+1)*sizeof(char));
    if(!filebuffer){
        perror("malloc");
        return -1;
    }
    CHECK_EQ_RETURN(readn(cfd, filebuffer, filesize+1), -1, "readn wrt", -1);
    //printf("%s", filebuffer);
    file_t* tmp;
    if((tmp = hashtableFind(storage_server, pathname)) != NULL){
        int flag = findNode(&tmp->openby, cfd);
        if(tmp->fdcreator == cfd && tmp->operationdonebycreator == 1 && flag == 0){ //controllo che sia il creatore e abbia fatto come ultima operazione openFile con O_CREATE
            if(storage_state->free_space >= filesize){
                tmp->size = filesize;
                tmp->contents = malloc((filesize+1)*sizeof(char));
                strcpy(tmp->contents, filebuffer);
                tmp->operationdonebycreator++;

                storage_state->free_space -=  filesize;
                storage_state->used_space += filesize;
            }
            else{
                //politica di rimpiazzo
            }
        }else{
            fprintf(stderr, "operation not authorized for client %d\n", cfd);
            answer = -2; //EPERM
        }
    }else{
        fprintf(stderr, "file not found in the storage\n");
        answer = -1; //ENOENT
    }
    CHECK_EQ_RETURN(writen(cfd, &answer, sizeof(int)), -1, "writen wrt", -1);
    return answer;
}

int cls(int cfd, char pathname[]){
    int answer = 0;
    file_t* tmp;
    if((tmp = hashtableFind(storage_server, pathname))== NULL){
        fprintf(stderr,"file not present\n");
        answer = -1; //ENOENT
    }else{
        if(findNode(&tmp->openby, cfd) == -1){
            fprintf(stderr, "the client %d doesnt't have the file open\n", cfd);
            answer = -2; //EPERM
        }else{
            removeNodeByKey(&tmp->openby, cfd);
        }
    }
    CHECK_EQ_RETURN(writen(cfd, &answer, sizeof(int)), -1, "writen cls", -1);
    return answer;
}

int rd(int cfd, char pathname[]){
    file_t* tmp;
    int answer = 0;
    int size = -1;
    if((tmp = hashtableFind(storage_server, pathname) )!= NULL){
        if(findNode(&tmp->openby, cfd) == 0){
            size = tmp->size;
        }
        else answer = -2; //EPERM
    }else answer = -1; //ENOENT
   
    CHECK_EQ_EXIT(writen(cfd, &answer, sizeof(int)), -1, "writen rd");
    if(answer != 0) return answer;

    printf("%d\n", size);
    CHECK_EQ_EXIT(writen(cfd, &size, sizeof(int)), -1, "writen rd");
    CHECK_EQ_EXIT(writen(cfd, tmp->contents, tmp->size), -1, "writen rd");
    return 0;
}

int rm(int cfd, char pathname[]){
    int answer = 0;
    file_t* tmp;
    
    if((tmp = hashtableFind(storage_server, pathname)) == NULL){
        fprintf(stderr,"file not present\n");
        answer = -1; //ENOENT
    }else{
        int filesize = tmp->size;
        hashtableDeleteNode(storage_server, pathname);
        
        storage_state->num_file--;
        storage_state->used_space = storage_state->used_space - filesize;
        storage_state->free_space = storage_state->free_space + filesize;
    }
    CHECK_EQ_RETURN(writen(cfd, &answer, sizeof(int)), -1, "writen rm", -1);
    return answer;
}

int rdn(int cfd, char info[]){
    int n = atoi(info);
    if(n > storage_state->num_file || n == 0) n = storage_state->num_file;
    CHECK_EQ_EXIT(writen(cfd, &n, sizeof(int)), -1, "writen rdn");
    Node_t *bucket, *curr;
    int i = 0;
    while(i<storage_server->numbuckets && n > 0){
        bucket = storage_server->buckets[i];
        for(curr=bucket; curr!=NULL; curr=curr->next){
            int len = strlen(bucket->key)+1;
            CHECK_EQ_EXIT(writen(cfd, &len, sizeof(int)), -1, "writen rdn");
            CHECK_EQ_EXIT(writen(cfd, (char*)bucket->key, len*sizeof(char)), -1, "writen rdn");
            
            file_t* tmp = bucket->data;
            CHECK_EQ_EXIT(writen(cfd, &tmp->size, sizeof(int)), -1, "writen rdn");
            CHECK_EQ_EXIT(writen(cfd, tmp->contents, tmp->size), -1, "writen rdn");
            n--;
        }
        i++;
    }
    return 0;
}