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

void *initPlatform(int mem);
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
void displayKeyboard(bool show);
void writeFile(const char *filename, const char *str);
bool fileExists(const char *filename);

bool platformShouldClose = false;
int platformMouseX;
int platformMouseY;
int platformMouseWheel;
int platformMouseLeftDown;
int platWidth = 1280;
int platHeight = 720;
int platformArgC;
char **platformArgV;
Platform platPlatform = PLAT_NULL;

char *windowsDiskLoadPath = NULL;

#ifdef SEMI_ANDROID
AAssetManager *assetManager = NULL;
#endif

int platformLoadedStringSize;
