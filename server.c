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
#include "icl_hash.h"
#include "api.h"

#define DEFAULT_WORKERS_THREAD 10
#define DEFAULT_MAX_FILE 100
#define DEFAULT_STORAGE_SIZE 512
#define DEFAULT_SOCKET_NAME "SOLsocket.sk"
#define NUMBUCKETS 100

Hashtable_t* hashtable = NULL;

typedef struct File{
    int fdcreator;
    char* pathname;
    void* buf;
    int size;
}File_t;

typedef struct server_state
{
   int num_file;
   size_t used_space;
   size_t free_space;
}Serverstate_t;


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
    Info_t* information = initInfo();
    hashtable = HASHTABLE_INIT(NUMBUCKETS);

    /*caso in cui il valori passati non sono corretti o incompleti*/
    if((argc == 3 && strcmp(argv[1], "-f")) || argc == 2){
        printf("use: %s [-f pathconfigurationfile]\n", argv[0]);
        return EXIT_FAILURE;
    }

    /*caso in cui il file di congifigurazione non è passato*/
    if(argc == 1){
        printf("server default configuration (configuration file is not passed)\n");
        setDefault(information);
    }
    else{ /*caso corretto*/
        if(parsConfiguration(argv[2], information) == -1){
            /*se qualcosa va storto nel parsing setto valori di default*/
            fprintf(stderr, "ERROR: configuration file is not parsed correctly, server will be setted with default values\n");
            setDefault(information);
        }    
    }

    Serverstate_t server_state;
    server_state.num_file = 0;
    server_state.used_space = 0;
    server_state.free_space = information->storage_size;


    /*-----------CREAZIONE SOCKET-------------*/
    cleanup(information);
	int fd_server, fd_client;
	struct sockaddr_un sa;
    memset(&sa, '0', sizeof(sa));
	strncpy(sa.sun_path, information->socket_name, UNIX_PATH_MAX);
	sa.sun_family = AF_UNIX;
	CHECK_EQ_EXIT((fd_server = socket(AF_UNIX, SOCK_STREAM, 0)), -1, "socket");
	CHECK_EQ_EXIT(bind(fd_server, (struct sockaddr*)&sa, sizeof(sa)), -1, "bind");
    

    /*-------------GESTIONE CLIENT------------*/
	while(1)
	{
		fprintf(stdout, "SERVER: listening...\n");
		CHECK_EQ_EXIT(listen(fd_server, SOMAXCON), -1, "listen");
		CHECK_EQ_EXIT(fd_client = accept(fd_server, NULL, 0), -1, "accept");
		fprintf(stdout, "SERVER: new client accepted %d\n", fd_client);

        
        type_t req;
        readn(fd_client, &req, sizeof(req));
        if(req == OPENC){
            printf("corretto\n");
            int len;
            char* path;
            readn(fd_client, &len, sizeof(int));
            printf("%d\n", len);
            path = malloc(len * sizeof(char));
            readn(fd_client, path, len);
            printf("%s\n", path);
            int answer = 1;
            writen(fd_client, &answer, sizeof(answer));
        }

        if(req == READ){
            printf("corretto\n");
            int len;
            char* path;
            readn(fd_client, &len, sizeof(int));
            printf("%d\n", len);
            path = malloc(len * sizeof(char));
            readn(fd_client, path, len);
            printf("%s\n", path);
            char* buf = "dio can";
            int answer = strlen(buf);
            writen(fd_client, &answer, sizeof(int));
            writen(fd_client, buf, sizeof(buf));
        }

        if(req == CLOSECONN){
            printf("chiusura\n");
            int answer = 1;
            writen(fd_client, &answer, sizeof(answer));
            close(fd_client);
        }

	}
	CHECK_EQ_EXIT(close(fd_server), -1, "close");
    freeInfo(information);
    return 0;
}