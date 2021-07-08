#ifndef ICL_HASH_H
#define ICL_HASH_H

#include<stdlib.h>
#include<stdio.h>

#define HASHTABLE_INIT(numbuckets) hashtableInit(numbuckets, NULL, NULL)

typedef struct Node{
	void* key;
	void* data;
	size_t datasize;
	struct Node* next;
}Node_t;

typedef struct Hashtable{
	size_t numbuckets;
	Node_t** buckets; 
	int (*hashFunction) (const void*);
	int (*hashCompare) (const void*, const void*);
}Hashtable_t;

unsigned int hashFunction(const void*);

int hashCompare(const void*, const void*);

Node_t* createNode(const void*, size_t, const void*, size_t);

void printNode(const Node_t*);

Hashtable_t* hashtableInit(size_t, unsigned int (*hash_function) (const void*), int (*hash_compare) (const void*, const void*));

int hashtableInsert(Hashtable_t*, const void*, size_t, const void*, size_t);

void* hashtableGetEntry(const Hashtable_t*, const void*);

void* hashtableFind(const Hashtable_t*, const void*);

int hashtableDeleteNode(Hashtable_t*, const void*);

void hashtableFree(Hashtable_t*);

void hashtablePrint(const Hashtable_t*);

#endif