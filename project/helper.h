#pragma once

#include"slab.h"
#include"sync.h"

#define MIN_BUFFER (5) //minimalna velicina bafera je 2^5
#define MAX_BUFFER (17) //max velicina bafera je 2^17
#define NUMBER_OF_BUFFER (13) //imamo 13 bafera

typedef struct slab Slab;

kmem_cache_t* cacheOfCaches;
kmem_cache_t* bufferCaches;
kmem_cache_t* lastBC;
kmem_cache_t* lastC;

typedef struct slab {

	Slab* next;

	int free; 
	int* freeArray;
	int busy; 

	void* startAddress;
	int offset;

}Slab;

typedef struct kmem_cache_s {

	kmem_cache_t* next;

	const char* name;
	int objectSize;
	int numObjInSlab;
	int numBlocksInSlab;
	int intFrag;
	int nextOffset;
	int firstCallShrink;
	int change;
	int error;

	void(*ctor)(void*);
	void(*dtor)(void*);

	Slab* full;
	Slab* empty;
	Slab* lastE; //jedino se kod empty liste koristi dodavanje na kraj
	Slab* partial;

	HANDLE h;

}kmem_cache_t;



void cacheInit(kmem_cache_t* cache);
int newEmptySlabAlloc(kmem_cache_t* cache);
void moveSlabEmptyToFull(Slab* sl, kmem_cache_t* cache);
void moveSlabEmptyToPartial(Slab* sl, kmem_cache_t* cache);
void moveSlabPartialToFull(Slab* sl, kmem_cache_t* cache);
void moveSlabFullToEmpty(Slab* sl, kmem_cache_t* cache);
void moveSlabFullToPartial(Slab* sl, kmem_cache_t* cache);
void moveSlabPartialToEmpty(Slab* sl, kmem_cache_t* cache);
void calculateNumberBlocksAndObject(kmem_cache_t* cache);


