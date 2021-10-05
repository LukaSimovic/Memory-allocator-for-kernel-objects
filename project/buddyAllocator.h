#pragma once

#include<Windows.h>

typedef struct {

	void* startAddress;
	int* buddyArray;
	int numOfElem;
	HANDLE h;

}Buddy;


Buddy* pb;


void buddyInit(void* space, int numOfBlocks);
void* buddyAlloc(int numOfBlocks);
int buddyFree(int block, int num);


//pomocne funkcije
void buddyInfo();
void* blockAddress(int block);
int isBuddy(int b1, int b2, int i);
int isPowerOfTwo(int num);

