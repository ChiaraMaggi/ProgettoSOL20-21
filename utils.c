#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<unistd.h>
#include<sys/types.h>
#include<fcntl.h>
#include<limits.h>
#include<sys/stat.h>

#include "utils.h"

int isNumber(const char* s, long* n) {
  if (s==NULL) return 1;
  if (strlen(s)==0) return 1;
  char* e = NULL;
  errno=0;
  long val = strtol(s, &e, 10);
  if (errno == ERANGE) return 2;    // overflow/underflow
  if (e != NULL && *e == (char)0) {
    *n = val;
    return 0;   // successo 
  }
  return 1;   // non e' un numero
}

int isdot(const char dir[]){
    int l = strlen(dir);
    if((l>0 && dir[l-1] == '.')) return 1;
    return 0;
}


int mkdir_p(const char *path){
    const size_t len = strlen(path);
    char _path[PATH_MAX];
    char *p; 

    errno = 0;

    /* Copy string so its mutable */
    if (len > sizeof(_path)-1) {
        errno = ENAMETOOLONG;
        return -1; 
    }   
    strcpy(_path, path);

    /* Iterate the string */
    for (p = _path + 1; *p; p++) {
        if (*p == '/') {
            /* Temporarily truncate */
            *p = '\0';

            if (mkdir(_path, S_IRWXU) != 0) {
                if (errno != EEXIST)
                    return -1; 
            }

            *p = '/';
        }
    }   

    if (mkdir(_path, S_IRWXU) != 0) {
        if (errno != EEXIST)
            return -1; 
    }   

    return 0;
}

/** Evita letture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la lettura da fd leggo EOF
 *   \retval size se termina con successo
 */
/* Read "n" bytes from a descriptor */
ssize_t readn(int fd, void* ptr, size_t n) {  
  size_t   nleft;
  ssize_t  nread;

  nleft = n;
  while (nleft > 0) {
    if((nread = read(fd, ptr, nleft)) < 0) {
      if (nleft == n) return -1; /* error, return -1 */
      else break; /* error, return amount read so far */
    } else if (nread == 0) break; /* EOF */
    nleft -= nread;
    ptr   += nread;
  }
   return(n - nleft); /* return >= 0 */
}
 

/** Evita scritture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la scrittura la write ritorna 0
 *   \retval  1   se la scrittura termina con successo
 */
/* Write "n" bytes to a descriptor */
ssize_t writen(int fd, const void* ptr, size_t n) {  
  size_t   nleft;
  ssize_t  nwritten;

  nleft = n;
  while (nleft > 0) {
    if((nwritten = write(fd, ptr, nleft)) < 0) {
      if (nleft == n) return -1; /* error, return -1 */
      else break; /* error, return amount written so far */
    } else if (nwritten == 0) break; 
    nleft -= nwritten;
    ptr   += nwritten;
  }
  return(n - nleft); /* return >= 0 */
}