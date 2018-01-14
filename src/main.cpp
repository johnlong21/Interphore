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

enum GameState { STATE_NULL=0, STATE_MENU, STATE_MOD };
enum MsgType { MSG_NULL=0, MSG_INFO, MSG_WARNING, MSG_ERROR };
struct Button;
struct Msg;

extern "C" void entryPoint();
void updateMain();
void initMain();
void changeState(GameState newState);
void urlModLoaded(char *serialData);
void urlModSourceLoaded(char *serialData);
void loadMod(char *serialData);
Button *createButton(const char *text, int width=256, int height=128);
void destroyButton(Button *btn);
void gotoPassage(const char *passageName);
void append(const char *text);
void addChoice(const char *text, const char *dest);
void msg(const char *str, MsgType type=MSG_INFO);
void destroyMsg(Msg *msg);
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
	char startJs[HUGE_STR];
	char choices[CHOICE_BUTTON_MAX][CHOICE_TEXT_MAX];
	int choicesNum;
};

struct ModEntry {
	char name[MOD_NAME_MAX];
	char url[URL_LIMIT];
	Button *button;
	Button *sourceButton;
};

struct Msg {
	bool exists;
	float elapsed;
	MsgType type;
	MintSprite *sprite;
};

struct Button {
	bool exists;
	MintSprite *sprite;
	MintSprite *tf;
	char destPassageName[PASSAGE_NAME_MAX];
};

struct GameStruct {
	GameState state;
	ModEntry *currentMod;

	Button buttons[BUTTON_MAX];

	MintSprite *bg;
	MintSprite *title;
	MintSprite *subtitle;
	MintSprite *browserBg;
	Button *exitButton;
	Button *refreshButton;
	MintSprite *modSourceText;

	Button *loadButton;

	MintSprite *mainText;
	Button *choices[CHOICE_BUTTON_MAX];
	int choicesNum;

	Passage *passages[PASSAGE_MAX];
	int passagesNum;

	ModEntry urlMods[MOD_ENTRIES_MAX];
	int urlModsNum;

	Msg msgs[MSG_MAX];
};

GameStruct *game;
CTinyJS *jsInterp;

const char modRepo[] = ""
"Basic mod example by Fallowwing|https://pastebin.com/raw/zuGa9n8A\n"
"Variables example by Fallowwing|https://pastebin.com/raw/SydjvVez\n"
"TestMod2 by Kittery|https://pastebin.com/raw/FTHQxiWy\n"
"Morphious86's Test|https://pastebin.com/raw/0MBv7bpK\n"
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
	initEngine(initMain, updateMain);
}

void initMain() {
	printf("Init\n");

	{ /// Setup js interp
		jsInterp = new CTinyJS();
		registerFunctions(jsInterp);
		jsInterp->addNative("function print(text)", &js_print, 0);
		jsInterp->addNative("function append(text)", &js_append, 0);
		jsInterp->addNative("function addChoice(text, dest)", &js_addChoice, 0);
		jsInterp->addNative("function submitPassage(text, startJs)", &js_submitPassage, 0);
		jsInterp->addNative("function gotoPassage(text)", &js_gotoPassage, 0);
		jsInterp->execute("print(\"JS engine init\");");
	}

	strcpy(engine->spriteData.defaultFont, "OpenSans-Regular_20"); //@cleanup I shouldn't have to do this in every new project
	engine->spriteData.tagMap->setString("ed22", "Espresso-Dolce_22");
	engine->spriteData.tagMap->setString("ed30", "Espresso-Dolce_30");
	engine->spriteData.tagMap->setString("ed38", "Espresso-Dolce_38");

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
			spr->gravitate(0.01, 0);

			game->title = spr;
		}

		{ /// Subtitle
			MintSprite *spr = createMintSprite();
			game->title->addChild(spr);
			spr->setupEmpty(engine->width, 100);

			spr->setText("A story tool");
			spr->y += spr->getHeight() + 5;

			game->subtitle = spr;
		}

		{ /// Browser bg
			MintSprite *spr = createMintSprite();
			spr->setupRect(engine->width*0.25, engine->height*0.5, 0x111111);
			game->bg->addChild(spr);
			spr->gravitate(0.3, 0.2);

			game->browserBg = spr;
		}

		{ /// Browser buttons
			for (int i = 0; i < game->urlModsNum; i++) {
				ModEntry *entry = &game->urlMods[i];

				{ /// Play button
					Button *btn = createButton(entry->name, game->browserBg->getWidth()*0.78, 64);
					game->browserBg->addChild(btn->sprite);
					btn->sprite->y = (btn->sprite->getHeight() + 5) * i;

					entry->button = btn;
				}

				{ /// Source button
					Button *btn = createButton("view source", game->browserBg->getWidth()*0.20, 64);
					game->browserBg->addChild(btn->sprite);
					btn->sprite->gravitate(1, 0);
					btn->sprite->y = entry->button->sprite->y;

					entry->sourceButton = btn;
				}
			}
		}

		{ /// Load button
			Button *btn = createButton("Load your own", 256, 64);
			game->bg->addChild(btn->sprite);
			btn->sprite->gravitate(0.5, 0.95);

			game->loadButton = btn;
		}

		{ /// Mod source text
			MintSprite *spr = createMintSprite();
			spr->setupEmpty(engine->width*0.5, engine->height);
			game->bg->addChild(spr);
			spr->setText("");
			spr->x = game->browserBg->x + game->browserBg->getWidth() + 50;
			spr->y = game->browserBg->y;

			game->modSourceText = spr;
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

		{ /// Exit button
			Button *btn = createButton("X", 50, 50);
			game->bg->addChild(btn->sprite);
			btn->sprite->gravitate(1, 0);

			game->exitButton = btn;
		}

		{ /// Refresh button
			Button *btn = createButton("R", 50, 50);
			game->bg->addChild(btn->sprite);
			btn->sprite->gravitate(1, 0.1);

			game->refreshButton = btn;
		}
	}

	if (game->state == STATE_MENU) {
		for (int i = 0; i < game->urlModsNum; i++)
			if (game->urlMods[i].button) {
				destroyButton(game->urlMods[i].button);
				destroyButton(game->urlMods[i].sourceButton);
			}

		game->title->destroy();
		game->browserBg->destroy();
		game->modSourceText->destroy();
		destroyButton(game->loadButton);
	}

	if (game->state == STATE_MOD) {
		clear();
		for (int i = 0; i < game->passagesNum; i++) free(game->passages[i]);
		game->passagesNum = 0;

		game->mainText->destroy();
		destroyButton(game->exitButton);
		destroyButton(game->refreshButton);
	}

	game->state = newState;
}

void updateMain() {
	if (keyIsJustPressed('M')) msg("This is a test", MSG_ERROR);

	if (game->state == STATE_MENU) {
		if (game->loadButton->sprite->justPressed) {
#ifdef SEMI_WIN32
			loadMod((char *)jsTest);
#else
			platformLoadFromDisk(loadMod);
#endif
		}

		for (int i = 0; i < game->urlModsNum; i++) {
			ModEntry *entry = &game->urlMods[i];
			if (entry->button->sprite->justPressed) {
				game->currentMod = entry;
				platformLoadFromUrl(entry->url, urlModLoaded);
			}
			if (entry->sourceButton->sprite->justPressed) {
				platformLoadFromUrl(entry->url, urlModSourceLoaded);
			}
		}
	}

	if (game->state == STATE_MOD) {
		for (int i = 0; i < game->choicesNum; i++) {
			if (game->choices[i]->sprite->justPressed) {
				gotoPassage(game->choices[i]->destPassageName);
			}
		}

		if (game->exitButton->sprite->justPressed) changeState(STATE_MENU);

		if (game->refreshButton->sprite->justPressed) {
			changeState(STATE_MENU);
			platformLoadFromUrl(game->currentMod->url, urlModLoaded);
		}
	}

	{ /// Msgs
		for (int i = 0; i < MSG_MAX; i++) {
			Msg *msg = &game->msgs[i];
			if (!msg->exists) continue;
			msg->elapsed += engine->elapsed;

			MintSprite *spr = msg->sprite;
			if (msg->type == MSG_INFO) {
				spr->y -= 3;
				if (spr->y < -spr->getHeight()) destroyMsg(msg);
			} else if (msg->type == MSG_WARNING) {
				spr->y -= 3;
				if (spr->y < -spr->getHeight()) destroyMsg(msg);
			} else if (msg->type == MSG_ERROR) {
				if (msg->elapsed > 5) spr->alpha -= 0.01;
				if (spr->alpha <= 0) destroyMsg(msg);
			}
		}
	}
}

void urlModSourceLoaded(char *serialData) {
	game->modSourceText->setText(serialData);
}

void urlModLoaded(char *serialData) {
	if (serialData) {
		loadMod(serialData);
	} else {
		msg("Failed to load.", MSG_ERROR);
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
	bool inJs = false;
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
		
		if (strstr(line, "START_PASSAGES")) {
			inPassage = true;
			strcat(realData, "__passage = \"\";");
			strcat(realData, "__startJs = \"\";");
		} else if (strstr(line, "---")) {
			strcat(realData, "submitPassage(__passage, __startJs);");
			strcat(realData, "__passage = \"\";");
			strcat(realData, "__startJs = \"\";");
		} else if (strstr(line, "END_PASSAGES")) {
			strcat(realData, "submitPassage(__passage, __startJs);");
			inPassage = false;
		} else if (strstr(line, "START_JS")) {
			inJs = true;
		} else if (strstr(line, "END_JS")) {
			inJs = false;
		} else if (inJs) {
			char parsedLine[LARGE_STR] = {}; //@cleanup What size should this be?
			strcat(parsedLine, "__startJs += \"");
			for (int lineIndex = 0; lineIndex < strlen(line); lineIndex++) {
				if (line[lineIndex] == '"') strcat(parsedLine, "\\\"");
				else strncat(parsedLine, &line[lineIndex], 1);
			}
			strcat(parsedLine, "\n\";");
			strcat(realData, parsedLine);
		} else if (inPassage) {
			char parsedLine[LARGE_STR] = {}; //@cleanup What size should this be?
			strcat(parsedLine, "__passage += \"");
			for (int lineIndex = 0; lineIndex < strlen(line); lineIndex++) {
				if (line[lineIndex] == '"') strcat(parsedLine, "\\\"");
				else strncat(parsedLine, &line[lineIndex], 1);
			}
			strcat(parsedLine, "\n\";");
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

	try {
		jsInterp->execute(realData);
	} catch (CScriptException *e) {
		msg(e->text.c_str(), MSG_ERROR);
	}
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

	if (slot >= BUTTON_MAX) {
		msg("Too many buttons", MSG_ERROR);
		return NULL;
	}

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
			jsInterp->execute(passage->startJs);

			const char *lineStart = passage->appendData;
			for (int i = 0;; i++) {
				const char *lineEnd = strstr(lineStart, "`");
				bool needToken = true;
				if (!lineEnd) {
					needToken = false;
					if (strlen(lineStart) == 0) break;
					else lineEnd = lineStart+strlen(lineStart);
				}

				char line[LARGE_STR] = {};
				strncpy(line, lineStart, lineEnd-lineStart);
				append(line);

				if (needToken) {
					lineStart = lineEnd+1;
					lineEnd = strstr(lineStart, "`");
					if (!lineEnd) assert(0); // @incomplete need other percent error

					strncpy(line, lineStart, lineEnd-lineStart);
					line[lineEnd-lineStart] = '\0';
				 std::string resultString = jsInterp->evaluate(line);
					if (streq(resultString.c_str(), "undefined")) append("`");
					else append(resultString.c_str());
				}

			lineStart = lineEnd+1;
		}

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
	if (game->choicesNum+1 > CHOICE_BUTTON_MAX) {
		msg("Too many choices", MSG_ERROR);
		return;
	}

	Button *btn = createButton(text);
	if (!btn) return;
	btn->sprite->x = (btn->sprite->width+5) * game->choicesNum;
	btn->sprite->y = engine->height - btn->sprite->height;

	strcpy(btn->destPassageName, dest);
	game->choices[game->choicesNum++] = btn;
}

void destroyMsg(Msg *msg) {
	msg->exists = false;
	msg->sprite->destroy();
}

void msg(const char *str, MsgType type) {
	int slot;
	for (slot = 0; slot < MSG_MAX; slot++)
		if (!game->msgs[slot].exists)
			break;

	assert(slot < MSG_MAX);
	Msg *msg = &game->msgs[slot];
	msg->exists = true;
	msg->type = type;

	{ /// Msg body text
		MintSprite *spr = createMintSprite();
		spr->setupEmpty(engine->width*0.5, engine->height*0.5);
		game->bg->addChild(spr);

		msg->sprite = spr;
	}

	{ /// Type specific
		if (type == MSG_INFO) {
			msg->sprite->setText(str);
			msg->sprite->y = engine->height;
		} else if (type == MSG_WARNING) {
			msg->sprite->setText(str);
			msg->sprite->y = engine->height;
		} else if (type == MSG_ERROR) {
			msg->sprite->setText("<ed38>%s</ed38>", str);
			msg->sprite->gravitate(0.5, 0.9);
		}
	}
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
	const char *arg2 = v->getParameter("startJs")->getString().c_str();

	char delim[3];
	if (strstr(arg1, "\r\n")) strcpy(delim, "\r\n");
	else strcpy(delim, "\n\0");

	const char *lineStart = arg1;

	Passage *passage = (Passage *)zalloc(sizeof(Passage));
	strcpy(passage->startJs, arg2);

	for (int i = 0;; i++) {
		const char *lineEnd = strstr(lineStart, delim);
		if (!lineEnd) break;

		char line[LARGE_STR] = {};
		strncpy(line, lineStart, lineEnd-lineStart);

		if (i == 0) {
			if (line[0] != ':') {
				char buf[LARGE_STR] = {};
				sprintf(buf, "Titles must start with a colon (%s)", line);
				msg(buf, MSG_ERROR);
				return;
			}

			strcpy(passage->name, &line[1]);
		} else if (line[0] == '[') {
			strcpy(passage->choices[passage->choicesNum++], line); //@incomplete Assert this push
		} else {
			strcat(passage->appendData, line);
		}

		lineStart = lineEnd+1;
	}
	// printf("-----\nPassage name: %s\nData:\n%s\n", passage->name, passage->appendData);
	// for (int i = 0; i < passage->choicesNum; i++) printf("Button: %s\n", passage->choices[i]);

	if (game->passagesNum+1 > PASSAGE_MAX) {
		msg("Too many passages", MSG_ERROR);
		return;
	}
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
