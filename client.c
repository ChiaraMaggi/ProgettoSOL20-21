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
#define O_CREATE 0x01
#define O_LOCK 0x10

int main(void){
    
    struct timespec abstime;
    clock_gettime(CLOCK_REALTIME, &abstime);
    abstime.tv_sec += 5;
    if(openConnection(SOCKETNAME, 2000, abstime) == -1){
        perror("opening connection");
        return (EXIT_FAILURE);
    }
    sleep(1);
    if(openFile("ciao.txt", O_CREATE) == -1){
        perror("operning file");
        return (EXIT_FAILURE);
    }

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