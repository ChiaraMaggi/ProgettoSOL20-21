/**
 * @file parsing.c
 * @author Chiara Maggi 578517
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

/*
Iniziallizazione della struct contenete le infromazioni di configurazione
*/
Info_t* initInfo(){
    Info_t* config = malloc(sizeof(Info_t));
    config->workers_thread = 0;
    config->max_file = 0;
    config->storage_size = 0;
    config->socket_name = malloc(MAX_LEN * sizeof(char));
    return config;
}

/*
Libera la memoria della struct Info_t
*/
void freeInfo(Info_t* config){
    free(config->socket_name);
    free(config);
}

/*
Gestione dell'errore del controllo se il valore inserito e' un numero e settaggio
*/
void checkAndSet(int* x, char* value, char* var, int check, int n){
    if(check == 1)
        fprintf(stderr, "value for %s not valid: %s is not a number\n",var ,value);
    if(check == 2)
        fprintf(stderr, "overflow/underflow error\n");
    if(check == 0)
        *x = (int)n;
}

/*  
    Ritrona:
    0 = tutto ok
    -1 = errore nel parsing 
 */
int parsConfiguration(char* filepath, Info_t* config){
    /*variabili per la configurazione*/
    char* parameter;
    CHECK_EQ_EXIT((parameter = malloc(MAX_LEN*sizeof(char))), NULL, "malloc parameter name");
    char* value;
    CHECK_EQ_EXIT((value = malloc(MAX_LEN*sizeof(char))), NULL, "malloc parameter value");

    /*buffer per lettura del file di configurazione*/
    char* buff;
    CHECK_EQ_EXIT((buff = malloc(MAX_LEN*sizeof(char))), NULL, "malloc parsing buffer");

    /*apertura file congifurazione*/
    FILE* file;
    CHECK_EQ_EXIT((file = fopen(filepath, "r")), NULL, "ERROR opening configuration file");

    /*scorro tutto il file*/
    while(fgets(buff, MAX_LEN, file) != NULL){

        /* controllo se sono su una linea contenete informazioni */
        if(strlen(buff) > 1 && buff[0] != '-'){ /*escludo il titolo e le linee vuote*/
            sscanf(buff, "%s = %s", parameter, value);

            long number;
            /*valore di ritorno della funzione che ci dice se si tratta di un numero*/
            int check = isNumber(value, &number);

            /*caso: NumThreadWorkers*/
            if(strcmp(parameter, "NUM_WORKERS_THREAD") == 0)
                checkAndSet(&config->workers_thread, value, "NUM_WORKERS_THREAD", check, number);    

            /*caso: NumMaxFile*/
            if(strcmp(parameter, "MAX_NUM_FILE") == 0)
                checkAndSet(&config->max_file, value, "MAX_NUM_FILE", check, number);    

            /*caso: StorageSpaceSize*/
            if(strcmp(parameter, "STORAGE_SIZE") == 0)
                checkAndSet(&config->storage_size, value, "STORAGE_SIZE", check, number);    
                
            /*caso: SocketName*/ //come va fatto il path assoluto del socket?
            if(strcmp(parameter, "SOCKET_NAME") == 0){
                if(strcmp(value, ""))
                    strcpy(config->socket_name, value);
                else fprintf(stderr, "socket path is an empty string\n");
            }
        }
        strcpy(parameter, "");
        strcpy(value, "");
    }
    fclose(file);
    free(parameter);
    free(value);
    free(buff);
    /*controllo se tutte le variabili hanno assunto un valore*/
    if((config->workers_thread == 0) || (config->max_file == 0) || (config->storage_size == 0) || (!strcmp(config->socket_name, ""))){
        return -1;
    }
    return 0;
}

char* getSocketName(char* filepath){
    /*variabili per la configurazione*/
    char* parameter;
    CHECK_EQ_EXIT((parameter = malloc(MAX_LEN*sizeof(char))), NULL, "malloc parameter name");
    char* value;
    CHECK_EQ_EXIT((value = malloc(MAX_LEN*sizeof(char))), NULL, "malloc parameter value");

    /*buffer per lettura del file di configurazione*/
    char* buff;
    CHECK_EQ_EXIT((buff = malloc(MAX_LEN*sizeof(char))), NULL, "malloc parsing buffer");

    /*apertura file congifurazione*/
    FILE* file;
    CHECK_EQ_EXIT((file = fopen(filepath, "r")), NULL, "ERROR opening configuration file");
    /*scorro tutto il file*/
    while(fgets(buff, MAX_LEN, file) != NULL){

        /* controllo se sono su una linea contenete informazioni */
        if(strlen(buff) > 1 && buff[0] != '-'){ /*escludo il titolo e le linee vuote*/
            sscanf(buff, "%s = %s", parameter, value);   
                
            /*caso: SocketName*/
            if(strcmp(parameter, "SOCKET_NAME") == 0){
                if(strcmp(value, ""))
                    return value;
            }
        }
        strcpy(parameter, "");
        strcpy(value, "");
    }
    fclose(file);
    free(parameter);
    free(value);
    free(buff);
    return 0;
}