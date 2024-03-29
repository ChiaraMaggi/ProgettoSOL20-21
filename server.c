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
#include<math.h>
#include<linux/limits.h>

#include "parsing.h"
#include "utils.h"
#include "icl_hash.h"

#define DEFAULT_WORKERS_THREAD 10
#define DEFAULT_MAX_FILE 100
#define DEFAULT_STORAGE_SIZE 512
#define DEFAULT_SOCKET_NAME "./SOLsocket.sk"
#define DEFAULT_NUM_BUCKETS 100
#define MAX_OPEN 40

/*================================== STRUTTURE UTILI ====================================================*/
typedef struct queueNode{
	int data;
	struct queueNode* next;
}queueNode_t;

//campo "data" nella struttura del nodo della hashtable
typedef struct file{
    int fdcreator;
    long index; //indice per gestione politica di rimpiazzo
    int openby[MAX_OPEN];
    pthread_mutex_t filemtx;
    char* contents;
    int size;
}file_t;

//memorizza lo stato del server durante tutta l'esecuzione
typedef struct serverstate{
   int num_file;
   int free_space;
   int used_space;
}serverstate_t;

//struttura per raccogliere le statistiche del server
typedef struct statistics{
    int max_saved_file;
    int max_bytes;
    int switches;
}statistics_t;

/*================================== VARIABILI GLOBALI ==================================================*/
Info_t* info;
Hashtable_t* cache;
pthread_t* workers;
queueNode_t* clientQueue;
statistics_t* statistics;
serverstate_t* cache_state;
static int pipefd[2];
int clients = 0; //numero di client attivi
long count = 0; //contatore per gestire la poltica di rimpiazzo
int max_fd = 0; //massimo fd
fd_set set;
fd_set tmpset;
volatile sig_atomic_t term = 0; //FLAG SETTATO DAL GESTORE DEI SEGNALI DI TERMINAZIONE
pthread_cond_t notempty = PTHREAD_COND_INITIALIZER;
pthread_mutex_t queueclientmtx = PTHREAD_MUTEX_INITIALIZER;	
pthread_mutex_t cachemtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cachestatemtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t indexmtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t statisticsmtx = PTHREAD_MUTEX_INITIALIZER;


/*=================================== FUNZIONI UTILI ====================================================*/
serverstate_t* initServerstate(int size);
statistics_t* initStatistics();
file_t* createFile(int fd);
void setDefault(Info_t* info);
void insertNode (queueNode_t** list, int data);  
void freeFile(void* f);
void freeList(queueNode_t** list);
void removeNodeByKey(queueNode_t** list, int data);
void getMinIndex(Hashtable_t* hashtable, char* key);
void removevalue(int a[], int val);
static void gestore_term (int signum);
int findvalue(int a[], int val);
int updatemax(fd_set set, int fdmax); 
int removeNode (queueNode_t** list);
int findNode(queueNode_t** list, int data);
void* workerFunction(void* args);   
int opn(type_t req, int cfd, char pathname[]); 
int wrt(int cfd, char pathname[]);
int cls(int cfd, char pathname[]);
int rd(int cfd, char pathname[]);
int rm(int cfd, char pathname[]);
int rdn(int cfd, char info[]);
int app(int cfd, char pathname[]);

/*===================================================== MAIN ==========================================*/
int main(int argc, char* argv[]){
    //--------GESTIONE SEGNALI---------//
    struct sigaction s;
    s.sa_handler = gestore_term;
    sigemptyset(&s.sa_mask);
    s.sa_flags = SA_RESTART;
    sigaction(SIGINT, &s, NULL);
    sigaction(SIGQUIT, &s, NULL);
    sigaction(SIGHUP, &s, NULL);
    sigaction(SIGPIPE, &s, NULL);

    //-----------CONFIGUARZIONE SERVER--------------//
    info = initInfo();
    /*caso in cui il valori passati non sono corretti o incompleti*/
    if((argc == 3 && strcmp(argv[1], "-f")) || argc == 2){
        printf("[SERVER] use: %s [-f pathconfigurationfile]\n", argv[0]);
        return EXIT_FAILURE;
    }
    /*caso in cui il file di congifigurazione non è passato*/
    if(argc == 1){
        printf("[SERVER] default configuration (configuration file is not passed)\n");
        setDefault(info);
    }
    else{ /*caso corretto*/
        if(parsConfiguration(argv[2], info) == -1){
            /*se qualcosa va storto nel parsing setto valori di default*/
            fprintf(stderr, "[SERVER] ERROR: configuration file is not parsed correctly, server will be setted with default values\n");
            setDefault(info);
        }    
    }
    
    //inizializzazioni strutture 
    info->storage_size = info->storage_size*1000000;
    cache = hashtableInit(info->num_buckets, hashFunction, hashCompare);
    cache_state = initServerstate(info->storage_size);
    statistics = initStatistics();

    //inizializzazione e creazione thread pool
    CHECK_EQ_EXIT((workers = (pthread_t*)malloc(sizeof(pthread_t) * info->workers_thread)), NULL, "Creazione Thread Pool\n");
   
    for (int i=0; i<info->workers_thread; i++)
        SYSCALL_PTHREAD(pthread_create(&workers[i],NULL,workerFunction,(void*)(&pipefd[1])),"Errore creazione thread pool");

    unlink(info->socket_name);

    //-------------------CREAZIONE SOCKET---------------------//
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

	fprintf(stdout, "[SERVER] listening...\n");
    while(1){
        tmpset = set;
        if(select(max_fd+1, &tmpset, NULL, NULL, NULL) == -1){
            if (term==1) break;
            else if (term==2) { 
                if (clients==0) break;
                else {
                    printf("\n[SERVER] Chiusura Soft...\n");
                    FD_CLR(listenfd, &set);
                    if (listenfd == max_fd) max_fd = updatemax(set,max_fd);
                    tmpset = set;
                    for (int i=0;i<info->workers_thread;i++) {
                        insertNode(&clientQueue, -1);
                    }
                    break;
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
                    //printf("[SERVER] New request from new client\n");
                    if((cfd = accept(listenfd, (struct sockaddr*)NULL, NULL)) == -1){
                        if(term == 1) break;
                        else if(term == 2){
                            if(clients == 0) break;
                        }else perror("Error accept client");
                    }
                    FD_SET(cfd, &set); //aggiungo descrittore al master set
                    if(cfd > max_fd) max_fd = cfd;
                    clients++;
                    printf("\n[SERVER] New connection accepted: client %d\n", cfd);

                }else if(i == pipefd[0]){ //è una scrittura sulla pipe
                    int fdfrompipe;
                    int len;
                    if((len = readn(pipefd[0], &fdfrompipe, sizeof(int))) != -1){
                        FD_SET(fdfrompipe, &set);
                        if(fdfrompipe > max_fd) max_fd = fdfrompipe;
                    }else if(len == -1){
                        perror("read pipe");
                        exit(EXIT_FAILURE);
                    }
                }else{
                    //client con una richiesta
                    insertNode(&clientQueue, i);
                    FD_CLR(i, &set);
                }
            }
        }
    }
    
    if(term == 2){
        for (int i=0;i<info->workers_thread;i++) {
            SYSCALL_PTHREAD(pthread_join(workers[i],NULL),"Errore join thread");
        }
    }

    printf("\n-------------STATISTICS SERVER--------------\n");
    printf("Max number of file saved: %d\n",statistics->max_saved_file);
    printf("Max size of Mbytes saved: %f\n",(statistics->max_bytes/pow(10,6)));
    printf("Number of swithes from the replace policy: %d\n",statistics->switches);
    hashtablePrint(cache);
    printf("---------------------------------------------\n");

    if (close(listenfd)==-1) perror("close");
    remove(info->socket_name);
    hashtableFree(cache, freeFile);
    freeList(&clientQueue);
    free(workers);
    
    return 0;
}

//funzione che inizializza lo stato del server
serverstate_t* initServerstate(int size){
    serverstate_t* serverstate = malloc(sizeof(serverstate_t));
    serverstate->num_file = 0;
    serverstate->free_space = size;
    serverstate->used_space = 0;
    return serverstate;
}

//funzione che inizializza le statistiche
statistics_t* initStatistics(){
    statistics_t* statistics = malloc(sizeof(statistics_t));
    statistics->max_bytes = 0;
    statistics->max_saved_file = 0;
    statistics->switches = 0;
    return statistics;
}

//funzione che crea un file
file_t* createFile(int fd){
    file_t* file = malloc(sizeof(file_t));
    file->fdcreator = fd;
    LOCK(&indexmtx);
    file->index = count++;
    UNLOCK(&indexmtx);
    pthread_mutex_init(&file->filemtx, NULL);
    for(int i=0; i<MAX_OPEN; i++) file->openby[i] = 0;
    file->openby[0] = fd;
    file->contents = NULL;
    file->size = 0;
    return file;
}

//funzione che setta valori di default se il file di configurazione non è passato
void setDefault(Info_t* info){
    info->workers_thread = DEFAULT_WORKERS_THREAD;
    info->max_file = DEFAULT_MAX_FILE;
    info->storage_size = DEFAULT_STORAGE_SIZE;
    strcpy(info->socket_name, DEFAULT_SOCKET_NAME);
    info->num_buckets = DEFAULT_NUM_BUCKETS;
}

//push di un nodo del tipo queueNode_t in una lista
void insertNode (queueNode_t** list, int data) {
    //PRENDO LOCK
    LOCK(&queueclientmtx);
    queueNode_t* new = malloc (sizeof(queueNode_t));
    if (new==NULL) {
        UNLOCK(&queueclientmtx);
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    new->data = data;
    new->next = *list;

    //INSERISCI IN TESTA
    *list = new;
    //INVIO SIGNAL
    SIGNAL(&notempty);
    //RILASCIO LOCK
    UNLOCK(&queueclientmtx);
    
}

//libera memoria occupata da un file
void freeFile(void* f){
    file_t* file = f;
    free(file->contents);
    free(file);
}

//libera memoria occupata da una queuelist
void freeList(queueNode_t** head) {
    if(*head == NULL) return;
    queueNode_t* tmp;
    queueNode_t * curr = *head;
    while (curr != NULL) {
        tmp = curr;
        curr = curr->next;
        free(tmp);
    }
    free(curr);
}

//rimuove un nodo del tipo queueNode_t  con un certo valore da una lista
void removeNodeByKey(queueNode_t** list, int data){
    //PRENDO LOCK
    LOCK(&queueclientmtx);

    queueNode_t* curr = *list;
    queueNode_t* prev = NULL;
    while (curr != NULL) {
        if(curr->data == data){
            if(prev == NULL){
                *list = curr->next;
            }else prev->next = curr->next;
            free(curr);
            UNLOCK(&queueclientmtx);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
    //RILASCIO LOCK
    UNLOCK(&queueclientmtx);
}

//funzione per gestire la politica fifo che cerca il minor indice nella hashtable
void getMinIndex(Hashtable_t* hashtable, char* key){
    Node_t *bucket, *curr;
    long min = LONG_MAX;
    for(int i=0; i<hashtable->numbuckets; i++) {
        bucket = hashtable->buckets[i];
        for(curr=bucket; curr!=NULL; curr=curr->next) {
            file_t* curr = bucket->data;
            if(curr->index < min){
                min = curr->index;
                strcpy(key, (char*)bucket->key);
            }
        }
    }
    for(int i=0; i<hashtable->numbuckets; i++) {
        bucket = hashtable->buckets[i];
        for(curr=bucket; curr!=NULL; curr=curr->next) {
            file_t* curr = bucket->data;
            curr->index--;
        }
    }
}

//rimuove un certo valore da un array
void removevalue(int a[], int val){
    int j = MAX_OPEN;
    while(j == 0 && j > 0) j--;
    for(int i=0; i<MAX_OPEN; i++){
        if(a[i] == val){
            a[i] = a[j];
            a[j] = 0;
            return;
        }
    }
}

//gestore della terminazione
static void gestore_term (int signum) {
    if (signum==SIGINT || signum==SIGQUIT) {
        term = 1;
    } else if (signum==SIGHUP) {
        //gestisci terminazione soft 
        term = 2;
    } 
}

//trova un certo valore in un array
int findvalue(int a[], int val){
    for(int i=0; i<MAX_OPEN; i++){
        if(a[i] == val) return 0;
    }
    return -1;
}

//funzione che monitora il valore massimo nel master set
int updatemax(fd_set set, int fdmax) {
    for(int i=(fdmax-1);i>=0;--i)
	if (FD_ISSET(i, &set)) return i;
    assert(1==0);
    return -1;
}

//pop di nodo del tipo queueNode_t da una lista
int removeNode (queueNode_t** list) {
    //PRENDO LOCK
    LOCK(&queueclientmtx);
    //ASPETTO CONDIZIONE VERIFICATA 
    while (clientQueue==NULL) {
        WAIT(&notempty,&queueclientmtx);
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
    UNLOCK(&queueclientmtx);
    return data;
}

//inserisce un nodo del tipo queueNode_t in una lista
int findNode (queueNode_t** list, int data){
    LOCK(&queueclientmtx);
    queueNode_t* curr = *list;
    while (curr != NULL) {
        if(curr->data == data){
            UNLOCK(&queueclientmtx);
            return 0;
        }
        curr = curr->next;
    }
    //RILASCIO LOCK
    UNLOCK(&queueclientmtx);
    return -1;
}

//funzione chiamata dal thread worker
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
        //printf("[SERVER] REQUEST %d from client %d: ", request.req, cfd);
        switch (request.req){
            case OPEN:
                //printf("APERTURA FILE %s\n", request.info);
                opn(OPEN, cfd, request.info);
                break;
            case OPENC:
                //printf("CREAZIONE FILE %s\n", request.info);
                answer = opn(OPENC, cfd, request.info);
                break;
            case CLOSECONN:
                //printf("CHIUSURA CONNESSIONE\n");
                writen(cfd, &answer, sizeof(int));
                if(cfd == max_fd) max_fd = updatemax(set, max_fd); //se fd rimosso era il max devo trovare un nuovo max
                close(cfd);
                break;
            case WRITE:
                //printf("SCRITTURA FILE %s\n", request.info);
                answer = wrt(cfd, request.info);
                break;
            case APPEND:
                answer = app(cfd, request.info);
                break;
            case READ:
                //printf("LETTURA FILE %s\n", request.info);
                answer = rd(cfd, request.info);
                break;
            case CLOSE:
                //printf("CHIUSURA FILE %s\n", request.info);
                answer = cls(cfd, request.info);
                break;
            case REMOVE:
                //printf("RIMOZIONE FILE %s\n", request.info);
                answer = rm(cfd, request.info);
                break;
            case READN:
                //printf("LETTURA N FILE\n");
                answer = rdn(cfd, request.info);
                break;
            default:
                fprintf(stderr, "[SERVER] Invalid request\n");
                break;
        }
        //if(answer != 0) fprintf(stderr, "[SERVER] Impossible to satisfy the request %d\n", request.req);
        if(request.req != CLOSECONN){
            writen(pipefd[1], &cfd, sizeof(int));
        }
    }
    return 0;
}

//funzione chiamata quando la richiesta è una apertura o creazione file
int opn(type_t req, int cfd, char pathname[]){
    int answer = 0;
    file_t* tmp;
    if(req == OPENC){
        LOCK(&cachemtx);
        if((tmp = hashtableFind(cache, pathname)) != NULL){
            UNLOCK(&cachemtx);
            //fprintf(stderr,"[SERVER] File still in the storage\n");
            answer = -3; //EEXIST
        }else{
            file_t* file = createFile(cfd);
            if(cache_state->num_file < info->max_file){
                hashtableInsert(cache, pathname, strlen(pathname)*sizeof(char), file, sizeof(file_t));
                UNLOCK(&cachemtx);

                LOCK(&cachestatemtx);
                cache_state->num_file++;

                LOCK(&statisticsmtx);
                if(cache_state->num_file > statistics->max_saved_file) statistics->max_saved_file = cache_state->num_file;
                UNLOCK(&cachestatemtx);
                UNLOCK(&statisticsmtx);
            }else{
                //politica di rimpiazzo
                char key[PATH_MAX];
                getMinIndex(cache, key);
                printf("[SERVER]replacement policy on %s\n", key);
                hashtableDeleteNode(cache, key, freeFile);
                hashtableInsert(cache, pathname, strlen(pathname)*sizeof(char), file, sizeof(file_t));
                UNLOCK(&cachemtx);

                LOCK(&statisticsmtx);
                statistics->switches++;

                LOCK(&cachestatemtx);
                if(cache_state->num_file > statistics->max_saved_file) statistics->max_saved_file = cache_state->num_file;

                UNLOCK(&statisticsmtx);
                UNLOCK(&cachestatemtx);
            }
        }
    }
    if(req == OPEN){
        LOCK(&cachemtx);
        if((tmp = hashtableFind(cache, pathname)) == NULL){
            UNLOCK(&cachemtx);
            //fprintf(stderr, "[SERVER] File not present in the storage\n");
            answer = -1; //ENOENT
        }
        else{
            LOCK(&tmp->filemtx);
            UNLOCK(&cachemtx);
            int i=0;
            while(tmp->openby[i] != 0) i++;
            tmp->openby[i] = cfd;
            UNLOCK(&tmp->filemtx);
        }
    }   
    CHECK_EQ_RETURN(writen(cfd, &answer, sizeof(int)), -1, "writen opn", -1);
    return answer;
}

//funzione chiamata quando la richiesta è una scrittura di file
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
    file_t* tmp;
    LOCK(&cachemtx);
    if((tmp = hashtableFind(cache, pathname)) != NULL){
        int flag = findvalue(tmp->openby, cfd);
        if(tmp->fdcreator == cfd && flag == 0){ //controllo che sia il creatore e abbia aperto il file
            if(filesize > info->storage_size){
                UNLOCK(&cachemtx);
                //fprintf(stderr, "[SERVER] File too large for the size of storage\n");
                answer = -4;
            }
            else{
                LOCK(&cachestatemtx);
                while(cache_state->free_space < filesize){
                    char key[PATH_MAX];
                    getMinIndex(cache, key);
                    file_t* tmp1 = hashtableFind(cache, key);
                    int size = tmp1->size;
                    printf("[SERVER]replacement policy on %s\n",key);
                    LOCK(&statisticsmtx);
                    statistics->switches++;
                    UNLOCK(&statisticsmtx);
                    hashtableDeleteNode(cache, key, freeFile);
                    cache_state->free_space += size;
                    cache_state->used_space -= size;
                    cache_state->num_file--;
                }
                tmp->size = filesize;
                tmp->contents = malloc((filesize+1)*sizeof(char));
                strcpy(tmp->contents, filebuffer);
                UNLOCK(&cachemtx);
                cache_state->free_space -= filesize;
                cache_state->used_space += filesize;
                LOCK(&statisticsmtx);
                if(statistics->max_bytes < cache_state->used_space) statistics->max_bytes = cache_state->used_space;
                UNLOCK(&statisticsmtx);
                UNLOCK(&cachestatemtx);
            }
        }else{
            UNLOCK(&cachemtx);
            //fprintf(stderr, "[SERVER] Operation not authorized for client %d\n", cfd);
            answer = -2; //EPERM
        }
    }else{
        UNLOCK(&cachemtx);
        //fprintf(stderr, "[SERVER] File not found in the storage\n");
        answer = -1; //ENOENT
    }
    free(filebuffer);
    CHECK_EQ_RETURN(writen(cfd, &answer, sizeof(int)), -1, "writen wrt", -1);
    return answer;
}

//funzione chiamata quando la richiesta è una chiusura di file
int cls(int cfd, char pathname[]){
    int answer = 0;
    file_t* tmp;
    LOCK(&cachemtx);
    if((tmp = hashtableFind(cache, pathname)) == NULL){
        UNLOCK(&cachemtx);
        //fprintf(stderr,"[SERVER] File not present\n");
        answer = -1; //ENOENT
    }else{
        LOCK(&tmp->filemtx);
        UNLOCK(&cachemtx);
        if(findvalue(tmp->openby, cfd) == -1){
            UNLOCK(&tmp->filemtx);
            //fprintf(stderr, "[SERVER] The client %d doesnt't have the file open\n", cfd);
            answer = -2; //EPERM
        }else{
            removevalue(tmp->openby, cfd);
            UNLOCK(&tmp->filemtx);
        }
    }
    CHECK_EQ_RETURN(writen(cfd, &answer, sizeof(int)), -1, "writen cls", -1);
    return answer;
}

//funzione chiamata quando la richiesta è una lettura di file
int rd(int cfd, char pathname[]){
    file_t* tmp;
    int answer = 0;
    int size = -1;
    LOCK(&cachemtx);
    if((tmp = hashtableFind(cache, pathname) )!= NULL){
        LOCK(&tmp->filemtx);
        UNLOCK(&cachemtx);
        if(findvalue(tmp->openby, cfd) == 0){
            size = tmp->size;
            UNLOCK(&tmp->filemtx);
        }
        else{
            UNLOCK(&tmp->filemtx);
            answer = -2; //EPERM
        }
    }else{
        UNLOCK(&cachemtx);
        answer = -1; //ENOENT
    }
    CHECK_EQ_EXIT(writen(cfd, &answer, sizeof(int)), -1, "writen rd");
    if(answer != 0) return answer;

    CHECK_EQ_EXIT(writen(cfd, &size, sizeof(int)), -1, "writen rd");
    CHECK_EQ_EXIT(writen(cfd, tmp->contents, tmp->size), -1, "writen rd");
    return 0;
}

//funzione chiamata quando la richiesta è una rimozione di file
int rm(int cfd, char pathname[]){
    int answer = 0;
    file_t* tmp;
    LOCK(&cachemtx);
    if((tmp = hashtableFind(cache, pathname)) == NULL){
        UNLOCK(&cachemtx);
        //fprintf(stderr,"[SERVER] File not found in the storage\n");
        answer = -1; //ENOENT
    }else{
        int filesize = tmp->size;
        
        hashtableDeleteNode(cache, pathname, freeFile);
        UNLOCK(&cachemtx);

        LOCK(&cachestatemtx);
        cache_state->num_file--;
        cache_state->free_space = cache_state->free_space + filesize;
        UNLOCK(&cachestatemtx);
    }
    CHECK_EQ_RETURN(writen(cfd, &answer, sizeof(int)), -1, "writen rm", -1);
    return answer;
}

//funzione chiamata quando la richiesta è una lettura di un certo numero di file
int rdn(int cfd, char info[]){
    int n = atoi(info);
    LOCK(&cachestatemtx);
    if(n > cache_state->num_file || n == 0) n = cache_state->num_file;
    UNLOCK(&cachestatemtx);

    CHECK_EQ_EXIT(writen(cfd, &n, sizeof(int)), -1, "writen rdn");
    Node_t *bucket, *curr;
    int i = 0;
    LOCK(&cachemtx);
    while(i<cache->numbuckets && n > 0){
        bucket = cache->buckets[i];
        for(curr=bucket; curr!=NULL; curr=curr->next){
            int len = strlen(bucket->key)+1;
            if(writen(cfd, &len, sizeof(int)) == -1){
                UNLOCK(&cachemtx);
                perror("writen rdn");
                return(EXIT_FAILURE);
            }
            if(writen(cfd, (char*)bucket->key, len*sizeof(char)) == -1){
                UNLOCK(&cachemtx);
                perror("writen rdn");
                return(EXIT_FAILURE);
            }            
            file_t* tmp = bucket->data;

            if(writen(cfd, &tmp->size, sizeof(int)) == -1){
                UNLOCK(&cachemtx);
                perror("writen rdn");
                return(EXIT_FAILURE);
            } 
            if(writen(cfd, tmp->contents, tmp->size) == -1){
                UNLOCK(&cachemtx);
                perror("writen rdn");
                return(EXIT_FAILURE);
            } 
            n--;
        }
        i++;
    }
    UNLOCK(&cachemtx);
    return 0;
}

//funzione chiamata quando è richiesta un append
int app(int cfd, char pathname[]){
    int answer = 0;
    file_t* tmp;
    size_t size;
    printf("%s\n", pathname);
    CHECK_EQ_RETURN(readn(cfd, &size, sizeof(size_t)), -1, "readn app", -1);

    char* buf = malloc((size+1)*sizeof(char));
    CHECK_EQ_RETURN(readn(cfd, buf, (size+1)*sizeof(char)), -1, "readn app", -1);

    LOCK(&cachemtx);
    if((tmp = hashtableFind(cache, pathname)) == NULL){
        UNLOCK(&cachemtx);
        //fprintf(stderr,"[SERVER] File not found in the storage\n");
        answer = -1; //ENOENT
    }else{
        if(info->storage_size < size){
            UNLOCK(&cachemtx);
            //fprintf(stderr, "[SERVER] File too large for the size of storage\n");
            answer = -4;
        }else{
            LOCK(&cachestatemtx);
            while(cache_state->free_space < size){
                char key[PATH_MAX];
                getMinIndex(cache, key);
                file_t* tmp1 = hashtableFind(cache, key);
                int size2 = tmp1->size;
                printf("[SERVER]replacement policy on %s\n",key);
                LOCK(&statisticsmtx);
                statistics->switches++;
                UNLOCK(&statisticsmtx);
                hashtableDeleteNode(cache, key, freeFile);
                cache_state->free_space += size2;
                cache_state->used_space -= size2;
                cache_state->num_file--;   
            }
            UNLOCK(&cachestatemtx);
            if((tmp = hashtableFind(cache, pathname)) != NULL){ //file ancora nella cache
                LOCK(&tmp->filemtx);
                UNLOCK(&cachemtx);

                int newsize = tmp->size+size;
                char* newcontent = malloc((newsize+1)*sizeof(char));
                memcpy(newcontent, tmp->contents, tmp->size);
                memcpy(newcontent + tmp->size, buf, size);

                free(tmp->contents);
                tmp->contents = newcontent;
                tmp->size = newsize;

                free(newcontent);

                UNLOCK(&tmp->filemtx);
                
                LOCK(&cachestatemtx);
                cache_state->free_space -= size;
                cache_state->used_space += size;
                LOCK(&statisticsmtx);
                if(statistics->max_bytes < cache_state->used_space) statistics->max_bytes = cache_state->used_space;
                UNLOCK(&cachestatemtx);
                UNLOCK(&statisticsmtx);
                
            }else{
                UNLOCK(&cachemtx);
                //fprintf(stderr, "[SERVER] File remmoved by replacement policy\n");
                answer = -1; //ENOENT
            }

        }
    }
    CHECK_EQ_RETURN(writen(cfd, &answer, sizeof(int)), -1, "writen app", -1);
    return answer;
}