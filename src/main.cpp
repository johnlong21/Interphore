/// Game
#define CHOICE_BUTTON_MAX 4
#define BUTTON_MAX 32
#define PASSAGE_NAME_MAX MED_STR
#define CHOICE_TEXT_MAX MED_STR
#define MOD_NAME_MAX MED_STR
#define PASSAGE_MAX 1024
#define MOD_ENTRIES_MAX 16

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
void loadMod(char *serialData);
Button *createButton(const char *text, int width=256, int height=128);
void destroyButton(Button *btn);
void gotoPassage(const char *passageName);
void append(const char *text);
void addChoice(const char *text, const char *dest);
void clear();

void js_print(CScriptVar *v, void *userdata);
void js_append(CScriptVar *v, void *userdata);
void js_addChoice(CScriptVar *v, void *userdata);
void js_submitPassage(CScriptVar *v, void *userdata);
void js_gotoPassage(CScriptVar *v, void *userdata);
void dumpHex(const void* data, size_t size);

struct Passage {
	char name[PASSAGE_NAME_MAX];
	char appendData[HUGE_STR];
	char choices[CHOICE_BUTTON_MAX][CHOICE_TEXT_MAX];
	int choicesNum;
};

struct ModEntry {
	char name[MOD_NAME_MAX];
	char url[URL_LIMIT];
};

struct Button {
	bool exists;
	MintSprite *sprite;
	MintSprite *tf;
	char destPassageName[PASSAGE_NAME_MAX];
};

struct GameStruct {
	GameState state;

	Button buttons[BUTTON_MAX];

	MintSprite *bg;
	MintSprite *title;
	MintSprite *subtitle;
	MintSprite *browserBg;

	Button *loadButton;
	Button *loadUrlButton;

	MintSprite *mainText;
	Button *choices[CHOICE_BUTTON_MAX];
	int choicesNum;

	Passage *passages[PASSAGE_MAX];
	int passagesNum;

	ModEntry urlMods[MOD_ENTRIES_MAX];
	int urlModsNum;
};

GameStruct *game;
CTinyJS *jsInterp;

const char modRepo[] = ""
"TestMod by Fallowwing|https://pastebin.com/raw/SydjvVez\n"
"TestMod2 by Fallowwing|https://pastebin.com/raw/zuGa9n8A\n"
;

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

	{ /// Setup js interp
		jsInterp = new CTinyJS();
		registerFunctions(jsInterp);
		jsInterp->addNative("function print(text)", &js_print, 0);
		jsInterp->addNative("function append(text)", &js_append, 0);
		jsInterp->addNative("function addChoice(text, dest)", &js_addChoice, 0);
		jsInterp->addNative("function submitPassage(text)", &js_submitPassage, 0);
		jsInterp->addNative("function gotoPassage(text)", &js_gotoPassage, 0);
		jsInterp->execute("print(\"JS engine init\");");
	}

	strcpy(engine->spriteData.defaultFont, "OpenSans-Regular_20"); //@cleanup I shouldn't have to do this in every new project
	game = (GameStruct *)zalloc(sizeof(GameStruct));

	{ /// Parse modRepo
		const char *lineStart = modRepo;
		for (int i = 0;; i++) {
			const char *lineEnd = strstr(lineStart, "\n");
			if (!lineEnd) break;

			ModEntry *entry = &game->urlMods[game->urlModsNum++];
			memset(entry, 0, sizeof(ModEntry));

			char line[URL_LIMIT+MOD_NAME_MAX+1] = {};
			strncpy(line, lineStart, lineEnd-lineStart);

			char *barPos = strstr(line, "|");
			strncpy(entry->name, line, barPos - line);
			strcpy(entry->url, barPos+1);

			lineStart = lineEnd+1;
		}
	}

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

		{ /// Browser bg
			MintSprite *spr = createMintSprite();
			spr->setupRect(engine->width*0.25, engine->height*0.5, 0x111111);

			game->browserBg = spr;
		}

		{ /// Load url button
			Button *btn = createButton("Load url");
			game->bg->addChild(btn->sprite);
			btn->sprite->gravitate(0.5, 0.70);

			game->loadUrlButton = btn;
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
			spr->setText("Mod load failed");
			game->bg->addChild(spr);

			game->mainText = spr;
		}
	}

	if (game->state == STATE_MENU) {
		game->title->destroy();
		destroyButton(game->loadButton);
		destroyButton(game->loadUrlButton);
	}

	game->state = newState;
}

void mainUpdate() {
	if (game->state == STATE_MENU) {
		if (game->loadButton->sprite->justPressed) {
#ifdef SEMI_WIN32
#if 1
			const char *jsTest = ""
				"START_PASSAGE\r\n"
				"Test Passage\r\n"
				"This is a test of a passage, \"no need to escape quotes\"\r\n"
				"[With Simple Buttons]\r\n"
				"[And With Complex Buttons|With Complex Buttons]\r\n"
				"END_PASSAGE\r\n"
				"\r\n"
				"START_PASSAGE\r\n"
				"With Simple Buttons\r\n"
				"You pressed the Simple Button\r\n"
				"[Go back|Test Passage]\r\n"
				"END_PASSAGE\r\n"
				"\r\n"
				"START_PASSAGE\r\n"
				"With Complex Buttons\r\n"
				"You pressed the Complex Button\r\n\r\n"
				"[Go back|Test Passage]\r\n"
				"END_PASSAGE\r\n"
				"\r\n"
				"gotoPassage(\"Test Passage\");"
				;
#else
			const char *jsTest = ""
				"START_PASSAGE\n"
				"Test Passage\n"
				"This is a test of a passage, \"no need to escape quotes\"\n"
				"[With Simple Buttons]\n"
				"[And With Complex Buttons|With Complex Buttons]\n"
				"END_PASSAGE\n"
				"\n"
				"START_PASSAGE\n"
				"With Simple Buttons\n"
				"You pressed the Simple Button\n"
				"[Go back|Test Passage]\n"
				"END_PASSAGE\n"
				"\n"
				"START_PASSAGE\n"
				"With Complex Buttons\n"
				"You pressed the Complex Button\n\n"
				"[Go back|Test Passage]\n"
				"END_PASSAGE\n"
				"\n"
				"gotoPassage(\"Test Passage\");\n"
				;
#endif
			loadMod((char *)jsTest);
#else
			platformLoadFromDisk(loadMod);
#endif
		}

		if (game->loadUrlButton->sprite->justPressed)
			platformLoadFromUrl("https://pastebin.com/raw/SydjvVez", loadMod);
	}

	if (game->state == STATE_MOD) {
		for (int i = 0; i < game->choicesNum; i++) {
			if (game->choices[i]->sprite->justPressed) {
				gotoPassage(game->choices[i]->destPassageName);
			}
		}
	}
}

void loadMod(char *serialData) {
	if (game->state != STATE_MOD) changeState(STATE_MOD);
	// printf("Loaded data: %s\n", serialData);
	char *inputData = (char *)zalloc(SERIAL_SIZE);

	char realData[HUGE_STR] = {};
	strcat(realData, "var __passage = \"\";\n\n");

	int serialLen = strlen(serialData);
	int inputLen = 0;
	for (int i = 0; i < serialLen; i++)
		if (serialData[i] != '\r')
			inputData[inputLen++] = serialData[i];

	bool inPassage = false;
	const char *lineStart = inputData;
	for (int i = 0;; i++) {
		const char *lineEnd = strstr(lineStart, "\n");
		if (!lineEnd) {
			if (strlen(lineStart) == 0) break;
			else lineEnd = lineStart+strlen(lineStart);
		}

		char line[LARGE_STR] = {};
		strncpy(line, lineStart, lineEnd-lineStart);
		// printf("Line: %s\n", line);
		// dumpHex(line, strlen(line));
		
		if (strstr(line, "START_PASSAGE")) {
			inPassage = true;
			strcat(realData, "__passage = \"\";");
		} else if (strstr(line, "END_PASSAGE")) {
			inPassage = false;
			strcat(realData, "submitPassage(__passage);");
		} else if (inPassage) {
			char parsedLine[LARGE_STR] = {}; //@cleanup What size should this be?
			strcat(parsedLine, "__passage += \"");
			for (int lineIndex = 0; lineIndex < strlen(line); lineIndex++) {
				if (line[lineIndex] == '"') strcat(parsedLine, "\\\"");
				else strncat(parsedLine, &line[lineIndex], 1);
			}
			strcat(parsedLine, "\n");
			strcat(parsedLine, "\";");
			strcat(realData, parsedLine);
		} else {
			strcat(realData, line);
		}
			strcat(realData, "\n");

		lineStart = lineEnd+1;
	}

	printf("Gonna exec:\n%s\n", realData);
	// exit(1);

	free(inputData);
	jsInterp->execute(realData);
}

void destroyButton(Button *btn) {
	btn->exists = false;
	btn->sprite->destroy();
}

Button *createButton(const char *text, int width, int height) {
	int slot;
	for (slot = 0; slot < BUTTON_MAX; slot++)
		if (!game->buttons[slot].exists)
			break;
	assert(slot < BUTTON_MAX);

	Button *btn = &game->buttons[slot];
	btn->exists = true;

	{ /// Button sprite
		MintSprite *spr = createMintSprite();
		spr->setupRect(width, height, 0x444444);

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

void clear() {
	game->mainText->setText("");
	for (int i = 0; i < game->choicesNum; i++) destroyButton(game->choices[i]);
	game->choicesNum = 0;
}

void gotoPassage(const char *passageName) {
	clear();

	// printf("Passages %d\n", game->passagesNum);
	for (int i = 0; i < game->passagesNum; i++) {
		Passage *passage = game->passages[i];
		// printf("Checking passage %s\n", passage->name);
		if (streq(passage->name, passageName)) {
			append(passage->appendData);
			for (int choiceIndex = 0; choiceIndex < passage->choicesNum; choiceIndex++) {
				char *choiceStr = passage->choices[choiceIndex];
				char *barLoc = strstr(choiceStr, "|");
				char text[CHOICE_TEXT_MAX] = {};
				char dest[PASSAGE_NAME_MAX] = {};
				if (barLoc) {
					strncpy(text, choiceStr+1, barLoc-choiceStr-1);
					strncpy(dest, barLoc+1, strlen(barLoc)-2);
				} else {
					strncpy(text, choiceStr+1, strlen(choiceStr)-2);
					strcpy(dest, text);
				}
				addChoice(text, dest);
			}
			return;
		}
	}

	printf("Failed to find passage %s\n", passageName);
	exit(1);
}

void append(const char *text) {
	strcat(game->mainText->rawText, text);
	game->mainText->setText(game->mainText->rawText);
}

void addChoice(const char *text, const char *dest) {
	assert(game->choicesNum+1 <= CHOICE_BUTTON_MAX);

	Button *btn = createButton(text);
	btn->sprite->x = (btn->sprite->width+5) * game->choicesNum;
	btn->sprite->y = engine->height - btn->sprite->height;

	strcpy(btn->destPassageName, dest);
	game->choices[game->choicesNum++] = btn;
}

void js_print(CScriptVar *v, void *userdata) {
	const char *arg1 = v->getParameter("text")->getString().c_str();
	printf("> %s\n", arg1);
}

void js_append(CScriptVar *v, void *userdata) {
	const char *arg1 = v->getParameter("text")->getString().c_str();
	append(arg1);
}

void js_addChoice(CScriptVar *v, void *userdata) {
	const char *arg1 = v->getParameter("text")->getString().c_str();
	const char *arg2 = v->getParameter("dest")->getString().c_str();
	addChoice(arg1, arg2);
}

void js_submitPassage(CScriptVar *v, void *userdata) {
	const char *arg1 = v->getParameter("text")->getString().c_str();

	char delim[3];
	if (strstr(arg1, "\r\n")) strcpy(delim, "\r\n");
	else strcpy(delim, "\n\0");

	const char *lineStart = arg1;

	Passage *passage = (Passage *)zalloc(sizeof(Passage));
	for (int i = 0;; i++) {
		const char *lineEnd = strstr(lineStart, delim);
		if (!lineEnd) break;

		char line[LARGE_STR] = {};
		strncpy(line, lineStart, lineEnd-lineStart);

		if (i == 0) strcpy(passage->name, line);
		else if (line[0] == '[') strcpy(passage->choices[passage->choicesNum++], line); //@incomplete Assert this push
		else strcat(passage->appendData, line);

		lineStart = lineEnd+1;
	}
	// printf("-----\nPassage name: %s\nData:\n%s\n", passage->name, passage->appendData);
	// for (int i = 0; i < passage->choicesNum; i++) printf("Button: %s\n", passage->choices[i]);

	assert(game->passagesNum+1 <= PASSAGE_MAX);
	game->passages[game->passagesNum++] = passage;
}

void js_gotoPassage(CScriptVar *v, void *userdata) {
	const char *arg1 = v->getParameter("text")->getString().c_str();
	gotoPassage(arg1);
}

void dumpHex(const void* data, size_t size) {
	char ascii[17];
	size_t i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i) {
		printf("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			printf(" ");
			if ((i+1) % 16 == 0) {
				printf("|  %s \n", ascii);
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					printf(" ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					printf("   ");
				}
				printf("|  %s \n", ascii);
			}
		}
	}
}
