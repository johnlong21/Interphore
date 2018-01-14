/// Game
#define CHOICE_BUTTON_MAX 4
#define BUTTON_MAX 32
#define PASSAGE_NAME_MAX MED_STR
#define CHOICE_TEXT_MAX MED_STR
#define MOD_NAME_MAX MED_STR
#define PASSAGE_MAX 1024
#define MOD_ENTRIES_MAX 16
#define MSG_MAX 64

/// Debugging
// #define LOG_ASSET_ADD
// #define LOG_ENGINE_UPDATE
// #define LOG_ASSET_GET
// #define LOG_ASSET_EVICT
// #define LOG_SOUND_GEN
// #define LOG_TEXTURE_GEN
// #define LOG_TEXTURE_STREAM

const char *jsTest = ""
"START_PASSAGES\n"
":Start\n"
"START_JS\n"
"var apples = 1;\n"
"END_JS\n"
"This passage shows off basic variables.\n"
"[Let's get to it|Main]\n"
"---\n"
":Main\n"
"You have `apples` apples.\n"
"[Gain an apple]\n"
"[Lose an apple]\n"
"---\n"
":Gain an apple\n"
"START_JS\n"
"apples += 1;\n"
"END_JS\n"
"You gained an apple.\n"
"[Go back|Main]\n"
"---\n"
":Lose an apple\n"
"START_JS\n"
"apples -= 1;\n"
"END_JS\n"
"You lost an apple.\n"
"[Go back|Main]\n"
"END_PASSAGES\n"
"\n"
"gotoPassage(\"Start\");\n"
;

#ifndef STATIC_ASSETS
#include "../bin/assetLib.cpp"
#endif

#define VERSION "0.0.1" //@cleanup I shouldn't have to do this
#include "gameEngine.h"

#include "writer/writer.cpp"

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
	initEngine(initMain, updateMain);
}

void initMain() {
	initWriter();
}

void updateMain() {
	updateWriter();
}
