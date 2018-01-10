/// Game
#define CHOICE_BUTTON_MAX 4
#define BUTTON_MAX 32

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
struct Button;

extern "C" void entryPoint();
void mainUpdate();
void mainInit();
void changeState(GameState newState);
void loadMod(const char *serialData);
Button *createButton(const char *text);
void destroyButton(Button *btn);

void js_print(CScriptVar *v, void *userdata);
void js_append(CScriptVar *v, void *userdata);
void js_addChoice(CScriptVar *v, void *userdata);

struct Button {
	bool exists;
	MintSprite *sprite;
	MintSprite *tf;
};

struct GameStruct {
	GameState state;

	Button buttons[BUTTON_MAX];

	MintSprite *bg;
	MintSprite *title;
	MintSprite *subtitle;

	Button *loadButton;

	MintSprite *mainText;
	Button *choices[CHOICE_BUTTON_MAX];
	int choicesNum;
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

	jsInterp = new CTinyJS();
	registerFunctions(jsInterp);
	jsInterp->addNative("function print(text)", &js_print, 0);
	jsInterp->addNative("function append(text)", &js_append, 0);
	jsInterp->addNative("function addChoice(text, dest)", &js_addChoice, 0);
	jsInterp->execute("print(\"JS engine init\");");
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
			game->bg->addChild(spr);
			spr->setText("Writer");
			spr->gravitate(0.5, 0);

			game->title = spr;
		}

		{ /// Subtitle
			MintSprite *spr = createMintSprite();
			game->title->addChild(spr);
			spr->setupEmpty(engine->width, 100);

			spr->setText("A story tool");
			spr->gravitate(0.5, 0.5);
			spr->y = spr->parent->getHeight() + 10;

			game->subtitle = spr;
		}

		{ /// Load button
			Button *btn = createButton("Load");
			game->bg->addChild(btn->sprite);
			btn->sprite->gravitate(0.5, 0.90);

			game->loadButton = btn;
		}
	}

	if (newState == STATE_MOD) {
		{ /// Main text
			MintSprite *spr = createMintSprite();
			spr->setupEmpty(engine->width, engine->height*0.75);
			spr->setText("Main Text");
			game->bg->addChild(spr);

			game->mainText = spr;
		}
	}

	if (game->state == STATE_MENU) {
		game->title->destroy();
		destroyButton(game->loadButton);
	}

	game->state = newState;
}

void mainUpdate() {
	if (game->state == STATE_MENU) {
		if (game->loadButton->sprite->justPressed) {
#if 0
			platformLoadFromDisk(loadMod);
#else
			const char *jsTest = ""
				"print(\"This is a test mod\");"
				"append(\"I'm appending some text\");"
				"addChoice(\"click me\", \"Thing clicked\");"
				"addChoice(\"No! Me!\", \"Thing clicked\");"
				"addChoice(\"Don't click anything\", \"Thing clicked\");"
				"addChoice(\"Don't click anything2\", \"Thing clicked\");"
				;
			loadMod(jsTest);
#endif
		}
	}
}

void loadMod(const char *serialData) {
	changeState(STATE_MOD);
	printf("Loaded data: %s\n", serialData);

	jsInterp->execute(serialData);
}

void destroyButton(Button *btn) {
	btn->exists = false;
	btn->sprite->destroy();
}

Button *createButton(const char *text) {
	int slot;
	for (slot = 0; slot < BUTTON_MAX; slot++)
		if (!game->buttons[slot].exists)
			break;
	assert(slot < BUTTON_MAX);

	Button *btn = &game->buttons[slot];
	btn->exists = true;

	{ /// Button sprite
		MintSprite *spr = createMintSprite();
		spr->setupRect(128, 64, 0x444444);

		btn->sprite = spr;
	}

	{ /// Button text
		MintSprite *spr = createMintSprite();
		spr->setupEmpty(btn->sprite->getFrameWidth(), btn->sprite->getFrameHeight());
		spr->setText(text);
		btn->sprite->addChild(spr);
		spr->gravitate(0.5, 0.5);

		btn->tf = spr;
	}

	return btn;
}

void js_print(CScriptVar *v, void *userdata) {
	const char *arg1 = v->getParameter("text")->getString().c_str();
	printf("> %s\n", arg1);
}

void js_append(CScriptVar *v, void *userdata) {
	const char *arg1 = v->getParameter("text")->getString().c_str();
	strcat(game->mainText->rawText, arg1);
	game->mainText->setText(game->mainText->rawText);
}

void js_addChoice(CScriptVar *v, void *userdata) {
	const char *arg1 = v->getParameter("text")->getString().c_str();
	const char *arg2 = v->getParameter("dest")->getString().c_str();

	assert(game->choicesNum+1 <= CHOICE_BUTTON_MAX);
	Button *btn = createButton(arg1);
	btn->sprite->x = btn->sprite->width * game->choicesNum;
	game->choices[game->choicesNum++] = btn;
}
