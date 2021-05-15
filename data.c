/**
 * @file data.c
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

#include "data.h"

Data_t* dataInit(FILE* fd, int fdcreator){
    Data_t* data = (Data_t*)malloc(sizeof(Data_t));
    data->fd = fd;
    data->fdcreator = fdcreator;
    return data;
}

