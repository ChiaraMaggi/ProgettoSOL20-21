#ifndef ICL_HASH_H_
#define ICL_HASH_H_

#include<stdio.h>
#include<stdlib.h>
#include<assert.h>
#include<malloc.h>
#include "queue.h"

typedef struct HashTable{
    int size;
    Queue_t* buckets;
}HashTable_t;

typedef size_t (*hashFunction)(HashTable*, int);

size_t hash(HashTable_t* hashtable, int key);

HashTable_t* HashTable_create(size_t size, hashFunction hash);

void HashTable_free(HashTable** hashtablePtr);

void HashTable_insert(HashTable* hashtable, int value);

void HashTable_delete(HashTable* hashtable, int value);

int HashTable_get(HashTable* hashtable, int value);

void HashTable_print(HashTable* hashtable);

#endif