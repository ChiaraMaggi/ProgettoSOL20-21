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

#define O_CREATE 01
#define O_LOCK 10

static char socketname[100];

void arg_f(char* s_name){
    strcpy(socketname, s_name);
    struct timespec abstime;
    clock_gettime(CLOCK_REALTIME, &abstime);
    abstime.tv_sec += 5;
    if(openConnection(socketname, 2000, abstime) == -1) perror("opening connection");
}

int main(int argc, char* argv[]){
    int opt; 
    void** buf = malloc(sizeof(buf));
    size_t size;
    while((opt = getopt(argc, argv, ":hf:w:W:r:R:d:t:l:u:c:p")) != -1){ //opstring contine le opzioni che vogliamo gestire
        //se getop trova una delle opzioni ritrona un intero (relativo al carattere letto) quindi posso fare lo switch
        switch(opt){
            case 'h':
                printf("-f filesocketname\n-w dirname[,n=0]\n-W file1[,file2]\n-r file1[,file2]\n-R [n=0]\n-d dirname\n-t time\n-l file1[,file2]\n-u file1[,file2]\n-c file1[,file2]\n-p\n");
                break; //optarg = variabile che setta getopt all'argomento dell'opzione
            case 'f':
                arg_f(optarg);
                break;
            case 'w':
                closeConnection(socketname);
                break;
            case 'W':
                break; 
            case 'r':
                break;
            case 'R':
                break;
            case 'd':
                break;
            case 't':
                break;
            case 'l':   
                break;
            case 'u':
                break;
            case 'c':
                break;
            case 'p':
                break;
            case ':': {//quando l'operazione da eseguire appartiene a quelle da poter eseguire ma manca l'argomento
                switch(optopt){ //optopt variabile interna di getopt che contiene le operazioni che non sono gestite
                    case 'R':
                        break;
                    case 't':
                        break;
                    default:
                        printf("L'opzione '-%c' richiede un argomento\n", optopt); 
                }
            }
            case '?':{ //getopt ritrona ? quando l'operazione da eseguire non appartiene alla opstring
                printf("L'opzione '-%c' non e' gestita\n", optopt); 
            } break;
            default:;
        }
    }
    sleep(15);
    if(closeConnection("SOLsocket.sk") == -1){
        perror("closing connection");
        return (EXIT_FAILURE);
    }
    return 0;
}