/**
 * @file client.c
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
#include<time.h>

#include "parsing.h"
#include "utils.h"
#include "api.h"
#include "icl_hash.h"

#define SOCKETNAME "SOLsocket.sk"
#define O_CREATE 01
#define O_LOCK 10

int main(int argc, char* argv[]){
    /*int opt; 
    while((opt = getopt(argc, argv, ":n:m:o:h")) != -1){ //opstring contine le opzioni che vogliamo gestire
        //se getop trova una delle opzioni ritrona un intero (relativo al carattere letto) quindi posso fare lo switch
        switch(opt){
            case 'n':arg_n(optarg); break; //optarg = variabile che setta getopt all'argomento dell'opzione
            case 'm':arg_m(optarg); break;
            case 'o':arg_o(optarg); break;
            case 'h':arg_h(argv[0]); break; //non ho nessun argomento, mi interessa passare il nome del programma
            case ':': { //quando l'operazione da eseguire appartiene a quelle da poter eseguire ma manca l'argomento
                printf("L'opzione '-%c' richiede un argomento\n", optopt); //= variabile interna di getopt che contiene le operazioni che non sono gestite
            }
            case '?':{ //getopt ritrona ? quando l'operazione da eseguire non appartiene alla opstring
                printf("L'opzione '-%c' non e' gestita\n", optopt); 
            } break;
            default:;
        }
    }
    */
    struct timespec abstime;
    clock_gettime(CLOCK_REALTIME, &abstime);
    abstime.tv_sec += 5;
    if(openConnection(SOCKETNAME, 2000, abstime) == -1){
        perror("opening connection");
        return (EXIT_FAILURE);
    }

    openFile("home/chiara/ciao.txt", O_CREATE);

    if(closeConnection(SOCKETNAME) == -1){
        perror("closing connection");
        return (EXIT_FAILURE);
    }
    /*char buf[100];
    int N = 100;
    CHECK_EQ_EXIT(write(sockfd, "Hello!", 7), -1, "write");
    CHECK_EQ_EXIT(read(sockfd, buf, N), -1, "read");
    printf("Client got: %s\n",buf) ;
    close(sockfd);*/
    exit(EXIT_SUCCESS); 
}