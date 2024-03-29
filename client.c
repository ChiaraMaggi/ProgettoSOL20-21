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
#include<linux/limits.h>
#include<dirent.h>
#include<libgen.h>
#include<fcntl.h>

#include "utils.h"
#include "api.h"

#define O_CREATE 01
#define MAX_DIR_LEN 200

static char socketname[100];
static int print_flag = 0;

int arg_f(char* s_name);
int parse_w(char* optarg, char* dirname, long* filetoSend);
int arg_w(char* dirname, long* fileToSend);
int arg_W(char* optarg);
int arg_r(char* optarg, char* dir);
int arg_c(char* optarg);
int arg_R(char* optarg, char* dir);

int main(int argc, char* argv[]){
    if(argc == 1){
        printf("One command is necessary: use -h for see the options\n");
        return 0;
    }
    //flag per fare controlli sui vari comandi
    int flag_h=0, flag_p=0, flag_d=0, flag_r=0, flag_R=0, flag_f=0, flag_t=0;
    char* dir_r = NULL;

    for(int i=0; i<argc; i++){
        if(strcmp(argv[i], "-h") == 0) flag_h = i;
        if(strcmp(argv[i], "-p") == 0) flag_p = i;
        if(strcmp(argv[i], "-d") == 0) flag_d = i;
        if(strcmp(argv[i], "-r") == 0) flag_r = i;
        if(strcmp(argv[i], "-R") == 0) flag_R = i;
        if(strcmp(argv[i], "-f") == 0) flag_f = i;
        if(strcmp(argv[i], "-t") == 0) flag_t = i;
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
            dir_r = malloc((strlen(argv[flag_d+1])+1)*sizeof(char));
            strcpy(dir_r, argv[flag_d+1]);
        }   
    }

    if(flag_f > 0){
        if(arg_f(argv[flag_f+1]) == -1)
            return 0;
    }else{
        fprintf(stderr, "Impossible to satisfy the request if the name of socket is not declared\n");
        return 0;
    }

    long tempo = 0;
    if(flag_t > 0){
        if(isNumber(argv[flag_t+1], &tempo) != 0){
            fprintf(stderr, "%s is not a number \n", argv[flag_t+1]);
            return 0;
        }else tempo = tempo/1000;
    }

    int opt; 
    while((opt = getopt(argc, argv, ":hf:w:W:D:r:R:d:t:l:u:c:p")) != -1){ //opstring contine le opzioni che vogliamo gestire
        //se getop trova una delle opzioni ritrona un intero (relativo al carattere letto) quindi posso fare lo switch
        char dir_w[MAX_DIR_LEN];
        long numFileToSend;
        switch(opt){
            case 'h':
                break;
            case 'f':
                break;
            case 'w':
                if(parse_w(optarg, dir_w, &numFileToSend) == -1){
                    if(print_flag)
                        fprintf(stderr, "Impossible to parse -w correctly\n");
                    break;
                }
                arg_w(dir_w, &numFileToSend);
                break;
            case 'W':
                arg_W(optarg);
                break; 
            case 'D':
                fprintf(stderr, "Operation -D not supported\n");
                break;
            case 'r':
                arg_r(optarg, dir_r);
                break;
            case 'R':
                arg_R(optarg, dir_r);
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
                arg_c(optarg);
                break;
            case 'p':
                break;
            case ':'://quando l'operazione da eseguire appartiene a quelle da poter eseguire ma manca l'argomento
                switch(optopt){ //optopt variabile interna di getopt che contiene le operazioni che non sono gestite
                    case 'R':
                        arg_R("0", dir_r);
                        break;
                    default:
                        fprintf(stderr, "Option '-%c' needs an argoument\n", optopt); 
                } break;
            break;
            case '?':{ //getopt ritrona ? quando l'operazione da eseguire non appartiene alla opstring
                fprintf(stderr, "'-%c' is an invalid option \n", optopt); 
            } break;
            default:;
        }
        if(tempo != 0) sleep(tempo);
    }
    if(closeConnection(argv[flag_f+1]) == -1){
        perror("ERROR closing connection");
        return (EXIT_FAILURE);
    }

    free(dir_r);
    return 0;
}

//funzione per gestire il comando -f
int arg_f(char* s_name){
    strcpy(socketname, s_name);
    struct timespec abstime;
    clock_gettime(CLOCK_REALTIME, &abstime);
    abstime.tv_sec += 5;
    CHECK_EQ_RETURN(openConnection(socketname, 2000, abstime), -1, "Opening connection", -1);
    return 0;
}

//funzione per controllare e parsare il comando -w
int parse_w(char* optarg, char* dirname, long* fileToSend){
    if(optarg[0] == ','){
        fprintf(stderr, "directory name can't be omitted\n");
        return -1;
    }
    int i=0;
    while(optarg[i] != '\0' && optarg[i] != ',')    i++;
    if(optarg[i] == '\0'){
        strncpy(dirname, optarg, i+1);
        *fileToSend = LONG_MAX;
    }
    else{
        strncpy(dirname, optarg, i);  
        char *numFile = calloc(strlen(optarg) + i, sizeof(char));
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

//funzione per gestire il comando -w
int arg_w(char* dirname, long* fileToSend){
    //controllo se dirname è effettivamente una directory
    struct stat statbuf;
    int r;
    char resolvedpath[PATH_MAX];
    if((r = stat(dirname, &statbuf)) == -1){
        perror("stat arg_w");
        if(print_flag) fprintf(stderr, "Operation: -w, outcome: negative\n");
    }
    if(!S_ISDIR(statbuf.st_mode)){
        if(print_flag) printf("%s is not a directory\n", dirname);
        return -1;
    }

    DIR* dir;
    struct dirent* file;
    CHECK_EQ_RETURN((dir = opendir(dirname)), NULL, "opendir arg_w", -1);

    while(*fileToSend != 0 && (errno = 0, file = readdir(dir)) != NULL){
        char path[PATH_MAX];
		snprintf(path, sizeof(path), "%s/%s", dirname, file->d_name);

        struct stat info;
        CHECK_EQ_RETURN(stat(path, &info), -1, "stat arg_w", -1);
        if(S_ISDIR(info.st_mode)){
            if (strcmp(file->d_name,".")==0 || strcmp(file->d_name,"..")==0) continue;
			arg_w(path, fileToSend);
        }else{
            char* res;
            CHECK_EQ_RETURN((res = realpath(path, resolvedpath)), NULL, "realpath arg_w", -1);
            if(openFile(resolvedpath, O_CREATE) != 0){
                perror("ERROR creating file");
                if(print_flag) fprintf(stderr, "Operation: -w, outcome: negative\n");
                continue;
            }
            if(writeFile(resolvedpath, NULL) != 0){
                perror("ERROR writing file");
                if(print_flag) fprintf(stderr, "Operation: -w, outcome: negative\n");
                removeFile(resolvedpath);
                continue;
            }
            if(closeFile(resolvedpath) != 0) perror("ERROR closing file");
        
            *fileToSend = *fileToSend - 1;
            if(print_flag){
                fprintf(stdout, "Operation: -w, outcome: positive, writen file: %s\n", path);
            }
        }
    }
    
    if(errno != 0) perror("readdir");
    closedir(dir);
    return 0;
}

//funzione per gestire il comando -W
int arg_W(char* optarg){
    char* tmpstr;
    char* token = strtok_r(optarg, ",", &tmpstr);
    char resolvedpath[PATH_MAX];
    while(token){
        char* file = token;
        char* res;
        if((res = realpath(file, resolvedpath)) ==  NULL){
            perror("ERROR realpath");
            if(print_flag) fprintf(stderr, "Operation: -W, file: %s, outcome: negative\n", file);
            token = strtok_r(NULL, ",", &tmpstr);
            continue;
        }
        struct stat info_file;
        stat(resolvedpath, &info_file);
        if (S_ISREG(info_file.st_mode)) {
            if(openFile(resolvedpath, O_CREATE) != 0){
                perror("ERROR creating file");
                if(print_flag) fprintf(stderr, "Operation: -W, file: %s, outcome: negative\n", file);
                token = strtok_r(NULL, ",", &tmpstr);
                continue;
            }
            if(writeFile(resolvedpath, NULL) != 0){
                perror("ERROR writing file");              
                if(print_flag) fprintf(stderr, "Operation: -W, file: %s, outcome: negative\n", file);
                token = strtok_r(NULL, ",", &tmpstr);
                continue;
            }
            if(closeFile(resolvedpath) != 0) perror("ERROR closing file");
        }
        token = strtok_r(NULL, ",", &tmpstr);
        if(print_flag) fprintf(stdout, "Operation: -W, outcome: positive, writen file: %s\n", file);
    }
    return 0;
}

//funzione per gestire il comando -r
int arg_r(char* optarg, char* dir){
    char* tmpstr;
    char* token = strtok_r(optarg, ",", &tmpstr);
    char resolvedpath[PATH_MAX];
    while(token){
        char* res;
        if((res = realpath(token, resolvedpath)) ==  NULL){
            perror("ERROR realpath");
            if(print_flag) fprintf(stderr, "Operation: -r, file: %s, outcome: negative\n", token);
            token = strtok_r(NULL, ",", &tmpstr);
            continue;
        }
        if(openFile(resolvedpath, 0) != 0){
            perror("ERROR opening file");
            if(print_flag) fprintf(stderr, "Operation: -r, file: %s, outcome: negative\n", token);
            token = strtok_r(NULL, ",", &tmpstr);
            continue;
        }

        //READ FILE
        char* buf = NULL;
        size_t size;
        if(readFile(resolvedpath,(void**)&buf, &size) != 0){
            perror("ERROR reading file");
            if(print_flag) fprintf(stderr, "Operation: -r, file: %s, outcome: negative\n", token);
            token = strtok_r(NULL, ",", &tmpstr);
            continue;
        }
        if(dir != NULL){
            char* filename = basename(resolvedpath);
            char path[PATH_MAX];
            sprintf(path, "%s/%s", dir, filename);
            mkdir(dir, 0777);
            FILE* f;
            //CREA FILE SE NON ESISTE
            if((f = fopen(path, "w")) == NULL){
                perror("fopen");
                if(print_flag) printf("Request to write the file %s on disk failed\n", token);
                token = strtok_r(NULL, ",", &tmpstr);
                continue;
            }else{
                fprintf(f, "%s", buf);
                fclose(f);
            }
            free(buf);
    	}
        if(closeFile(resolvedpath) != 0) perror("ERROR closing file");
        if(print_flag) fprintf(stdout, "Operation: -r, outcome: positive, readen files: %s, size read: %ld\n", token, size);
        token = strtok_r(NULL,",",&tmpstr);
    }
    return 0;
}

//funzione per gestire il comando -c
int arg_c(char* optarg){
    char* tmpstr;
    char* token = strtok_r(optarg, ",", &tmpstr);
    char resolvedpath[PATH_MAX];
    while(token){
        char* file = token;
        char* res;
        if((res = realpath(file, resolvedpath)) ==  NULL){
            perror("ERROR realpath");
            if(print_flag) fprintf(stderr, "Operation: -c, file: %s, outcome: negative\n", file);
            token = strtok_r(NULL, ",", &tmpstr);
            continue;
        }
        if(removeFile(resolvedpath) != 0){
            perror("ERROR removing file");
            if(print_flag) fprintf(stderr, "Operation: -c, file: %s, outcome: negative\n", file);
            token = strtok_r(NULL, ",", &tmpstr);
            continue; 
        }
        if(print_flag) fprintf(stdout, "Operation: -c, outcome: positive, removed file: %s\n", file);
        token = strtok_r(NULL, ",", &tmpstr);
    }
    return 0;
}

//funzione per gestire il comando -R
int arg_R(char* optarg, char* dir){
    long n;
    if(isNumber(optarg, &n) != 0){
        if(print_flag) fprintf(stderr, "Operation: -R, outcome: negative\n");
        return -1;
    }
    if((n = readNFiles(n, dir)) == -1){
        perror("ERROR reading N file");
        if(print_flag) fprintf(stderr, "Operation: -R, outcome: negative\n");
    }else{
        if(print_flag) fprintf(stderr, "Operation: -R, outcome: positive, read files: %lu\n", n);
    }    
    return 0;
}
