/// Game
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

#define VERSION "0.0.1" //@cleanup I shouldn't have to do this
#include "gameEngine.h"

#include "writer/writer.cpp"

void updateMain();
void initMain();
extern "C" void entryPoint();

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
	MintSprite *spr = createMintSprite();
	spr->setupRect(engine->width, engine->height, 0x000000);
	strcpy(engine->spriteData.defaultFont, "OpenSans-Regular_20"); //@cleanup I shouldn't have to do this in every new project
	Writer::initWriter(spr);
}

void updateMain() {
	Writer::updateWriter();
}
