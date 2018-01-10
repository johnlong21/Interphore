#define VERSION "0.0.1"

/// Game
// nil

/// Debugging
// #define LOG_ASSET_ADD
// #define LOG_ENGINE_UPDATE
// #define LOG_ASSET_GET
// #define LOG_ASSET_EVICT
// #define LOG_SOUND_GEN
// #define LOG_TEXTURE_GEN
// #define LOG_TEXTURE_STREAM

#ifndef STATIC_ASSETS
#include "../bin/assetLib.cpp"
#endif

#include "gameEngine.h"

extern "C" void entryPoint();
void mainUpdate();
void mainInit();
void js_print(CScriptVar *v, void *userdata);

CTinyJS *jsInterp;

#ifdef SEMI_WIN32
INT WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) {
#else
int main(int argc, char **argv) {
	platformArgC = argc;
	platformArgV = argv;
#endif
	entryPoint();
	return 0;
}

void entryPoint() {
	initEngine(mainInit, mainUpdate);
}

void mainInit() {
	printf("Init\n");

	CTinyJS *jsInterp = new CTinyJS();
	registerFunctions(jsInterp);
	jsInterp->addNative("function print(text)", &js_print, 0);
	jsInterp->execute("print(\"This is a print test in js\");");
}

void mainUpdate() {
}

void js_print(CScriptVar *v, void *userdata) {
	const char *arg1 = v->getParameter("text")->getString().c_str();
	printf("> %s\n", arg1);
}
