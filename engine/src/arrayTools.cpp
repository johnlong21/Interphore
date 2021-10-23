#include "arrayTools.h"

#define ArrayLength(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

// Only works on things of pointer size!
void arraySwapPop(void **arr, int *len, void *element) {
	for (int i = 0; i < *len; i++) {
		if (arr[i] == element) {
			arraySwapPopIndex(arr, len, i);
			return;
		}
	}
}

// Only works on things of pointer size!
void arraySwapPopIndex(void **arr, int *len, int indexToRemove) {
	arr[indexToRemove] = arr[*len-1];
	*len = *len - 1;
}

// Only works on things of pointer size!
bool arrayContainsPointer(void **arr, int len, void *element) {
	for (int i = 0; i < len; i++)
		if (arr[i] == element)
			return true;

	return false;
}

void arrayPrint2d(int *arr, int width, int height) {
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			printf("%d", (int)arr[y*width + x]);
			if (x != width-1) printf(",");
		}
		printf("\n");
	}
}

void arrayPrint2d(unsigned char *arr, int width, int height) {
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			printf("%d", (unsigned char)arr[y*width + x]);
			if (x != width-1) printf(",");
		}
		printf("\n");
	}
}

int arrayFindNull(void *arr, int len) {
	for (int i = 0; i < len; i++)
		if (((char **)arr)[i] == 0)
			return i;

	return -1;
}
