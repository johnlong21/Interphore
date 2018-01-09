#define VERSION "0.0.1"

/// Game
#define ACTION_QUEUE_LIMIT 128
#define OVERLAP_LIMIT 64
#define CHOICE_LABEL_LIMIT 128
#define MAP_OBJECT_LIMIT 512
#define DOOR_FROM_LIMIT 32

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

#include "game.cpp"
#include "hotspot.cpp"
#include "level.cpp"

extern "C" void entryPoint();
void mainUpdate();
void mainInit();

bool passedSitelock = false;
bool isTeaser = false;
bool pressed[4] = {};

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
// #ifdef SITELOCK
// 	passedSitelock = false;
// #elif defined(SEMI_EARLY)
// 	isTeaser = true;

// 	MintSprite *bg = createMintSprite();
// 	bg->setupRect(1, 1, 0);
// 	bg->scale(engine->width, engine->height);
// 	bg->scrolls = false;
// 	bg->layer = 0;

// 	strcpy(engine->spriteData.defaultFont, "Espresso-Dolce_50");
// 	MintSprite *text = createMintSprite();
// 	text->setupEmpty(200, 200);
// 	text->setText("Soon.");
// 	text->x = engine->width/2 - text->textWidth/2;
// 	text->y = engine->height/2 - text->textHeight/2;
// #else
//	passedSitelock = true;
//	initGame();
// #endif
	initGame();
}

void mainUpdate() {
		updateGame();
	// if (isTeaser) return;
	// if (passedSitelock) {
	// 	updateGame();
	// } else {
	// 	if (keyIsJustPressed('P')) pressed[0] = true;
	// 	if (keyIsJustPressed('L')) pressed[1] = true;
	// 	if (keyIsJustPressed('A')) pressed[2] = true;
	// 	if (keyIsJustPressed('Y')) pressed[3] = true;
	// 	if (pressed[0] && pressed[1] && pressed[2] && pressed[3]) {
	// 		initGame();
	// 		passedSitelock = true;
	// 	}
	// }
}
