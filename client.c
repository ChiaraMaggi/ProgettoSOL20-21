/**
 * @file client.c
 * @author Chiara Maggi
 * 
 * @copyright Copyright (c) 2021
 * 
 */
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
#include<limits.h>
#include<dirent.h>
#include<libgen.h>
#include<fcntl.h>

#include "parsing.h"
#include "utils.h"
#include "api.h"
#include "icl_hash.h"

#define O_CREATE 01
#define MAX_DIR_LEN 200

static char socketname[100];
static int print_flag = 0;

int arg_f(char* s_name);
int parse_w(char* optarg, char* dirname, long* filetoSend);
int arg_w(char* dirname, long* fileToSend);
int arg_W(char* optarg);
int arg_r(char* optarg, char* dir);

int main(int argc, char* argv[]){
    if(argc == 1){
        printf("One command is necessary: use -h for see the options\n");
        return 0;
    }
    int flag_h=0, flag_p=0, flag_d=0, flag_r=0, flag_R=0, flag_f=0;
    char* dir = NULL;

    for(int i=0; i<argc; i++){
        if(strcmp(argv[i], "-h") == 0) flag_h = i;
        if(strcmp(argv[i], "-p") == 0) flag_p = i;
        if(strcmp(argv[i], "-d") == 0) flag_d = i;
        if(strcmp(argv[i], "-r") == 0) flag_r = i;
        if(strcmp(argv[i], "-R") == 0) flag_R = i;
        if(strcmp(argv[i], "-f") == 0) flag_f = i;
    }
    if(flag_h > 0){
        printf("-f filesocketname\n-w dirname[,n=0]\n-W file1[,file2]\n-r file1[,file2]\n-R [n=0]\n-d dirname\n-t time\n-l file1[,file2]\n-u file1[,file2]\n-c file1[,file2]\n-p\n");
        return 0;
    }

    if(flag_p > 0)
        print_flag = 1;
    
    if(flag_d > 0){
        if(flag_r == 0 && flag_R == 0){
            fprintf(stderr, "Must be operation -r or -R with operation -d\n");
            return 0;
        }else{
            dir = malloc((strlen(argv[flag_d+1])+1)*sizeof(char));
            strcpy(dir, argv[flag_d+1]);
        }   
    }

    if(flag_f > 0){
        if(arg_f(argv[flag_f+1]) == -1){
            fprintf(stderr, "Connection failed\n");
            return 0;
        }else{
            fprintf(stdout, "Connected correctly\n");
        }
    }else{
        fprintf(stderr, "Impossible to satisfy the request if the name of socket is not declared\n");
        return 0;
    }

    int opt; 
    size_t size;
    while((opt = getopt(argc, argv, ":hf:w:W:D:r:R:d:t:l:u:c:p")) != -1){ //opstring contine le opzioni che vogliamo gestire
        //se getop trova una delle opzioni ritrona un intero (relativo al carattere letto) quindi posso fare lo switch
        char dir[MAX_DIR_LEN];
        long numFileToSend;
        switch(opt){
            case 'h':
                break;
            case 'f':
                break;
            case 'w':
                if(parse_w(optarg, dir, &numFileToSend) == -1){
                    fprintf(stderr, "Impossible to parse -w correctly\n");
                    break;
                }
                if(arg_w(dir, &numFileToSend) == -1){
                    fprintf(stderr, "Operation -w doesn't end correctly\n");
                }
                break;
            case 'W':
                if(arg_W(optarg) == -1){
                    fprintf(stderr, "Operation -W doesn't end correctly\n");
                }
                break; 
            case 'D':
                fprintf(stderr, "Operation -D not supported\n");
                break;
            case 'r':
                arg_r(optarg, dir);
                break;
            case 'R':
                break;
            case 'd':
                break;
            case 't':
                break;
            case 'l':   
                fprintf(stderr, "Operation -l not supported\n");
                break;
            case 'u':
                fprintf(stderr, "Operation -u not supported\n");
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

int arg_f(char* s_name){
    strcpy(socketname, s_name);
    struct timespec abstime;
    clock_gettime(CLOCK_REALTIME, &abstime);
    abstime.tv_sec += 5;
    CHECK_EQ_RETURN(openConnection(socketname, 2000, abstime), -1, "Opening connection", -1);
    return 0;
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
        char* numFile = malloc(sizeof(char)*(strlen(optarg) - i+1));
        int j = 0;
        i++;
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
    CHECK_EQ_EXIT((r = stat(dirname, &statbuf)), -1, "stat arg_w");
    if(!S_ISDIR(statbuf.st_mode)){
        printf("%s is not a directory\n", dirname);
        exit(EXIT_FAILURE);
    }

    DIR* dir;
    CHECK_EQ_RETURN((dir = opendir(dirname)), NULL, "opendir", -1);
    struct dirent* file;

    //se n=0????
    while(*fileToSend != 0 && (errno = 0, file = readdir(dir)) != NULL){
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
                int ret = arg_w(filename, fileToSend);
                if(ret == -1) return -1;
            }
        }else{
            char resolvedpath[PATH_MAX];
            char* res;
            CHECK_EQ_RETURN((res = realpath(filename, resolvedpath)), NULL, "realpath", -1);
            CHECK_EQ_RETURN(openFile(resolvedpath, O_CREATE), -1, "openFile arg_w", -1);
            CHECK_EQ_RETURN(writeFile(resolvedpath, NULL), -1, "writeFile arg_w", -1);
            CHECK_EQ_RETURN(closeFile(resolvedpath), -1, "closeFile arg_w", -1);
            *fileToSend = *fileToSend - 1;
        }
    }
    if(errno != 0) perror("readdir");
    closedir(dir);
    return 0;
}

int arg_W(char* optarg){
    char* tmpstr;
    char* token = strtok_r(optarg, ",", &tmpstr);
    char resolvedpath[PATH_MAX];
    while(token){
        char* file = token;
        char* res;
        CHECK_EQ_RETURN((res = realpath(file, resolvedpath)), NULL, "realpath arg_W", -1);
        //printf("%s\n",resolvedpath);
        struct stat info_file;
        stat(resolvedpath, &info_file);
        if (S_ISREG(info_file.st_mode)) {
            CHECK_EQ_RETURN(openFile(resolvedpath, O_CREATE), -1, "openFile arg_W", -1);
            CHECK_EQ_RETURN(writeFile(resolvedpath, NULL), -1, "writeFile arg_W", -1);
            CHECK_EQ_RETURN(closeFile(resolvedpath), -1, "closeFile arg_W", -1);
        }
        token = strtok_r(NULL, ",", &tmpstr);
    }
    return 0;
}

int arg_r(char* optarg, char* dir){
    char* tmpstr;
    char* token = strtok_r(optarg, ",", &tmpstr);
    char resolvedpath[PATH_MAX];
    while(token){
        char* file = token;
        char* res;
        CHECK_EQ_RETURN((res = realpath(file, resolvedpath)), NULL, "realpath arg_r", -1);
        CHECK_EQ_RETURN(openFile(resolvedpath, 00), -1, "openFile arg_r", -1);

        //READ FILE
        char* buf;
        size_t size;
        CHECK_EQ_RETURN(readFile(resolvedpath,(void**)&buf,&size), -1, "readFile arg_r", -1);
        if(dir != NULL){
            char* filename = basename(resolvedpath);
            char path[PATH_MAX];
            sprintf(path, "%s/%s", dir, filename);
            int fd_file;
            int check = mkdir_p(dir);
            if(check == 0) printf("cartella creata\n");
            else printf("cartella non creata\n");
            //CREA FILE SE NON ESISTE
            if((fd_file = open(path, O_CREAT|O_WRONLY, 0666)) == -1){
				perror("open");
				if(print_flag)
					printf("richiesta di scrittura del file <%s> su disco è fallita\n",token);
				token = strtok_r(NULL, ",", &tmpstr);
				//free(buf);
				continue;
			}

			if(writen(fd_file, buf, size) == -1){
				perror("writen");
				if(print_flag)
					printf("richiesta di scrittura del file <%s> su disco è fallita\n",token);
				token = strtok_r(NULL, ",", &tmpstr);
				close(fd_file);
				//free(buf);
				continue;
			}
			close(fd_file);
    	}
        CHECK_EQ_RETURN(closeFile(resolvedpath), -1, "closeFile arg_r", -1);
        token = strtok_r(NULL,",",&tmpstr);
    }
    return 0;
}