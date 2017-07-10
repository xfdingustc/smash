
#include <stdlib.h>
#include <stdio.h>

// _Znwj
void* operator new(size_t size) throw()
{
	return malloc(size);
}

// _ZdlPv
void operator delete(void *ptr) throw()
{
	free(ptr);
}

// _Znaj
void* operator new[](size_t size) throw()
{
	return malloc(size);
}

// _ZdaPv
void operator delete[](void *ptr) throw()
{
	free(ptr);
}

extern "C" void __cxa_pure_virtual(void)
{
	printf("__cxa_pure_virtual called");
	*(int*)0=0;	// force seg fault
}

