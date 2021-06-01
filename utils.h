#if !defined(_UTIL_H)
#define _UTIL_H

#define _POSIX_C_SOURCE 200112L
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<string.h>
#include<pthread.h>
#include<unistd.h>
#include<sys/types.h>

#define UNIX_PATH_MAX 108
#define SOMAXCON 100

// macro di utilità per controllare errori
#define CHECK_EQ_EXIT(X, val, str) \
  if ((X)==val) {	\
    perror(str); \
    exit(EXIT_FAILURE); \
  }

#define CHECK_NEQ_EXIT(X, val, str)	\
  if ((X)!=val) { \
    perror(str); \
    exit(EXIT_FAILURE); \
  }

#define CHECK_EQ_RETURN(X, val, str, ret) \
  if((X)==val) { \
    perror(str); \
    return (ret); \
  }

#define LOCK(l) \
  if (pthread_mutex_lock(l)!=0){ \
    fprintf(stderr, "FATAL ERROR lock\n"); \
    pthread_exit((void*)EXIT_FAILURE); \
  }   

#define UNLOCK(l) \
  if (pthread_mutex_unlock(l)!=0){ \
    fprintf(stderr, "FATAL ERROR unlock\n"); \
    pthread_exit((void*)EXIT_FAILURE); \
  }

#define WAIT(c,l) \
  if(pthread_cond_wait(c,l)!=0){ \
    fprintf(stderr, "FATAL ERROR wait\n"); \
    pthread_exit((void*)EXIT_FAILURE); \
  }

/* ATTENZIONE: t e' un tempo assoluto! */
#define TWAIT(c,l,t) { \
    int r=0; \
    if ((r=pthread_cond_timedwait(c,l,t))!=0 && r!=ETIMEDOUT) { \
      fprintf(stderr, "ERRORE FATALE timed wait\n"); \
      pthread_exit((void*)EXIT_FAILURE); \
    } \
  }

#define SIGNAL(c) \
  if (pthread_cond_signal(c)!=0){	\
    fprintf(stderr, "FATALE ERROR signal\n"); \
    pthread_exit((void*)EXIT_FAILURE); \
  }

#define BCAST(c) \
  if (pthread_cond_broadcast(c)!=0){ \
    fprintf(stderr, "FATAL ERROR broadcast\n"); \
    pthread_exit((void*)EXIT_FAILURE); \
  }


/** 
 * @brief Controlla se la stringa passata come primo argomento e' un numero.
 * @return  0 ok  1 non e' un numbero   2 overflow/underflow
 */
int isNumber(const char*, long*);

ssize_t readn(int fd, void *ptr, size_t n);

ssize_t writen(int fd, void *ptr, size_t n);


#endif /* _UTIL_H */