#include"buddyAllocator.h"
#include"slab.h"
#include<stdio.h>
#include<math.h>
#include"sync.h"


void buddyInfo()
{
	printf("broj nivoa: %d\nNiz:\n", pb->numOfElem);
	for (int i = 0; i < pb->numOfElem; i++) {
		int gl = pb->buddyArray[i];
		printf("%d |%d| ", i, gl);
		while (gl != -1) {
			int* pn = blockAddress(gl);
			gl = *pn;
			printf(" -> %d", gl);
		}
		printf("\n");
	}
}

void* blockAddress(int block) {
	return (char*)pb->startAddress + block * BLOCK_SIZE;
}

void buddyInit(void* space, int numOfBlocks) {

	numOfBlocks--; 

	pb = (Buddy*)space;

	pb->startAddress = (char*)space + BLOCK_SIZE;
	space = (char*)space + sizeof(Buddy);

	pb->numOfElem = floor(log2(numOfBlocks))+1; 
	pb->buddyArray = (int*)space;
	space = (char*)space + pb->numOfElem * sizeof(int);

	for (int pomNum = 0, i = pb->numOfElem - 1; i >= 0; i--) {
		int st = (int)pow(2, i);
		if (numOfBlocks - st >= 0) {
			pb->buddyArray[i] = pomNum;
			*((int*)blockAddress(pomNum)) = -1;
			numOfBlocks -= st;
			pomNum += st;
		}
		else pb->buddyArray[i] = -1;
	}


	pb->h = createMutex();
}


void* buddyAlloc(int numOfBlocks) {

	wait(pb->h);

	int powN = ceil(log2(numOfBlocks));
	if (powN < 0 || powN >= pb->numOfElem) {
		//printf("trazeno parce je prevliko ili <0\n");
		signal(pb->h);
		return 0;
	}

	void* ret = 0;
	while (1) {
		int turn = 0;
		if (pb->buddyArray[powN] > -1) {
			ret = blockAddress(pb->buddyArray[powN]);
			pb->buddyArray[powN] = *((int*)blockAddress(pb->buddyArray[powN]));
			break; //iz while petlje, imamo adresu koju vracamo
		}
		for (int i = powN + 1; i < pb->numOfElem; i++) {
			if (pb->buddyArray[i] > -1) {
				int d1 = pb->buddyArray[i];
				int d2 = d1 + (int)pow(2, ((double)i - 1));

				int* p1 = blockAddress(d1);
				int* p2 = blockAddress(d2);

				pb->buddyArray[i] = *p1;


				int old = pb->buddyArray[i - 1];
				pb->buddyArray[i - 1] = d1;
				*p1 = d2;
				*p2 = old;

				turn = 1; //signal da petlja treba da se nastavi

				break; //iz for petlje
			}
		}
		if (turn == 1) {
			continue; //nastavlja se while petlja
		}

		//printf("TRENUTNO NEMA DOSTUPNOG PARCETA\n");
		break; 

	}


	signal(pb->h);
	return ret; 
}


int isPowerOfTwo(int num) {
	for (int power = 1; power > 0 ; power = power << 1)
	{
		if (power == num) {
			//printf("jeste stepen dvojke");
			return 1;
		}
		if (power > num) {
			//printf("nije stepen dvojke");
			return 0;
		}
	}
}

int isBuddy(int b1, int b2, int i) {
	int min, max;

	if (b1 < b2) {
		min = b1; max = b2;
	}
	else {
		min = b2; max = b1;
	}

	if (min + (int)pow(2, i) == max && min % ((int)(pow(2, (double)i + 1))) == 0)
		return 1;
	else
		return 0;
}

int buddyFree(int block, int numOfBlocks) {

	wait(pb->h);
	
	if (!isPowerOfTwo(numOfBlocks)) { 
		//printf("Broj blokova koji se oslobadja mora biti stepen dvojke!\n");
		signal(pb->h);
		return 0; 
	}

	int powN = ceil(log2(numOfBlocks));

	while (1) {

		int cur = pb->buddyArray[powN]; int prev = -1;
		while (cur != -1) {
			if (isBuddy(cur, block, powN)) {
				break;
			}
			prev = cur;
			cur = *((int*)blockAddress(cur));
		}

		if (cur == -1) { //nema parnjaka -> uvezivanje na pocetak liste i izlazak
			int old = pb->buddyArray[powN];
			pb->buddyArray[powN] = block;
			*((int*)blockAddress(pb->buddyArray[powN])) = old;
			break;
		}
		else { //ima parnjaka - to je blok cur

			int* pc = blockAddress(cur);
			if (prev == -1) { 
				pb->buddyArray[powN] = *pc;
				*pc = -1;
			}
			else {
				int* pp = blockAddress(prev);
				*pp = *pc;
				*pc = -1;
			}
			powN++;
			int min = cur < block ? cur : block;
			block = min;
		}
	}


	signal(pb->h);
	return 1;
} 

