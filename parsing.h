/**
 * @file parsing.h
 * @author Chiara Maggi
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef PARSING_H_
#define PARSING_H_

#include<stdio.h>

#define MAX_LEN 100

/*
Struttura che contiene le informazioni per la creazione del server
*/
typedef struct Info
{
    int workers_thread; /*Numero di thread workers*/
    int max_file; /*Numero massimo di file contenibili nello storage*/
    int storage_size;  /*Spazio di arrchiviazione*/
    char* socket_name; /*Nome del Socket*/
}Info_t;

/*
Inizializzazione della struct
*/
Info_t* initInfo();

/*
Liberazione memoria struct
*/
void freeInfo(Info_t*);

/*
Controllo dati configurazione e settaggio
*/
void checkAndSet(int*, char*,char*, int, int);

/*
Funzione che effettua il parsing della configurazione server
*/
int parsConfiguration(char*, Info_t*);

#endif
