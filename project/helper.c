#include"helper.h"
#include"slab.h"
#include"buddyAllocator.h"
#include<math.h>

void cacheInit(kmem_cache_t* cache) {

	cache->empty = NULL;
	cache->full = NULL;
	cache->partial = NULL;
	cache->lastE = NULL;

	cache->nextOffset = 0;
	cache->change = 0;
	cache->firstCallShrink = 0;
	cache->error = 0;

	calculateNumberBlocksAndObject(cache);

}


void calculateNumberBlocksAndObject(kmem_cache_t* cache) {

	int power = 0, numBl,numObj,w,find=0;

	while(find==0){
		numBl = pow(2, power);
		numObj = (BLOCK_SIZE * numBl - sizeof(Slab)) / (cache->objectSize + sizeof(int));
		w = (BLOCK_SIZE * numBl - sizeof(Slab)) % (cache->objectSize + sizeof(int));
		if (numObj != 0 /*&& w < BLOCK_SIZE * numBl / 8*/)
			find = 1;
		else
			power++;
				
	}

	cache->numObjInSlab = numObj;
	cache->numBlocksInSlab = numBl;
	cache->intFrag = w;

}

int newEmptySlabAlloc(kmem_cache_t* cache) {

	if (cache == 0)
		return 0;



	void* s = buddyAlloc(cache->numBlocksInSlab);
	if (s == 0)
		return 0;
	Slab* newSlab = (Slab*)s;
	s = (char*)s + sizeof(Slab);

	//hardverski kes poravnanje, offseti
	newSlab->offset = cache->nextOffset;
	int endOff = newSlab->offset + CACHE_L1_LINE_SIZE;
	if (endOff + CACHE_L1_LINE_SIZE <= cache->intFrag)
		cache->nextOffset += CACHE_L1_LINE_SIZE;
	else
		cache->nextOffset = 0;

	newSlab->busy = 0;
	newSlab->free = 0;
	newSlab->freeArray = (int*)s;
	s = (char*)s + cache->numObjInSlab * sizeof(int) + newSlab->offset;
	newSlab->startAddress = s;
	void* pomStAdd = s;
	for (int i = 0; i < cache->numObjInSlab; i++) {
		if (i < cache->numObjInSlab - 1)
			newSlab->freeArray[i] = i + 1;
		else
			newSlab->freeArray[i] = -1;

		if (cache->ctor != NULL) 
			(cache->ctor)(pomStAdd); 

		pomStAdd = (char*)pomStAdd + cache->objectSize;

	}


	//ulancavanje ploce newSlab u empty listu
	newSlab->next = NULL;
	if (cache->lastE == NULL)
		cache->empty = newSlab;
	else
		cache->lastE->next = newSlab;
	cache->lastE = newSlab;



	return 1;
}

void moveSlabEmptyToFull(Slab* sl, kmem_cache_t* cache)
{
	//uklanjanje iz empty liste
	Slab* pom = cache->empty;
	Slab* prev = NULL;
	while (1) {
		if (pom == sl)
			break;
		prev = pom;
		pom = pom->next;
	}
	if (prev != NULL) {
		prev->next = pom->next;
	}
	else {
		cache->empty = pom->next;
	}
	cache->lastE = prev;


	//dodavanje u full listu
	if (cache->full == NULL) { //full lista prazna
		cache->full = sl;
		sl->next = NULL;
	}
	else { //dodavanje na pocetak
		Slab* pom = cache->full;
		cache->full = sl;
		sl->next = pom;
	}

}

void moveSlabEmptyToPartial(Slab* sl, kmem_cache_t* cache)
{
	//uklanjanje iz empty liste
	Slab* pom = cache->empty;
	Slab* prev = NULL;
	while (1) {
		if (pom == sl)
			break;
		prev = pom;
		pom = pom->next;
	}
	if (prev != NULL) {
		prev->next = pom->next;
	}
	else {
		cache->empty = pom->next;
	}
	cache->lastE = prev;


	//dodavanje u partial listu
	if (cache->partial == NULL) { //partial lista prazna
		cache->partial = sl;
		sl->next = NULL;
	}
	else { //dodavanje na pocetak
		Slab* pom = cache->partial;
		cache->partial = sl;
		sl->next = pom;
	}
}

void moveSlabPartialToFull(Slab* sl, kmem_cache_t* cache)
{
	//uklanjanje iz partial liste
	Slab* pom = cache->partial;
	Slab* prev = NULL;
	while (1) {
		if (pom == sl)
			break;
		prev = pom;
		pom = pom->next;
	}
	if (prev != NULL) {
		prev->next = pom->next;
	}
	else {
		cache->partial = pom->next;
	}



	//dodavanje u full listu
	if (cache->full == NULL) { 
		cache->full = sl;
		sl->next = NULL;
	}
	else { //dodavanje na pocetak
		Slab* pom = cache->full;
		cache->full = sl;
		sl->next = pom;
	}

}

void moveSlabFullToEmpty(Slab* sl, kmem_cache_t* cache)
{
	//uklanjanje iz full liste
	Slab* pom = cache->full;
	Slab* prev = NULL;
	while (1) {
		if (pom == sl)
			break;
		prev = pom;
		pom = pom->next;
	}
	if (prev != NULL) {
		prev->next = pom->next;
	}
	else {
		cache->full = pom->next;
	}


	//dodavanje u empty listu
	sl->next = NULL;
	if (cache->lastE == NULL)
		cache->empty = sl;
	else
		cache->lastE->next = sl;
	cache->lastE = sl;
}

void moveSlabFullToPartial(Slab* sl, kmem_cache_t* cache)
{
	//uklanjanje iz full liste
	Slab* pom = cache->full;
	Slab* prev = NULL;
	while (1) {
		if (pom == sl)
			break;
		prev = pom;
		pom = pom->next;
	}
	if (prev != NULL) {
		prev->next = pom->next;
	}
	else {
		cache->full = pom->next;
	}


	//dodavanje u partial listu
	if (cache->partial == NULL) { 
		cache->partial = sl;
		sl->next = NULL;
	}
	else { //dodavanje na pocetak
		Slab* pom = cache->partial;
		cache->partial = sl;
		sl->next = pom;
	}
}

void moveSlabPartialToEmpty(Slab* sl, kmem_cache_t* cache)
{
	//uklanjanje iz partial liste
	Slab* pom = cache->partial;
	Slab* prev = NULL;
	while (1) {
		if (pom == sl)
			break;
		prev = pom;
		pom = pom->next;
	}
	if (prev != NULL) {
		prev->next = pom->next;
	}
	else {
		cache->partial = pom->next;
	}


	//dodavanje u empty listu
	sl->next = NULL;
	if (cache->lastE == NULL)
		cache->empty = sl;
	else
		cache->lastE->next = sl;
	cache->lastE = sl;
}
