#include "defines.hpp"

#define ZIP_HEADERS_MAX 2048

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint;

struct LocalFileHeader {
    uint signature;
    uint16 extractVersion;
    uint16 bitFlags;
    uint16 compressionMethod;
    uint16 loadModFileTime;
    uint16 loadModFileDate;
    uint crc;
    uint compressedSize;
    uint uncompressedSize;
    uint16 fileNameLength;
    uint16 extraFieldLength;

    char *fileName;
    char *extraField;
    unsigned char *compressedData;
    unsigned char *uncompressedData;
};

struct Zip {
    LocalFileHeader headers[ZIP_HEADERS_MAX];
    int headersNum;
};

#define INFLATE_BUFFER_SIZE Megabytes(5)
extern unsigned char inBuffer[INFLATE_BUFFER_SIZE];
extern unsigned char outBuffer[INFLATE_BUFFER_SIZE];

void openZip(unsigned char *data, int size, Zip *zip);
void closeZip(Zip *zip);
