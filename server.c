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

#include "parsing.h"
#include "utils.h"

#define WORKERS_THREAD 10
#define MAX_FILE 100
#define STORAGE_SIZE 512
#define SOCKET_NAME "./SOLsocket.sk"

void setDefault(Info_t* info){
    info->workers_thread = WORKERS_THREAD;
    info->max_file = MAX_FILE;
    info->storage_size = STORAGE_SIZE;
    strcpy(info->socket_name, SOCKET_NAME);
}

int main(int argc, char* argv[]){
    Info_t* Information = initInfo();

    /*caso in cui il valori passati non sono corretti*/
    if((argc == 3 && strcmp(argv[1], "-cf")) || argc == 2){
        printf("use: %s [-cf configurationfilename]\n", argv[0]);
        return EXIT_FAILURE;
    }

    /*caso in cui il file di congif non è passato*/
    if(argc == 1){
        printf("default configuration: configuration file is not passed\n");
        setDefault(Information);
    }
    else{ /*caso corretto*/
        if(parsConfiguration(argv[2], Information) == -1){
            /*se qualcosa va storto nel parsing setto valori di default*/
            fprintf(stderr, "ERROR: configuration file is not parsed correctly, server will be setted with default values\n");
            setDefault(Information);
        }    
    }
    printf("%d %d %d %s\n", Information->workers_thread, Information->max_file, Information->storage_size, Information->socket_name);
    return 0;   
}