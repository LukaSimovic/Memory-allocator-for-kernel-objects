#pragma once


#include<Windows.h>


HANDLE createMutex();

void wait(HANDLE h);
void signal(HANDLE h);
