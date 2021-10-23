#include "memoryTools.h"
#define MEMORY_CHUNK_LIMIT 8192

#define ConsumeBytes(dest, src, count) memcpy(dest, src, count); src += count;

struct MemoryChunk {
	bool exists;
	size_t size;
	void *mem;
};

MemoryChunk memChunks[MEMORY_CHUNK_LIMIT];
size_t memUsed = 0;

void *zalloc(unsigned long size) {
	void *mem = Malloc(size);
	if (mem) memset(mem, 0, size);
	return mem;
}

void dumpHex(const void* data, size_t size) {
	static char line[Megabytes(2)] = {};
	memset(line, 0, Megabytes(2));

	char ascii[17];
	size_t i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i) {
		sprintf(line+strlen(line), "%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			sprintf(line+strlen(line), " ");
			if ((i+1) % 16 == 0) {
				sprintf(line+strlen(line), "|  %s \n", ascii);
				printf("%s", line);
				memset(line, 0, Megabytes(2));
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					sprintf(line+strlen(line), " ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					sprintf(line+strlen(line), "   ");
				}
				sprintf(line+strlen(line), "|  %s \n", ascii);
				printf("%s", line);
				memset(line, 0, Megabytes(2));
			}
		}
	}
}

void *engineMalloc(size_t size, const char *filename, int lineNum) {
	// printf("Size malloced: %ld\n", size);
	void *mem = malloc(size);

	MemoryChunk *chunk = NULL;
	for (int i = 0; i < MEMORY_CHUNK_LIMIT; i++) {
		if (!memChunks[i].exists) {
			chunk = &memChunks[i];
			break;
		}
	}

	chunk->exists = true;
	chunk->mem = mem;
	chunk->size = size;
	memUsed += size;
	// printf("%.f kb (%d bytes) alloced by %s:%d (chunks: %d)\n", (float)size / Kilobytes(1), size, filename, lineNum, countMemoryChunks());
	Assert(mem);
	return mem;
}

void engineFree(void *mem, const char *filename, int lineNum) {
	MemoryChunk *chunk = NULL;
	for (int i = 0; i < MEMORY_CHUNK_LIMIT; i++) {
		if (memChunks[i].mem == mem) {
			chunk = &memChunks[i];
			break;
		}
	}

	if (chunk) {
		if (!chunk->exists) {
			printf("Double free by %s:%d\n", filename, lineNum);
			return;
		}

		memUsed -= chunk->size;
		chunk->exists = false;
		// printf("%d bytes freed by %s:%d\n", chunk->size, filename, lineNum);
	}

	Assert(mem);
	free(mem);
	// printf("Mem chunks (f): %d\n", countMemoryChunks());
}

void printReadableSize(size_t size) {
	if (size < Kilobytes(1)) {
		printf("%db", size);
	} else if (size < Megabytes(1)) {
		printf("%0.2fkb", (float)size/Kilobytes(1));
	} else if (size < Gigabytes(1)) {
		printf("%0.2fmb", (float)size/Megabytes(1));
	} else {
		printf(">1gb");
	}
}

void parseStringByDelim(const char *bigString, const char *delim, char ***returnStrArray, int *returnStrArrayNum) {
	const char *lineStart = bigString;
	int delimLen = strlen(delim);

	int delimCount = 0;
	for (;;) {
		const char *lineEnd = strstr(lineStart, delim);
		if (!lineEnd) break;
		delimCount++;
		lineStart = lineEnd + delimLen;
	}

	char **strArray = (char **)malloc(sizeof(char *) * (delimCount + 1));
	int strArrayNum = 0;

	lineStart = bigString;
	for (;;) {
		const char *lineEnd = strstr(lineStart, delim);
		bool lastLine = false;

		if (!lineEnd) {
			lastLine = true;
			lineEnd = &bigString[strlen(bigString)-1];
		}

		int lineLen = lineEnd-lineStart;
		char line[MED_STR] = {};
		if (lineLen <= 0) break;

		if (!lastLine) {
			strncpy(line, lineStart, lineLen-1);
		} else {
			strncpy(line, lineStart, lineLen+1);
		}

		char *str = stringClone(line);
		strArray[strArrayNum++] = str;

		if (lastLine) break;
		lineStart = lineEnd + delimLen;
	}

	*returnStrArray = strArray;
	*returnStrArrayNum = strArrayNum;
}


int countMemoryChunks() {
	int count = 0;

	for (int i = 0; i < MEMORY_CHUNK_LIMIT; i++)
		if (memChunks[i].exists)
			count++;

	return count;
}
