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

#include "parsing.h"
#include "utils.h"

#define DEFAULT_WORKERS_THREAD 10
#define DEFAULT_MAX_FILE 100
#define DEFAULT_STORAGE_SIZE 512
#define DEFAULT_SOCKET_NAME "/SOLsocket.sk"

void setDefault(Info_t* info){
    info->workers_thread = DEFAULT_WORKERS_THREAD;
    info->max_file = DEFAULT_MAX_FILE;
    info->storage_size = DEFAULT_STORAGE_SIZE;
    strcpy(info->socket_name, DEFAULT_SOCKET_NAME);
}

void cleanup(Info_t* info) {
    unlink(info->socket_name);
}

int main(int argc, char* argv[]){

    /*-----------INIZIALIZZO SERVER-----------------*/
    Info_t* Information = initInfo();

    /*caso in cui il valori passati non sono corretti o incompleti*/
    if((argc == 3 && strcmp(argv[1], "-f")) || argc == 2){
        printf("use: %s [-f pathconfigurationfile]\n", argv[0]);
        return EXIT_FAILURE;
    }

    /*caso in cui il file di congifigurazione non è passato*/
    if(argc == 1){
        printf("default configuration (configuration file is not passed)\n");
        setDefault(Information);
    }
    else{ /*caso corretto*/
        if(parsConfiguration(argv[2], Information) == -1){
            /*se qualcosa va storto nel parsing setto valori di default*/
            fprintf(stderr, "ERROR: configuration file is not parsed correctly, server will be setted with default values\n");
            setDefault(Information);
        }    
    }

    /*-----------CREAZIONE SOCKET-------------*/
    cleanup(Information);
	int fd_server, fd_client;
	struct sockaddr_un sa;
    memset(&sa, '0', sizeof(sa));
	strncpy(sa.sun_path, Information->socket_name, UNIX_PATH_MAX);
	sa.sun_family = AF_UNIX;
	CHECK_EQ_EXIT((fd_server = socket(AF_UNIX, SOCK_STREAM, 0)), -1, "socket");
	CHECK_EQ_EXIT(bind(fd_server, (struct sockaddr*)&sa, sizeof(sa)), -1, "bind");

    /*-------------GESTIONE CLIENT------------*/
    /*char buf[100];
    int N = 100;*/
	while(1)
	{
		fprintf(stdout, "SERVER: listening...\n");
		CHECK_EQ_EXIT(listen(fd_server, SOMAXCON), -1, "listen");
		CHECK_EQ_EXIT(fd_client = accept(fd_server, NULL, 0), -1, "accept");
		fprintf(stdout, "SERVER: new client accepted %d\n", fd_client);
        /*read(fd_client,buf,N);
        printf("Server got: %s\n",buf) ;
        write(fd_client,"Bye!",5);*/
	}
	CHECK_EQ_EXIT(close(fd_server), -1, "close");
    freeInfo(Information);
    return 0;
}