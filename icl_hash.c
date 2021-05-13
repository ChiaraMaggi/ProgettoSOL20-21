#include<stdio.h>
#include<stdlib.h>
#include<assert.h>
#include<malloc.h>
#include "queue.h"

typedef struct HashTable{
    size_t size;
    Queue_t* buckets;
    size_t (*hash)(struct HashTable_t*, int);
}HashTable_t;

typedef size_t (*hashFunction_t)(HashTable_t*, int);

size_t hash(HashTable_t* hashtable, int key) {
    return (key % 100002271) % hashtable->size;
}

HashTable_t* initHashTable(size_t size, hashFunction_t hash) {
    HashTable_t* hashtable = malloc(sizeof(*hashtable));
    assert(hashtable);

    hashtable->size = size;
    hashtable->hash = hash;

    hashtable->buckets = malloc(size * sizeof(*(hashtable->buckets)));
    assert(hashtable->buckets);

    for (size_t i = 0; i < size; i++) {
        hashtable->buckets[i] = *initQueue();
    }
    return hashtable;
}

void freeHashTable(HashTable_t** hashtablePtr) {
    for (size_t i = 0; i < (*hashtablePtr)->size; i++) {
        deleteQueue(&((*hashtablePtr)->buckets[i]));
    }
    free((*hashtablePtr)->buckets);
    free((*hashtablePtr));
    *hashtablePtr = NULL;
}

void insertHashTable(HashTable_t* hashtable, int value) {
    size_t h = hashtable->hash(hashtable, value);
    //finire
}

void deleteHashTable(HashTable_t* hashtable, int value) {
    size_t h = hashtable->hash(hashtable, value);

    int index = List_findElement(hashtable->buckets[h], value);
    if (index == -1) {
        List_deleteByIndex(hashtable->buckets[h], index);
    }
}

void HashTable_print(HashTable_t* hashtable) {
    for (size_t i = 0; i < hashtable->size; i++) {
        List_print(hashtable->buckets[i]);
    }
}
