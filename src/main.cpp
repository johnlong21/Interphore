#ifndef STATIC_ASSETS
#include "../bin/assetLib.cpp"
#endif

#define VERSION "0.0.1" //@cleanup I shouldn't have to do this
#include "gameEngine.h"

#include "writer/writer.cpp"

void updateMain();
void initMain();
void exitMain();
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
	engine->exitCallback = exitMain;

#if 1
	MintSprite *bg = createMintSprite();
	bg->setupRect(1, 1, 0x000000);
	bg->scale(engine->width, engine->height);

	MintSprite *spr = createMintSprite();
	spr->setupContainer(engine->width, engine->height);
#else
	MintSprite *spr = createMintSprite();
	spr->setupRect(engine->width, engine->height, 0x000000);
#endif
	strcpy(engine->spriteData.defaultFont, "OpenSans-Regular_20"); //@cleanup I shouldn't have to do this in every new project
	Writer::initWriter(spr);
}

void updateMain() {
	Writer::updateWriter();
}

void exitMain() {
	Writer::deinitWriter();
}
