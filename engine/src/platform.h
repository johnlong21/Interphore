#pragma once

struct NanoTime {
	unsigned int seconds;
	unsigned int nanos;

#ifdef _WIN32
	LARGE_INTEGER winFreq;
	LARGE_INTEGER time;
#endif
};

enum Platform { PLAT_NULL, PLAT_SDL2, PLAT_FLASH, PLAT_ANDROID, PLAT_GLFW };

enum PlatformKey { KEY_LEFT=301, KEY_RIGHT=302, KEY_UP=303, KEY_DOWN=304, KEY_SHIFT=305, KEY_BACKSPACE=306, KEY_CTRL=307 };

void initPlatform();
#ifdef SEMI_DESKTOP
void deinitPlatform();
#endif
void platformStartFrame();
void platformEndFrame();
unsigned long platformGetTime();
void platformSleepMs(int millis);
void platformSaveToTemp(const char *str);
char *platformLoadFromTemp();
void platformSaveToDisk(const char *str);
void platformLoadFromDisk(void (*loadCallback)(char *, int));
void platformLoadFromUrl(const char *url, void (*loadCallback)(char *, int));
void platformClearTemp();
void gotoUrl(const char *url);
void getDirList(const char *dirn, char **pathNames, int *pathNamesNum);
long readFile(const char *filename, void **storage);
void writeFile(const char *filename, const char *str);
bool fileExists(const char *filename);

extern bool platformShouldClose;
extern int platformMouseX;
extern int platformMouseY;
extern int platformMouseWheel;
extern int platformMouseLeftDown;
extern int platWidth;
extern int platHeight;
extern int platformArgC;
extern char **platformArgV;
extern Platform platPlatform;

extern char *windowsDiskLoadPath;

extern int platformLoadedStringSize;

void getNanoTime(NanoTime *time);
