#include "platform.h"
#include "engine.h"

extern "C" void flashUpdate();
extern "C" void flashSaveCallback(int errorCode);
extern "C" void flashLoadCallback(int errorCode);
extern "C" void flashKeyboardInput(int key, int pressBool);
extern "C" float *flashGetSoundBuffer();
extern "C" int getMemoryNeeded();
extern "C" void flashStreamTextureCallback();

void flashStreamTextures(const char **assetIds, int assetIdsNum);

void (*flashLoadCallbackToGame)(char *, int);
char flashTempLoadString[SERIAL_SIZE];

void *initPlatform(int mem) {
	AS3_DeclareVar(traceString, String); //@cleanup Do I need this?

	printf("Flash plat initing\n");
	platformShouldClose = true;
	platPlatform = PLAT_FLASH;

	inline_as3("%0 = Console.stageRef.stageWidth;" : "=r"(platWidth) :);
	inline_as3("%0 = Console.stageRef.stageHeight;" : "=r"(platHeight) :);

	void *memory = Malloc(mem);
	memset(memory, 0, mem);
	return memory;
}

void platformStartFrame() {
	inline_as3("%0 = Console.xMouse;" : "=r"(platformMouseX) :);
	inline_as3("%0 = Console.yMouse;" : "=r"(platformMouseY) :);
	inline_as3("%0 = Console.leftMouse;" : "=r"(platformMouseLeftDown) :);
	inline_as3("%0 = Console.wheelMouse;" : "=r"(platformMouseWheel) :);

	if (platformMouseWheel < 0) platformMouseWheel = -1;
	if (platformMouseWheel > 0) platformMouseWheel = 1;
	inline_as3("Console.wheelMouse = 0;" ::);
}

void platformEndFrame() {
}

unsigned long platformGetTime() {
	unsigned long val;
	inline_as3("%0 = Console.getTime();" : "=r"(val) :);
	return val;
}

void platformSleepMs(int millis) {
}

void updateEngine(); //@hack to get the def
extern "C" void flashUpdate() {
	updateEngine();
}

extern "C" void flashKeyboardInput(int key, int pressBool) {
	if (key == 37) key = KEY_LEFT;
	if (key == 38) key = KEY_UP;
	if (key == 39) key = KEY_RIGHT;
	if (key == 40) key = KEY_DOWN;
	if (key == 16) key = KEY_SHIFT;
	if (key == 8) key = KEY_BACKSPACE;
	if (key == 17) key = KEY_CTRL;
	if (key == 187) key = '=';
	if (key == 189) key = '-';
	if (key == 192) key = '`';

	if (pressBool == 1) pressKey(key);
	if (pressBool == 0) releaseKey(key);
}

extern "C" void flashSaveCallback(int errorCode) {
	if (errorCode != 0) {
		printf("A error occurred while saving. %d\n", errorCode);
	}
}

extern "C" void flashLoadCallback(int errorCode) {
	if (errorCode != 0) {
		printf("A error occurred while loading. %d\n", errorCode);
		flashLoadCallbackToGame(NULL, 0);
		return;
	}

	inline_as3("%0 = Console.loadedStringSize;" : "=r"(platformLoadedStringSize) :);

	char *buffer = (char *)malloc(platformLoadedStringSize+1);
	memcpy(buffer, flashTempLoadString, platformLoadedStringSize);
	buffer[platformLoadedStringSize] = '\0';
	flashLoadCallbackToGame(buffer, platformLoadedStringSize);
}

extern "C" float *flashGetSoundBuffer() {
	// soundMix();
	// return sampleBuffer;
	return NULL;
}

extern "C" int getMemoryNeeded() {
	return MEMORY_LIMIT;
}

void platformSaveToTemp(const char *str) {
	memset(flashTempLoadString, 0, SERIAL_SIZE);
	inline_as3("Console.saveToTemp(%0, %1);" :: "r"(str), "r"(strlen(str)));
}

char *platformLoadFromTemp() {
	char *str = (char *)Malloc(SERIAL_SIZE * sizeof(char));
	str[0] = '\0';

	inline_as3("Console.loadFromTemp(%0);" :: "r"(str));

	return str;
}

void platformSaveToDisk(const char *str) {
	inline_as3("Console.saveToDisk(%0, %1);" :: "r"(str), "r"(strlen(str)));
}

void platformLoadFromDisk(void (*loadCallback)(char *, int)) {
	flashLoadCallbackToGame = loadCallback;
	memset(flashTempLoadString, 0, SERIAL_SIZE);
	inline_as3("Console.loadFromDisk(%0);" :: "r"(flashTempLoadString));
}

void platformLoadFromUrl(const char *url, void (*loadCallback)(char *, int)) {
	flashLoadCallbackToGame = loadCallback;
	memset(flashTempLoadString, 0, SERIAL_SIZE);
	inline_as3("Console.loadFromUrl(%0, %1, %2);" :: "r"(url), "r"(strlen(url)), "r"(flashTempLoadString));
}

void platformClearTemp() {
	inline_as3(
		"import flash.net.SharedObject;"
		"var so = SharedObject.getLocal(\"paraphore2\");"
		"so.clear();"
		"so.flush();"
		"so.close();"
		::
	);
}

void displayKeyboard(bool show) {
}

void gotoUrl(const char *url) {
	inline_as3(
		"var urlPtr = %0;"
		"var urlSize = %1;"
		"import flash.net.URLRequest;"
		"flash.net.navigateToURL(new URLRequest(CModule.readString(urlPtr, urlSize)), \"_blank\");"
		:: "r"(url), "r"(strlen(url))
	);
}

extern "C" void flashStreamTextureCallback() {
	engine->assets.memoryStreamingAssetsNum--;

	int width, height;
	inline_as3("%0 = Console.tempStreamedBitmap.width;" : "=r"(width) :);
	inline_as3("%0 = Console.tempStreamedBitmap.height;" : "=r"(height) :);

	char name[PATH_LIMIT] = {};
	inline_as3("CModule.writeString(%0, Console.tempStreamedName);" :: "r"(name));

	void *data = Malloc(width*height*4);

	inline_as3("CModule.writeBytes(%0, Console.tempStreamedBytes.bytesAvailable, Console.tempStreamedBytes);" ::"r"(data));
	Asset *asset = getTextureFromData(name, data, width, height, true);

	asset->level = assetData->nextStreamCacheLevel;

}

char *flashGetUrl() {
	char *str = (char *)Malloc(HUGE_STR * sizeof(char));
	str[0] = '\0';

	inline_as3("Console.getContainerUrl(%0);" :: "r"(str));

	return str;
}
