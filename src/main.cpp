/// Game
#define CHOICE_BUTTON_MAX 4

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

#define VERSION "0.0.1"
#include "gameEngine.h"

enum GameState { STATE_NULL=0, STATE_MENU, STATE_MOD };

extern "C" void entryPoint();
void mainUpdate();
void mainInit();
void js_print(CScriptVar *v, void *userdata);
void changeState(GameState newState);
void loadMod(char *serialData);

struct GameStruct {
	GameState state;

	MintSprite *bg;
	MintSprite *title;
	MintSprite *subtitle;

	MintSprite *loadButton;
	MintSprite *loadButtonTf;

	MintSprite *mainText;
	MintSprite *choiceButtons[CHOICE_BUTTON_MAX];
};

GameStruct *game;
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
	strcpy(engine->spriteData.defaultFont, "OpenSans-Regular_20");

	game = (GameStruct *)zalloc(sizeof(GameStruct));

	changeState(STATE_MENU);
}

void changeState(GameState newState) {
	if (newState == STATE_MENU) {
		{ /// Bg
			MintSprite *spr = createMintSprite();
			spr->setupRect(engine->width, engine->height, 0x000000);

			game->bg = spr;
		}

		{ /// Title
			MintSprite *spr = createMintSprite();
			spr->setupEmpty(engine->width, 100);
			spr->setText("Writer");
			spr->x = engine->width/2 - spr->getWidth()/2;

			game->title = spr;
		}

		{ /// Subtitle
			MintSprite *spr = createMintSprite();
			spr->setupEmpty(engine->width, 100);
			spr->setText("A story tool");

			game->title->addChild(spr);
			spr->gravitate(0.5, 0.5);
			spr->y = spr->parent->getHeight() + 10;

			game->subtitle = spr;
		}

		{ /// Load button
			MintSprite *spr = createMintSprite();
			spr->setupRect(128, 64, 0x444444);
			spr->x = engine->width/2 - spr->width/2;
			spr->y = engine->height - spr->getHeight() - 10;

			game->loadButton = spr;
		}

		{ /// Load button text
			MintSprite *spr = createMintSprite();
			spr->setupEmpty(game->loadButton->getFrameWidth(), game->loadButton->getFrameHeight());
			spr->setText("Load");
			game->loadButton->addChild(spr);
			spr->gravitate(0.5, 0.5);

			game->loadButtonTf = spr;
		}
	}

	game->state = newState;
}

void mainUpdate() {
	if (game->state == STATE_MENU) {
		if (game->loadButton->justPressed) platformLoadFromDisk(loadMod);
	}
}

void loadMod(char *serialData) {
	printf("Loaded data: %s\n", serialData);
	// jsInterp->execute(serialData);
}

void js_print(CScriptVar *v, void *userdata) {
	const char *arg1 = v->getParameter("text")->getString().c_str();
	printf("> %s\n", arg1);
}
