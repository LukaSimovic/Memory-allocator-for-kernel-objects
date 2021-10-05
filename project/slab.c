#include"slab.h"
#include"buddyAllocator.h"
#include"helper.h"
#include<math.h>
#include<stdio.h>

HANDLE forInfo;

void kmem_init(void* space, int block_num)
{
	if (space == 0 || block_num < 0) 
		return; 
	
		

	buddyInit(space, block_num); 

	
	double s = sizeof(kmem_cache_t) * (NUMBER_OF_BUFFER + 1);
	int numOfBlocks = ceil(s / BLOCK_SIZE);
	void* p = buddyAlloc(numOfBlocks);
	if (p == 0)
		return;

	cacheOfCaches = (kmem_cache_t*)p;
	p = (char*)p + sizeof(kmem_cache_t);

	cacheOfCaches->next = NULL;
	lastBC = cacheOfCaches;

	cacheOfCaches->name = "cacheOfCaches";
	cacheOfCaches->ctor = NULL;
	cacheOfCaches->dtor = NULL;
	cacheOfCaches->objectSize = sizeof(kmem_cache_t);

	cacheInit(cacheOfCaches); 
	
	cacheOfCaches->h = createMutex();



	bufferCaches = (kmem_cache_t*)p;
	p = (char*)p + NUMBER_OF_BUFFER * sizeof(kmem_cache_t);
	for (int i = 0; i < NUMBER_OF_BUFFER; i++) {
		
		lastBC->next = &bufferCaches[i];
		(&bufferCaches[i])->next = NULL;
		lastBC = &bufferCaches[i];

		(&bufferCaches[i])->name = "size2^N"; // N= 5..17
		(&bufferCaches[i])->objectSize = pow(2, (double)i + MIN_BUFFER);
		(&bufferCaches[i])->ctor = NULL;
		(&bufferCaches[i])->dtor = NULL;

		cacheInit(&bufferCaches[i]); //ostatak inic (ono sto nema veze sa argumentima i listom keseva)

		(&bufferCaches[i])->h = createMutex();
	}

	lastC = NULL;
	forInfo = createMutex();

}


kmem_cache_t* kmem_cache_create(const char* name, size_t size,
	void (*ctor)(void*),
	void (*dtor)(void*)) {

	kmem_cache_t* lc = kmem_cache_alloc(cacheOfCaches);

	if (lc == 0) 
		return 0;

	if (size <= 0) {
		lc->error = 1;
		return lc;
	}

	lc->name = name;
	lc->objectSize = size;
	if(lastBC->next==NULL)
		lastBC->next = lc;
	if (lastC != NULL)
		lastC->next = lc;
	lastC = lc;
	lc->next = NULL;
	

	lc->ctor = ctor;
	lc->dtor = dtor;

	cacheInit(lc); //ostatak inic (ono sto nema veze sa argumentima i listom keseva)

	lc->h = createMutex();

	return lc;
}


void* kmalloc(size_t size) {

	if (size < 0)
		return 0;

	int ind = ceil(log2(size));
	if (ind > MAX_BUFFER)
		return 0;
	else if (ind < MIN_BUFFER)
		ind = MIN_BUFFER;
	
	return kmem_cache_alloc(&bufferCaches[ind - MIN_BUFFER]);

}



void* kmem_cache_alloc(kmem_cache_t* cachep) {

	if (cachep == 0)
		return 0;

	wait(cachep->h);

	void* ret = 0;
	int ind;
	Slab* sl;
	if ((sl = cachep->partial) != NULL) {
		ind = sl->free;
		sl->free = sl->freeArray[sl->free];
		sl->freeArray[ind] = -5;
		sl->busy++;
		if (sl->busy == cachep->numObjInSlab) {
			moveSlabPartialToFull(sl, cachep);
		}
	}
	else if ((sl = cachep->empty) != NULL) {
		ind = sl->free;
		sl->free = sl->freeArray[sl->free];
		sl->freeArray[ind] = -5;
		sl->busy++;
		if (sl->busy == cachep->numObjInSlab) {
			moveSlabEmptyToFull(sl, cachep);
		}
		else {
			moveSlabEmptyToPartial(sl, cachep);
		}
	}
	else { //alociraj novu empty plocu, sve ploce su pune
		int ret = newEmptySlabAlloc(cachep);
		if (ret == 0) {
			cachep->error = 2;
			signal(cachep->h);
			return 0;
		}
		else {
			cachep->change = 1;
			sl = cachep->empty;
			if (sl == NULL) {
				cachep->error = 3;
				signal(cachep->h);
				return 0;
			}
			ind = sl->free;
			sl->free = sl->freeArray[sl->free];
			sl->freeArray[ind] = -5;
			sl->busy++;
			if (sl->busy == cachep->numObjInSlab) {
				moveSlabEmptyToFull(sl, cachep);
			}
			else {
				moveSlabEmptyToPartial(sl, cachep);
			}
		}
	}

	ret = (char*)sl->startAddress + ind * cachep->objectSize;

	signal(cachep->h);
	return ret;

}


void kfree(const void* objp) {

	if (objp == 0)
		return;


	kmem_cache_t* pom = NULL;
	Slab* cur = NULL;
	int find = 0;
	for (int i = 0; i < NUMBER_OF_BUFFER; i++) {
		pom = &bufferCaches[i];
		wait(pom->h);
		cur= pom->full;
		while (cur != NULL) {
			if (cur->startAddress <= objp && objp < ((char*)cur->startAddress + pom->numObjInSlab * pom->objectSize)) {
				find = 1; break;
			}
			cur = cur->next;
		}
		if (find == 1)
			break;
		cur = pom->partial;
		while (cur != NULL) {
			if (cur->startAddress <= objp && objp < ((char*)cur->startAddress + pom->numObjInSlab * pom->objectSize)) {
				find = 1; break;
			}
			cur = cur->next;
		}
		if (find == 1)
			break;

		signal(pom->h);
	}

	if (find == 1) {
		

		int ind = ((char*)objp - (char*)cur->startAddress) / pom->objectSize;
		int oldFree = cur->free;
		cur->free = ind;
		cur->freeArray[ind] = oldFree;

		int oldFill = cur->busy;
		cur->busy--;
		if (oldFill == pom->numObjInSlab) {
			if (cur->busy == 0) { 
				moveSlabFullToEmpty(cur, pom);
			}
			else { 
				moveSlabFullToPartial(cur, pom);
			}
		}
		else {
			if (cur->busy == 0) {
				moveSlabPartialToEmpty(cur, pom);
			}
		}

		if (pom->dtor != NULL)
			(pom->dtor)(objp);
		if (pom->ctor != NULL)
			(pom->ctor)(objp);

		signal(pom->h);
	}

}

void kmem_cache_free(kmem_cache_t* cachep, void* objp) {

	if (cachep==0)
		return;

	wait(cachep->h);

	if (objp == 0) {
		cachep->error = 4;
		signal(cachep->h);
		return;
	}

	
	int find = 0;
	Slab* cur = cachep->full;
	while (cur != NULL) {
		if (cur->startAddress <= objp && objp < ((char*)cur->startAddress + cachep->numObjInSlab * cachep->objectSize)) {
			find = 1; break;
		}
		cur = cur->next;
	}
	if (find == 0) {
		cur = cachep->partial;
		while (cur != NULL) {
			if (cur->startAddress <= objp && objp < ((char*)cur->startAddress + cachep->numObjInSlab * cachep->objectSize)) {
				find = 1; break;
			}
			cur = cur->next;
		}
	}

	if (find == 1) {

		int ind = ((char*)objp - (char*)cur->startAddress) / cachep->objectSize;
		int oldFree = cur->free;
		cur->free = ind;
		cur->freeArray[ind] = oldFree;

		int oldFill = cur->busy;
		cur->busy--;
		if (oldFill == cachep->numObjInSlab) {
			if (cur->busy == 0) { 
				moveSlabFullToEmpty(cur, cachep);
			}
			else { 
				moveSlabFullToPartial(cur, cachep);
			}
		}
		else {
			if (cur->busy == 0) {
				moveSlabPartialToEmpty(cur, cachep);
			}
		}

		if (cachep->dtor != NULL)
			(cachep->dtor)(objp);
		if (cachep->ctor != NULL)
			(cachep->ctor)(objp);
		
	}
	else { //find=0
		cachep->error = 5;
	}

	signal(cachep->h);

}


int kmem_cache_shrink(kmem_cache_t* cachep) {

	if (cachep == 0)
		return 0;


	wait(cachep->h);

	if (cachep->change == 0 || cachep->firstCallShrink==0) {
		if (cachep->firstCallShrink == 0) {
			cachep->firstCallShrink = 1;
			cachep->change = 0;
		}
		int ret = 0;
		Slab* cur = cachep->empty,*tmp;
		while (cur != NULL) {
			

			cachep->empty = cur->next;
			tmp = cur;
			cur = cur->next;
			tmp->next = NULL;
			if (tmp == cachep->lastE)
				cachep->lastE = NULL;

			int block = ((char*)cur - (char*)pb->startAddress) / BLOCK_SIZE; 
			int num = cachep->numBlocksInSlab;
			buddyFree(block, num);

			ret += num;
		}

		signal(cachep->h);
		return ret;

	}
	else {

		cachep->change = 0;

		signal(cachep->h);
		return 0;

	}
}

void kmem_cache_destroy(kmem_cache_t* cachep) {
	
	if (cachep == 0)
		return;

	wait(cachep->h);

	
	Slab* cur = cachep->full,*tmp;	
	while (cur != NULL) {
		void* st = cur->startAddress;
		for (int i = 0; i < cachep->numObjInSlab; i++) {
			if(cachep->dtor!=NULL)
				(cachep->dtor)(st);
			//zovem samo destruktor, objekat ne ostavljam u inic stanju
			st = (char*)st + cachep->objectSize;
		}

		cachep->full = cur->next;
		tmp = cur;
		cur = cur->next;
		tmp->next = NULL;
		int block = ((char*)cur - (char*)pb->startAddress) / BLOCK_SIZE;
		if (block < 0) {
			cachep->error = 7;
			signal(cachep->h);
			return;
		}
		int num = cachep->numBlocksInSlab;
		buddyFree(block, num);
	}

	cur = cachep->partial;
	while (cur != NULL) {
		void* st = cur->startAddress;
		int hasMoreFree = 1;
		for (int i = 0; i < cachep->numObjInSlab; i++) {
			if (cur->freeArray[i] == -5) { //zauzeto
				if (cachep->dtor != NULL)
					(cachep->dtor)(st);
				
			}

			st = (char*)st + cachep->objectSize;
		}

		cachep->partial = cur->next;

		tmp = cur;
		cur = cur->next;
		tmp->next = NULL;
		int block = ((char*)cur - (char*)pb->startAddress) / BLOCK_SIZE;

		if (block < 0) {
			cachep->error = 7;
			signal(cachep->h);
			return;
		}
		int num = cachep->numBlocksInSlab;
		buddyFree(block, num);
	}

	
	cur = cachep->empty; int i = 0;
	while (cur != NULL) {
	
		
		cachep->empty = cur->next;
		tmp = cur;
		cur = cur->next;
		tmp->next = NULL;
		int block = ((char*)tmp - (char*)pb->startAddress) / BLOCK_SIZE;

		if (tmp == cachep->lastE)
			cachep->lastE = NULL;

		if (block < 0) {
			cachep->error = 7;
			signal(cachep->h);
			return;
		}
		int num = cachep->numBlocksInSlab;
		int powN = ceil(log2(num));

		buddyFree(block, powN);

	}

	int notFree = 0;
	for (int i = 0; i < NUMBER_OF_BUFFER; i++) {
		if (&bufferCaches[i] == cachep || cacheOfCaches == cachep) {
			notFree = 1;
			break;
		}
	}
	
	signal(cachep->h);

	if (notFree == 0) {
		kmem_cache_t* tmp = lastBC->next,*prev=lastBC;
		while (tmp != NULL) {
			if (tmp == cachep) {
				break;
			}
			prev = tmp;
			tmp = tmp->next;
		}
		if (tmp == lastC) {
			lastC = prev;
		}
		if (tmp != NULL) {
			prev->next = tmp->next;
			tmp->next = NULL;
		}

		wait(cacheOfCaches->h);

		int find = 0;
		Slab* cur = cacheOfCaches->full;
		while (cur != NULL) {
			if (cur->startAddress <= (void*)cachep && (void*)cachep < ((char*)cur->startAddress + cacheOfCaches->numObjInSlab * cacheOfCaches->objectSize)) {
				find = 1; break;
			}
			cur = cur->next;
		}
		if (find == 0) {
			cur = cacheOfCaches->partial;
			while (cur != NULL) {
				if (cur->startAddress <= (void*)cachep && (void*)cachep < ((char*)cur->startAddress + cacheOfCaches->numObjInSlab * cacheOfCaches->objectSize)) {
					find = 1; break;
				}
				cur = cur->next;
			}
		}

		if (find == 1) {
			int ind = ((char*)cachep- (char*)cur->startAddress) / cacheOfCaches->objectSize;
			int oldFree = cur->free;
			cur->free = ind;
			cur->freeArray[ind] = oldFree;

			int oldFill = cur->busy;
			cur->busy--;
			if (oldFill == cacheOfCaches->numObjInSlab) {
				if (cur->busy == 0) { 
					moveSlabFullToEmpty(cur, cacheOfCaches);
				}
				else { 
					moveSlabFullToPartial(cur, cacheOfCaches);
				}
			}
			else {
				if (cur->busy == 0) { 
					moveSlabPartialToEmpty(cur, cacheOfCaches);
				}
			}

			if (cacheOfCaches->dtor != NULL)
				(cacheOfCaches->dtor)(cachep);
			//if (cachep->ctor != NULL) KES SE UNISTAVA
			//	(cachep->ctor)(cachep); NE OSTAVLJAM GA U INIC STANJU
		}
		
		signal(cacheOfCaches->h);
	}
	else {
		cachep->error = 6;
	}
}


void kmem_cache_info(kmem_cache_t* cachep) {

	wait(forInfo);

	if (cachep == 0) {
		signal(forInfo);
		return;
	}

	wait(cachep->h);

	int busyObjects = 0, numOfSlubs = 0;
	Slab* pom; 

	//empty
	pom = cachep->empty;
	while (pom != NULL) {
		pom = pom->next;
		numOfSlubs = numOfSlubs + 1;
	}
	//partial
	pom = cachep->partial;
	while (pom != NULL) {
		busyObjects += pom->busy;
		pom = pom->next;
		numOfSlubs = numOfSlubs + 1;

	}
	//full
	pom = cachep->full;
	while (pom != NULL) {
		busyObjects += cachep->numObjInSlab;
		pom = pom->next;
		numOfSlubs = numOfSlubs + 1;
	}

	double proc = busyObjects / ((double)(cachep->numObjInSlab) * numOfSlubs) * 100;
	
	printf("\n -------------------------------------------------- ");
	printf("\nINFOMRACIJE O KESU:\n");
	printf("Ime: %s\n", cachep->name);
	printf("Velicina objekta u bajtovima: %d\n", cachep->objectSize);
	printf("Velicina kesa u broju blokova: %d\n", cachep->numBlocksInSlab*numOfSlubs);
	printf("Broj ploca: %d\n", numOfSlubs);
	printf("Broj objekata u jednoj ploci: %d\n", cachep->numObjInSlab);
	printf("Zauzetost kesa: %f\n ", proc);
	printf(" -------------------------------------------------- \n\n");

	signal(cachep->h);

	signal(forInfo);

}

int kmem_cache_error(kmem_cache_t* cachep) {

	wait(forInfo);

	if (cachep == 0) {
		printf("Lose prosledjen kes funkciji kmem_cache_error\n");

		signal(forInfo);
		return 7;
	}

	wait(cachep->h);

	int e = cachep->error;
	if (e == 0) {
		return 0;
	}
	else if (e == 1) {
		printf("Velicina koja se zadaje funkciji kmem_cache_create mora biti pozitivna vrednost!\n");
	}
	else if (e == 2) {
		printf("Neuspesna alokacija prostora za novu empty plocu!\n");
	}
	else if (e == 3) {
		printf("Greska pri ulancavanju nove empty ploce u listu!\n");
	}
	else if (e == 4) {
		printf("Nekorektna vrednost parametra 'objp' koji se zadaje funkciji kmem_cache_free!\n");
	}
	else if (e == 5) {
		printf("Objekat koji treba da se oslobodi se ne nalazi u zadatom kesu!\n");
	}
	else if (e == 6) {
		printf("Trazeni kes ne moze da bude obrisan! Samo je ispraznjen!\n");
	}
	else if (e == 7) {
		printf("Greska pri odredjivanju broja bloka od kog se vrsi dealokacija memorije!\n");
	}

	signal(cachep->h);
	signal(forInfo);

	return e;

}

