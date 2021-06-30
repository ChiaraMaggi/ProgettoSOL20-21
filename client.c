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
#include<sys/stat.h>
#include<dirent.h>

#include "parsing.h"
#include "utils.h"
#include "api.h"
#include "icl_hash.h"

#define O_CREATE 01
#define MAX_DIR_LEN 200

static char socketname[100];

void arg_f(char* s_name);
int parse_w(char* optarg, char* dirname, long* filetoSend);
int arg_w(char* dirname, long* fileToSend);

int main(int argc, char* argv[]){
    if(argc == 1){
        printf("One command is necessary: use -h for see the options\n");
        return 0;
    }
    int opt; 
    void** buf = malloc(sizeof(buf));
    size_t size;
    while((opt = getopt(argc, argv, ":hf:w:W:r:R:d:t:l:u:c:p")) != -1){ //opstring contine le opzioni che vogliamo gestire
        //se getop trova una delle opzioni ritrona un intero (relativo al carattere letto) quindi posso fare lo switch
        char dir[MAX_DIR_LEN];
        long numFileToSend;
        switch(opt){
            case 'h':
                printf("-f filesocketname\n-w dirname[,n=0]\n-W file1[,file2]\n-r file1[,file2]\n-R [n=0]\n-d dirname\n-t time\n-l file1[,file2]\n-u file1[,file2]\n-c file1[,file2]\n-p\n");
                return 0; //optarg = variabile che setta getopt all'argomento dell'opzione
            case 'f':
                arg_f(optarg);
                break;
            case 'w':
                if(parse_w(optarg, dir, &numFileToSend) == -1){
                    fprintf(stderr, "impossible to parse -w correctly\n");
                    break;
                }
                if(arg_w(dir, &numFileToSend) == -1){
                    fprintf(stderr, "operation -w doesn't end correctly\n");
                    break;
                }
                break;
            case 'W':
                break; 
            case 'r':
                break;
            case 'R':
                break;
            case 'd':
                break;
            case 't':
                break;
            case 'l':   
                break;
            case 'u':
                break;
            case 'c':
                break;
            case 'p':
                break;
            case ':': {//quando l'operazione da eseguire appartiene a quelle da poter eseguire ma manca l'argomento
                switch(optopt){ //optopt variabile interna di getopt che contiene le operazioni che non sono gestite
                    case 'R':
                        break;
                    case 't':
                        break;
                    default:
                        fprintf(stderr, "Option '-%c' needs an argoument\n", optopt); 
                } break;
            }
            case '?':{ //getopt ritrona ? quando l'operazione da eseguire non appartiene alla opstring
                fprintf(stderr, "'-%c' is an invalid option \n", optopt); 
            } break;
            default:;
        }
    }
    sleep(10);/*
    if(closeConnection("SOLsocket.sk") == -1){
        perror("ERROR closing connection");
        return (EXIT_FAILURE);
    }*/
    return 0;
}

void arg_f(char* s_name){
    strcpy(socketname, s_name);
    struct timespec abstime;
    clock_gettime(CLOCK_REALTIME, &abstime);
    abstime.tv_sec += 5;
    if(openConnection(socketname, 2000, abstime) == -1) perror("ERROR opening connection");
}

int parse_w(char* optarg, char* dirname, long* fileToSend){
    if(optarg[0] == ','){
        fprintf(stderr, "directory name can't be omitted\n");
        return -1;
    }
    int i=0;
    while(optarg[i] != '\0' && optarg[i] != ',')    i++;
    if(optarg[i] == '\0')   strncpy(dirname, optarg, i+1);
    else{
        strncpy(dirname, optarg, i);
        if(optarg[i+1] != 'n' || optarg[i+2] != '='){
            fprintf(stderr, "usage: -w dirname[,n=0]\n");
            return -1;
        }    
        char* numFile = malloc(sizeof(char)*(strlen(optarg) - i));
        i = i+3; //arrivo al punto dopo '='
        int j = 0;
        while(optarg[i] != '\0'){
            numFile[j] = optarg[i];
            j++;
            i++;
        }
        if(isNumber(numFile, fileToSend) != 0){
            fprintf(stderr, "%s is not a number\n", numFile);
            return -1;
        }
    }
    return 0;
}

int arg_w(char* dirname, long* fileToSend){
    //controllo se dirname è effettivamente una directory
    struct stat statbuf;
    int r;
    //gestione errore, avrei potuto farla anche con la macro SYSCALL_EXIT contenuta in utilis
    CHECK_EQ_EXIT((r = stat(dirname, &statbuf)), -1, "stat");
    if(!S_ISDIR(statbuf.st_mode)){
        printf("%s is not a directory\n", dirname);
        exit(EXIT_FAILURE);
    }

    DIR* dir;
    
    //printf("Directory %s:\n", dirname);
    CHECK_EQ_RETURN((dir = opendir(dirname)), NULL, "opendir", -1);
    struct dirent* file;
    while(*fileToSend != 0 && (errno = 0, file = readdir(dir)) != NULL){
        printf("entro\n");
        struct stat statebuf;
        char filename[MAX_LEN]; 
        int len1 = strlen(dirname);
        int len2 = strlen(file->d_name);
        if ((len1+len2+2)>MAX_LEN) {
            fprintf(stderr, "ERRORE: MAXFILENAME too small\n");
            exit(EXIT_FAILURE);
        }	    
        strncpy(filename,dirname, MAX_LEN-1);
        strncat(filename,"/", MAX_LEN-1);
        strncat(filename,file->d_name, MAX_LEN-1);

        CHECK_EQ_RETURN(stat(filename, &statebuf), -1, "stat", -1);
       
        if(S_ISDIR(statebuf.st_mode)){
            if(!isdot(filename)){
                printf("sottodirectory\n");
                int ret = arg_w(filename, fileToSend);
                if(ret == -1) return -1;
            }
        }else{
            printf("%s\n", filename);
            CHECK_EQ_RETURN(openFile(filename, O_CREATE), -1, "openFile", -1);
            printf("file creato\n");
            CHECK_EQ_RETURN(writeFile(filename, NULL), -1, "writeFile", -1);
            printf("file scritto\n");
            // CHECK_EQ_RETURN(closeFile(filename), -1, "closeFile", -1);
            *fileToSend = *fileToSend - 1;
        }
    }
    if(errno != 0) perror("readdir");
    closedir(dir);
    return 0;
}


