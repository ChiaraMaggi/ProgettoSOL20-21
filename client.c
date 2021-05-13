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

#include "parsing.h"
#include "utils.h"
#include "api.h"

#define UNIX_PATH_MAX 108
#define SOMAXCON 100
//#define SOCKETNAME "SOLsocket.sk"

int main(void){
    char* SOCKETNAME = getSocketName("config.txt");

    int sockfd;
    struct sockaddr_un server_addr;
    CHECK_EQ_EXIT((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)), -1, "socket");
    memset(&server_addr, '0', sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;    
    strncpy(server_addr.sun_path, SOCKETNAME, strlen(SOCKETNAME)+1);
    /*gestico la situzione in cui il client faccia richesta al server che non è stato ancora
    svegliato e quindi invece di dare errore riposa per 1 sec e poi riprova*/
    while (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1 ){
        if ( errno == ENOENT )
            sleep(1); /* sock non esise */
        else {
            printf("il socket esiste gia ma il server non è ancora stato svegliato\n");
            exit(EXIT_FAILURE); 
        }
    }
    printf("Client\n");
    close(sockfd);
    exit(EXIT_SUCCESS); 
}