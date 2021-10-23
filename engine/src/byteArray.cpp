#include "byteArray.h"

#define BYTE_ARRAY_LIMIT 1024*1024*10
unsigned char byteArrayBuffer[BYTE_ARRAY_LIMIT];

void byteArrayCompress(unsigned char *inBytes, size_t inBytesLen, unsigned char **outBytes, size_t *outBytesLen) {
	unsigned long newLen = compressBound((BYTE_ARRAY_LIMIT) * 1.001 + 12); // Whatever I guess, zlib told me to
	int compressStatus = compress(byteArrayBuffer, &newLen, inBytes, inBytesLen);

	if (compressStatus != Z_OK) {
		printf("Error compressing %ld bytes (%d)\n", inBytesLen, compressStatus);
		*outBytesLen = 0;
		return;
	}

	unsigned char *res = (unsigned char *)Malloc(newLen);
	memcpy(res, byteArrayBuffer, newLen);

	*outBytes = res;
	*outBytesLen = newLen;
}

void byteArrayUncompress(unsigned char *inBytes, size_t inBytesLen, unsigned char **outBytes, size_t *outBytesLen) {
	unsigned long newLen = compressBound((BYTE_ARRAY_LIMIT) * 1.001 + 12); // Whatever I guess, zlib told me to
	int compressStatus = uncompress(byteArrayBuffer, &newLen, inBytes, inBytesLen);

	if (compressStatus != Z_OK) {
		printf("Error uncompressing %ld bytes (%d)\n", inBytesLen, compressStatus);
		*outBytesLen = 0;
		return;
	}

	unsigned char *res = (unsigned char *)Malloc(newLen);
	memcpy(res, byteArrayBuffer, newLen);

	*outBytes = res;
	*outBytesLen = newLen;
}
