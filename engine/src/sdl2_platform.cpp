#include "platform.h"
#include "engine.h"

#ifdef SEMI_LINUX
#include <unistd.h>
#include <dirent.h>
#endif

#ifdef SEMI_WIN32
#include <direct.h>
#endif

SDL_Window* sdlWindow = NULL;
SDL_GLContext sdlContext = NULL;

char tempSavePath[PATH_LIMIT];

void *initPlatform(int mem) {
	printf("Plat initing\n");

	char path[PATH_LIMIT];

#if defined(SEMI_LINUX) || defined(SEMI_MAC)
	printf("argv: %s\n", platformArgV[0]);
	strcpy(path, platformArgV[0]);
	*strrchr(path, '/') = '\0';
	chdir(path);
	// strcat(path, "/");
#endif

#ifdef SEMI_WIN32
		HMODULE hModule = GetModuleHandleW(NULL);
		GetModuleFileName(NULL, path, PATH_LIMIT);
		*strrchr(path, '\\') = '\0';
		_chdir(path);
		// strcat(path, "\\");
#endif

	memset(tempSavePath, 0, PATH_LIMIT);
	// strcat(tempSavePath, path);
	strcat(tempSavePath, "tempSave");
	// exit(1);
	Assert(SDL_Init(SDL_INIT_VIDEO) >= 0);

#ifdef SEMI_GL_CORE
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	// SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
#else
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	// SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
	// SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	// SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	// SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

	sdlWindow = SDL_CreateWindow("Engine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, platWidth, platHeight, SDL_WINDOW_OPENGL);
	// sdlWindow = SDL_CreateWindow("Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, platWidth, platHeight, SDL_WINDOW_SHOWN);
	Assert(sdlWindow);

	sdlContext = SDL_GL_CreateContext(sdlWindow);
	Assert(sdlContext);

	Assert(!SDL_GL_SetSwapInterval(1));

	glewExperimental = GL_TRUE; 
	Assert(glewInit() == GLEW_OK);

	platPlatform = PLAT_SDL2;

	void *memory = Malloc(mem);
	memset(memory, 0, mem);

	printf("Maybe set\n");
#ifdef SEMI_HTML5
	printf("Setting inf loop\n");
	platformShouldClose = true;
	emscripten_set_main_loop(updateEngine, 0, 0);
	printf("Set\n");
#endif

	return memory;
}

void platformStartFrame() {
	platformMouseWheel = 0;

	SDL_Event e;
	while (SDL_PollEvent(&e) != 0) {
		if (e.type == SDL_QUIT) {
			platformShouldClose = true;
		} else if (e.type == SDL_KEYDOWN) {
			int key = e.key.keysym.sym;

			if (key >= 'a' && key <= 'z') key -= 'a'-'A';
			if (key == SDLK_LSHIFT) key = KEY_SHIFT;
			if (key == SDLK_RSHIFT) key = KEY_SHIFT;
			if (key == SDLK_BACKSPACE) key = KEY_BACKSPACE;
			if (key == SDLK_LCTRL) key = KEY_CTRL;
			if (key == SDLK_RCTRL) key = KEY_CTRL;
			if (key == SDLK_UP) key = KEY_UP;
			if (key == SDLK_DOWN) key = KEY_DOWN;
			if (key == SDLK_LEFT) key = KEY_LEFT;
			if (key == SDLK_RIGHT) key = KEY_RIGHT;

			if (key == 27) platformShouldClose = true;
			pressKey(key);
		} else if (e.type == SDL_KEYUP) {
			int key = e.key.keysym.sym;

			if (key >= 'a' && key <= 'z') key -= 'a'-'A';
			if (key == SDLK_LSHIFT) key = KEY_SHIFT;
			if (key == SDLK_RSHIFT) key = KEY_SHIFT;
			if (key == SDLK_BACKSPACE) key = KEY_BACKSPACE;
			if (key == SDLK_LCTRL) key = KEY_CTRL;
			if (key == SDLK_RCTRL) key = KEY_CTRL;
			if (key == SDLK_UP) key = KEY_UP;
			if (key == SDLK_DOWN) key = KEY_DOWN;
			if (key == SDLK_LEFT) key = KEY_LEFT;
			if (key == SDLK_RIGHT) key = KEY_RIGHT;

			releaseKey(key);
		}	else if (e.type == SDL_MOUSEMOTION) {
			SDL_GetMouseState(&platformMouseX, &platformMouseY);
		} else if (e.type == SDL_MOUSEBUTTONDOWN) {
			platformMouseLeftDown = true;
		} else if (e.type == SDL_MOUSEBUTTONUP) {
			platformMouseLeftDown = false;
		} else if (e.type == SDL_MOUSEWHEEL) {
			platformMouseWheel = e.wheel.y;
		}
	}
}

void platformEndFrame() {
	SDL_GL_SwapWindow(sdlWindow);
}

unsigned long platformGetTime() {
	return SDL_GetTicks();
}

void platformSleepMs(int millis) {
#ifdef _WIN32
	Sleep(millis);
#elif __linux__
	timespec ts;
	ts.tv_sec = millis / 1000;
	ts.tv_nsec = (millis % 1000) * 1000000;
	nanosleep(&ts, NULL);
#endif
}

void platformSaveToTemp(const char *str) {
	FILE *f = fopen(tempSavePath, "w");
	Assert(f);

	fprintf(f, "%s", str);
	fclose(f);
}

char *platformLoadFromTemp() {
	FILE *f = fopen(tempSavePath, "r");
	if (f == NULL) {
		printf("Save file doesn't exist\n");

		char *str = (char *)"none";
		return stringClone(str);
	}

	fseek(f, 0, SEEK_END);
	long fileLen = ftell(f);
	rewind(f);

	char *str = (char *)Malloc((fileLen + 1) * sizeof(char));
	fread(str, fileLen, 1, f);
	fclose(f);

	str[fileLen] = '\0';
	return str;
}

void platformSaveToDisk(const char *str) {
	// platformSaveToTemp(str);
}

void platformLoadFromDisk(void (*loadCallback)(char *, int)) {
	FILE *f = fopen(windowsDiskLoadPath, "rb");
	if (f == NULL) {
		printf("Disk file doesn't exist\n");

		char *str = stringClone("none");
		loadCallback(str, 0);
		return;
	}

	fseek(f, 0, SEEK_END);
	long fileLen = ftell(f);
	rewind(f);

	char *str = (char *)Malloc((fileLen + 1) * sizeof(char));
	fread(str, fileLen, 1, f);
	fclose(f);

	str[fileLen] = '\0';
	loadCallback(str, fileLen);
}

void platformLoadFromUrl(const char *url, void (*loadCallback)(char *, int)) {
	loadFromUrl(url, loadCallback);
}

void platformClearTemp() {
	remove(tempSavePath);
}

void displayKeyboard(bool show) {
}

void gotoUrl(const char *url) {
// #ifdef SEMI_WIN32
// 	winGotoUrl(url);
// #endif
}
