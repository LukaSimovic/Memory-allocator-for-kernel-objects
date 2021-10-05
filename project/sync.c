#include"sync.h"


HANDLE createMutex() {

	HANDLE h = CreateMutex(NULL, FALSE, NULL);
	return h;

}



void wait(HANDLE h) {
	DWORD d = WaitForSingleObject(h, INFINITE);
}


void signal(HANDLE h) {
	ReleaseMutex(h);
}