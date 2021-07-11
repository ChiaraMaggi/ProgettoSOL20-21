#include<errno.h>
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<assert.h>
#include <limits.h>

#include "icl_hash.h"

#define BITS_IN_int     ( sizeof(int) * CHAR_BIT )
#define THREE_QUARTERS  ((int) ((BITS_IN_int * 3) / 4))
#define ONE_EIGHTH      ((int) (BITS_IN_int / 8))
#define HIGH_BITS       ( ~((unsigned int)(~0) >> ONE_EIGHTH ))

unsigned int hashFunction(const void* key){
    char *datum = (char*)key;
    unsigned int hashvalue, i;
    if(!datum) return 0;
    for (hashvalue = 0; *datum; ++datum) {
        hashvalue = (hashvalue << ONE_EIGHTH) + *datum;
        if ((i = hashvalue & HIGH_BITS) != 0)
            hashvalue = (hashvalue ^ (i >> THREE_QUARTERS)) & ~HIGH_BITS;
    }
    return (hashvalue);
}


int hashCompare(const void* a, const void* b){
	return strcmp((char*) a, (char*) b);
}


Node_t* createNode(const void* key, size_t keysize, const void* data, size_t datasize){
	Node_t* node;
	node = (Node_t*)malloc(sizeof(Node_t));
	node->key = malloc(keysize);
	if (data != NULL && datasize > 0){
		node->data = malloc(datasize);
		memcpy(node->data, data, datasize);
		node->datasize = datasize;
	}else{
		node->data = NULL;
		node->datasize = 0;
	}
	memcpy(node->key, key, keysize);
	node->next = NULL;
	return node;
}


void printNode(const Node_t* node){
	if (node == NULL) fprintf(stdout, "NULL\n");
	else{
		fprintf(stdout, "[%s] -> ", (char*) node->key);
		printNode(node->next);
	}
}


Hashtable_t* hashtableInit(size_t numbuckets, unsigned int (*hash_function) (const void*), int (*hash_compare) (const void*, const void*)){
	Hashtable_t* hashtable;
	hashtable = (Hashtable_t*) malloc(sizeof(Hashtable_t));
	hashtable->numbuckets = numbuckets;
	hashtable->buckets = (Node_t**)malloc(sizeof(Node_t*) * numbuckets);
	if (hashtable->buckets == NULL){
		perror("malloc");
		return NULL;
	}
	for (size_t i = 0; i < numbuckets; i++) hashtable->buckets[i] = NULL;
	hashtable->hashFunction = ((hash_function == NULL) ? ((const void*)hashFunction) : (hash_function));
	hashtable->hashCompare = ((hash_compare == NULL) ? (hashCompare) : (hash_compare));
	return hashtable;
}


int hashtableInsert(Hashtable_t* hashtable, const void* key, size_t keysize, const void* data, size_t datasize){
	if (key == NULL || keysize == 0) return -1;
	size_t hash = hashtable->hashFunction(key) % hashtable->numbuckets;
	for (Node_t* curr = hashtable->buckets[hash]; curr != NULL; curr = curr->next){
		if (hashtable->hashCompare(curr->key, key) == 0) // duplicates are not allowed
			return 0;
	}
	Node_t* node;
	node = createNode(key, keysize, data, datasize);
	if (node == NULL){
		errno = ENOMEM;
		return -1;
	}
	node->next = hashtable->buckets[hash];
	hashtable->buckets[hash] = node;
	return 0;
}

void* hashtableGetNode(const Hashtable_t* hashtable, const void* key){
	if (hashtable == NULL) return NULL;
	size_t hash = hashtable->hashFunction(key) % hashtable->numbuckets;
	Node_t* node = hashtable->buckets[hash];
	while (node != NULL){
		if (hashtable->hashCompare(node->key, key) == 0){
			void* data = malloc(sizeof(node->datasize));
			if (data == NULL){
				errno = ENOMEM;
				return NULL;
			}
			memcpy(data, node->data, node->datasize);
			return data;
		}
		node = node->next;
	}
	return NULL;
}

void* hashtableFind(const Hashtable_t* hashtable, const void* key){
	if (hashtable == NULL) return NULL;
	size_t hash = hashtable->hashFunction(key) % hashtable->numbuckets;
	Node_t* node = hashtable->buckets[hash];
	while (node != NULL){
		if (hashtable->hashCompare(node->key, key) == 0) return node->data;
		node = node->next;
	}
	return NULL;
}

int hashtableDeleteNode(Hashtable_t* hashtable, const void* key){
	if (hashtableFind(hashtable, key) == 0) return 0; // nothing to delete
	size_t hash = hashtable->hashFunction(key) % hashtable->numbuckets;
	Node_t* curr = hashtable->buckets[hash];
	Node_t* prec = NULL;
	while(curr != NULL){
		// this node is the one to be deleted
		if (hashtable->hashCompare(curr->key, key) == 0){
			if (prec == NULL){ // curr is the first node
				hashtable->buckets[hash] = curr->next;
				free(curr->key);
				if (curr->data != NULL) free(curr->data);
				free(curr);
				return 1;
			}
			if (curr->next == NULL){ // curr is the last node
				prec->next = NULL;
				free(curr->key);
				if (curr->data != NULL) free(curr->data);
				free(curr);
				return 1;
			}
			prec->next = curr->next;
			free(curr->key);
			if (curr->data != NULL) free(curr->data);
			free(curr);
			return 1;
		}
		prec = curr;
		curr = curr->next;
	}
	return 0;
}

void hashtableFree(Hashtable_t* hashtable){
	if (hashtable == NULL) return;
	for (size_t i = 0; i < hashtable->numbuckets; i++){
		Node_t* curr = hashtable->buckets[i];
		Node_t* tmp;
		while (curr != NULL){
			tmp = curr;
			curr = curr->next;
			free(tmp->key);
			if (tmp->data != NULL) free(tmp->data);
			free(tmp);
		}
	}
	free(hashtable->buckets);
	free(hashtable);
}

void hashtablePrint(const Hashtable_t* hashtable){
    Node_t *bucket, *curr;
    int i;
    if(!hashtable) return;
    for(i=0; i<hashtable->numbuckets; i++) {
        bucket = hashtable->buckets[i];
        for(curr=bucket; curr!=NULL; ) {
                fprintf(stdout, "%s\n", (char *)curr->key);
            curr=curr->next;
        }
    }
    return;
}


	