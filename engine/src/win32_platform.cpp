#include "platform.h"

#ifndef SEMI_SDL

#define GLEW_STATIC
#include <GL/glew.h>
#include <shellapi.h>

#include <direct.h>

LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

HWND windowHandle = {};
WNDCLASS windowClass = {};
HMODULE hInstance = {};

void *initPlatform(int mem) {
	printf("Plat initing\n");
	hInstance = GetModuleHandle(NULL);

	char path[PATH_LIMIT];

	GetModuleFileName(NULL, path, PATH_LIMIT);
	*strrchr(path, '\\') = '\0';
	_chdir(path);

	// glewExperimental = GL_TRUE; 

	windowClass = {};
	// windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW; //@clenaup Do these matter
	windowClass.hCursor = LoadCursor(0, IDC_ARROW);
	windowClass.hIcon = LoadIcon(0, IDI_WINLOGO);
	windowClass.style = 0;
	windowClass.lpfnWndProc = windowProc;
	windowClass.hInstance = hInstance;
	windowClass.lpszClassName = "WindowClass";

	ATOM regClassAtom = RegisterClass(&windowClass);

	windowHandle = CreateWindowEx(
		0,
		windowClass.lpszClassName,
		"Window Name",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		0,
		0,
		hInstance,
		0
	);

	// DWORD error = GetLastError();
	Assert(windowHandle);

	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER,
		PFD_TYPE_RGBA,
		32,
		0,0,0,0,0,0,0,0,0,0,0,0,0, // useles parameters
		16,
		0,0,0,0,0,0,0
	};

	HDC deviceContext = GetDC(windowHandle);
	HWND checkHandle = WindowFromDC(deviceContext);
	int indexPixelFormat = ChoosePixelFormat(deviceContext, &pfd);
	SetPixelFormat(deviceContext, indexPixelFormat, &pfd);

	HGLRC hglrc = 0;
	hglrc = wglCreateContext(deviceContext);

	void *memory = Malloc(mem);
	memset(memory, 0, mem);

	return memory;
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	// DWORD error = GetLastError();
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
	LRESULT result = 0;

	if (uMsg = WM_CREATE) {
	} else if (uMsg == WM_SIZE) {
		printf("got WM_SIZE\n");
	} else if (uMsg == WM_ACTIVATEAPP) {
		printf("got WM_ACTIVATEAPP\n");
	} else if (uMsg == WM_DESTROY) {
		printf("got WM_DESTROY\n");
	} else if (uMsg == WM_CLOSE) {
		printf("got WM_CLOSE\n");
	} else {
		result = DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	return result;
}

void platformStartFrame() {
	MSG message;
	for (;;) {
		BOOL messageResult = GetMessage(&message, NULL, 0, 0);
		if (messageResult < 0) break;

		TranslateMessage(&message);
		DispatchMessage(&message);
	}
}

void platformEndFrame() {
}

int platformGetTime() {
	return 0;
}

void platformSleepMs(int millis) {
}

void platformSaveToTemp(const char *str) {
}

char *platformLoadFromTemp() {
	return NULL;
}

void platformSaveToDisk(const char *str) {
}

void platformLoadFromDisk(void (*loadCallback)(char *)) {
}

void platformLoadFromUrl(const char *url, void (*loadCallback)(char *)) {
}

void platformClearTemp() {
}

void gotoUrl(const char *url) {
	ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
}
#endif
