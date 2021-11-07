#pragma once

#define ArrayLength(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

void arraySwapPop(void **arr, int *len, void *element);
void arraySwapPopIndex(void **arr, int *len, int indexToRemove);
void arrayPrint2d(int *arr, int width, int height);
void arrayPrint2d(unsigned char *arr, int width, int height);
bool arrayContainsPointer(void **arr, int len, void *element);
int arrayFindNull(void *arr, int len);
