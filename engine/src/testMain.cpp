#include <stdio.h>

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

int main(int argc, char **argv) {
	printf("\n\nHello, world\n");
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_DEBUG );  

	int *array = (int *)malloc(sizeof(int) * 100);

	_CrtDumpMemoryLeaks();  
	return 0;
	// return array[argc];  // BOOM
}
