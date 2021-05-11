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

#include "parsing.h"
#include "utils.h"

#define DEFAULT_WORKERS_THREAD 10
#define DEFAULT_MAX_FILE 100
#define DEFAULT_STORAGE_SIZE 512
#define DEFAULT_SOCKET_NAME "SOLsocket.sk"

#define UNIX_PATH_MAX 108

void setDefault(Info_t* info){
    info->workers_thread = DEFAULT_WORKERS_THREAD;
    info->max_file = DEFAULT_MAX_FILE;
    info->storage_size = DEFAULT_STORAGE_SIZE;
    strcpy(info->socket_name, DEFAULT_SOCKET_NAME);
}

static void* fun(void* arg){

}

int main(int argc, char* argv[]){
    /*
    Iniziallizazione server
    */
    Info_t* Information = initInfo();

    /*caso in cui il valori passati non sono corretti o incompleti*/
    if((argc == 3 && strcmp(argv[1], "-cf")) || argc == 2){
        printf("use: %s [-cf configurationfilename]\n", argv[0]);
        return EXIT_FAILURE;
    }

    /*caso in cui il file di congifigurazione non è passato*/
    if(argc == 1){
        printf("default configuration (configuration file is not passed):\n");
        setDefault(Information);
    }
    else{ /*caso corretto*/
        if(parsConfiguration(argv[2], Information) == -1){
            /*se qualcosa va storto nel parsing setto valori di default*/
            fprintf(stderr, "ERROR: configuration file is not parsed correctly, server will be setted with default values\n");
            setDefault(Information);
        }    
    }
    //printf("%d %d %d %s\n", Information->workers_thread, Information->max_file, Information->storage_size, Information->socket_name);
     
    int fd_skt;
    /*creazione struttura socket*/
    struct sockaddr_un sa;
    strncpy(sa.sun_path, Information->socket_name, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;
    fd_skt = socket(AF_UNIX, SOCK_STREAM, 0);
    bind(fd_skt, (struct sockaddr*)&sa, sizeof(sa));

    /*creazione thread master*/
    pthread_t tidM;
    if(pthread_create(&tidM, NULL, &fun, NULL) != 0){
        fprintf(stderr, "ERROR: impossible to create master thread\n");
        return EXIT_FAILURE;
    }

    
}