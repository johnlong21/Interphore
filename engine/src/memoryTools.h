#pragma once

void *zalloc(unsigned long size);
void dumpHex(const void* data, size_t size);

void engineAssert(bool expr, const char *filename, int lineNum);
void *engineMalloc(size_t size, const char *filename, int lineNum);
void engineFree(void *mem, const char *filename, int lineNum);
void printReadableSize(size_t size);
void parseStringByDelim(const char *bigString, const char *delim, char ***returnStrArray, int *returnStrArrayNum); // Each string allocated separately along with container
int countMemoryChunks();
