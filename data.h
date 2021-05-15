#ifndef DATA_H_
#define DATA_H_

#include<stdlib.h>
#include<stdio.h>

typedef struct Data{
    FILE* fd;
    int fdcreator;
}Data_t;

Data_t* dataInit(FILE*, int);


#endif