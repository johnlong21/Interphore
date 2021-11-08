#include "defines.hpp"

void androidPrintf(const char *fmt, ...) {
    char buffer[10240];

    va_list argptr;
    va_start(argptr, fmt);
    int bufferLen = vsprintf(buffer, fmt, argptr);
    va_end(argptr);

    if (buffer[bufferLen-1] == '\n') {
        buffer[bufferLen-1] = '\0';
        bufferLen--;
    }

    LOGI("%s\n", buffer);
}
