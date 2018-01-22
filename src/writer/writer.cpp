namespace Writer {
#include "writer.h"

#define CHOICE_BUTTON_MAX 4
#define BUTTON_MAX 128
#define PASSAGE_NAME_MAX MED_STR
#define CHOICE_TEXT_MAX MED_STR
#define MOD_NAME_MAX MED_STR
#define AUTHOR_NAME_MAX MED_STR
#define CATEGORIES_NAME_MAX MED_STR
#define PASSAGE_MAX 1024
#define MOD_ENTRIES_MAX 16
#define MSG_MAX 64
#define IMAGES_MAX 128
#define AUDIOS_MAX 128
#define CATEGORIES_MAX 8
#define ENTRY_LIST_MAX 16

	const char *CENTER = "CENTER";
	const char *TOP = "TOP";
	const char *BOTTOM = "BOTTOM";
	const char *LEFT = "LEFT";
	const char *RIGHT = "RIGHT";

	char tempBytes[Megabytes(2)];
	char tempBytes2[Megabytes(2)];
	bool exists = false;

	const char *jsTest = ""
		"START_IMAGES\n"
		"img1\n"
		"data1\n"
		"---\n"
		"img2\n"
		"data2\n"
		"END_IMAGES\n"
		"\n"
		"\n"
		"START_PASSAGES\n"
		":Start\n"
		"!`apples = 1;`\n"
		"This passage shows off basic variables.\n"
		"And has two lines\n"
		"[Let's get to it|Main]\n"
		"---\n"
		":Main\n"
		"You have `apples` apples.\n"
		"[Gain an apple]\n"
		"[Lose an apple]\n"
		"---\n"
		":Gain an apple\n"
		"!`apples += 1`\n"
		"You gained an apple.\n"
		"[Go back|Main]\n"
		"---\n"
		":Lose an apple\n"
		"!`apples -= 1`\n"
		"You lost an apple.\n"
		"[Go back|Main]\n"
		"END_PASSAGES\n"
		"\n"
		"gotoPassage(\"Start\");\n"
		;

	enum GameState { STATE_NULL=0, STATE_MENU, STATE_MOD };
	enum MsgType { MSG_NULL=0, MSG_INFO, MSG_WARNING, MSG_ERROR };
	struct Button;
	struct Msg;
	struct Image;
	struct Audio;
	struct ModEntry;

	void changeState(GameState newState);
	void enableEntry(ModEntry *entry);
	void disableEntry(ModEntry *entry);
	void disableAllEntries();
	void enableCategory(const char *category);
	void urlModLoaded(char *serialData);
	void urlModSourceLoaded(char *serialData);
	void loadMod(char *serialData);
	Button *createButton(const char *text, int width=256, int height=128);
	void destroyButton(Button *btn);
	void gotoPassage(const char *passageName, bool skipClear=false);
	void append(const char *text);
	void addChoice(const char *text, const char *dest);
	void addImage(const char *path, const char *name);
	void msg(const char *str, MsgType type, ...);
	void destroyMsg(Msg *msg);
	Image *getImage(const char *name);
	void removeImage(const char *name);
	void removeImage(Image *img);
	void alignImage(const char *name, const char *gravity=CENTER);
	void clear();

	void js_print(CScriptVar *v, void *userdata);
	void js_append(CScriptVar *v, void *userdata);
	void js_addChoice(CScriptVar *v, void *userdata);
	void js_submitPassage(CScriptVar *v, void *userdata);
	void js_submitImage(CScriptVar *v, void *userdata);
	void js_submitAudio(CScriptVar *v, void *userdata);
	void js_gotoPassage(CScriptVar *v, void *userdata);
	void js_jumpToPassage(CScriptVar *v, void *userdata);
	void js_addImage(CScriptVar *v, void *userdata);
	void js_removeImage(CScriptVar *v, void *userdata);
	void js_centerImage(CScriptVar *v, void *userdata);
	void js_alignImage(CScriptVar *v, void *userdata);
	void js_moveImage(CScriptVar *v, void *userdata);
	void js_scaleImage(CScriptVar *v, void *userdata);
	void js_rotateImage(CScriptVar *v, void *userdata);
	void js_tintImage(CScriptVar *v, void *userdata);
	void js_playAudio(CScriptVar *v, void *userdata);
	void js_exitMod(CScriptVar *v, void *userdata);

	struct Passage {
		char name[PASSAGE_NAME_MAX];
		char appendData[HUGE_STR];
		char choices[CHOICE_BUTTON_MAX][CHOICE_TEXT_MAX];
		int choicesNum;
	};

	struct ModEntry {
		bool enabled;
		char name[MOD_NAME_MAX];
		char author[AUTHOR_NAME_MAX];
		char url[URL_LIMIT];
		char category[CATEGORIES_NAME_MAX];
		Button *button;
		Button *peakButton;
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

	struct Image {
		bool exists;
		char *name;
		MintSprite *sprite;
	};

	struct Audio {
		bool exists;
		char *name;
		Channel *channel;
	};

	struct WriterStruct {
		GameState state;
		ModEntry *currentMod;

		Button buttons[BUTTON_MAX];

		MintSprite *bg;
		MintSprite *title;
		MintSprite *subtitle;
		MintSprite *browserBg;

		MintSprite *categoryBg;
		Button *categoryButtons[CATEGORIES_MAX];
		int categoryButtonsNum;

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

		ModEntry *currentEntries[ENTRY_LIST_MAX];
		int currentEntriesNum;

		Msg msgs[MSG_MAX];

		Image images[IMAGES_MAX];
		Audio audios[AUDIOS_MAX];
	};

	WriterStruct *writer;
	CTinyJS *jsInterp;

	void initWriter(MintSprite *bgSpr) {
		printf("Init\n");
		exists = true;

		{ /// Setup js interp
			jsInterp = new CTinyJS();
			registerFunctions(jsInterp);
			registerMathFunctions(jsInterp);

			jsInterp->execute(
				"CENTER = \"CENTER\";\n"
				"TOP = \"TOP\";\n"
				"BOTTOM = \"BOTTOM\";\n"
				"LEFT = \"LEFT\";\n"
				"RIGHT = \"RIGHT\";\n"
				"function rndInt(min, max) {\n"
				"	return Math.randInt(min, max);\n"
				"}\n"
			);

			jsInterp->addNative("function print(text)", &js_print, 0);
			jsInterp->addNative("function append(text)", &js_append, 0);
			jsInterp->addNative("function addChoice(text, dest)", &js_addChoice, 0);
			jsInterp->addNative("function submitPassage(text)", &js_submitPassage, 0);
			jsInterp->addNative("function submitImage(text)", &js_submitImage, 0);
			jsInterp->addNative("function submitAudio(text)", &js_submitAudio, 0);
			jsInterp->addNative("function gotoPassage(text)", &js_gotoPassage, 0);
			jsInterp->addNative("function jumpToPassage(text)", &js_jumpToPassage, 0);
			jsInterp->addNative("function addImage(path, name)", &js_addImage, 0);
			jsInterp->addNative("function removeImage(name)", &js_removeImage, 0);
			jsInterp->addNative("function centerImage(name)", &js_centerImage, 0);
			jsInterp->addNative("function alignImage(name, gravity)", &js_alignImage, 0);
			jsInterp->addNative("function moveImage(name, x, y)", &js_moveImage, 0);
			jsInterp->addNative("function scaleImage(name, x, y)", &js_scaleImage, 0);
			jsInterp->addNative("function rotateImage(name, angle)", &js_rotateImage, 0);
			jsInterp->addNative("function tintImage(name, tint)", &js_tintImage, 0);
			jsInterp->addNative("function playAudio(path, name)", &js_playAudio, 0);
			jsInterp->addNative("function exitMod()", &js_exitMod, 0);
			jsInterp->execute("print(\"JS engine init\");");
		}

		engine->spriteData.tagMap->setString("ed22", "Espresso-Dolce_22");
		engine->spriteData.tagMap->setString("ed30", "Espresso-Dolce_30");
		engine->spriteData.tagMap->setString("ed38", "Espresso-Dolce_38");

		writer = (WriterStruct *)zalloc(sizeof(WriterStruct));
		writer->bg = bgSpr;

		{ /// Setup mod repo
			struct ModEntryDef {
				const char *name;
				const char *author;
				const char *url;
				const char *category;
			};

			ModEntryDef defs[] = {
				{
					"False Moon",
					"Kittery",
					"https://pastebin.com/raw/72bbnRK5",
					"Story"
				}, {
					"Origin Story",
					"Kittery",
					"https://pastebin.com/raw/T5rZ9ue6",
					"Story"
				}, {
					"Waking up",
					"Kittery",
					"https://pastebin.com/raw/XNbdjHGA",
					"Story"
				}, {
					"Sexy Time",
					"Kittery",
					"https://pastebin.com/raw/s9mat6wK",
					"Story"
				}, {
					"Basic mod",
					"Fallowwing",
					"https://pastebin.com/raw/zuGa9n8A",
					"Examples"
				}, {
					"Variables",
					"Fallowwing",
					"https://pastebin.com/raw/SydjvVez",
					"Examples"
				}, {
					"Image example",
					"Fallowwing",
					"https://www.dropbox.com/s/spxl6wtgjiyac0f/Images%20Test.txt?dl=1",
					"Examples"
				}, {
					"Audio example",
					"Fallowwing",
					"https://www.dropbox.com/s/hrfodb9brym4105/Audio%20Test.txt?dl=1",
					"Examples"
				}, {
					"TestMod2",
					"Kittery",
					"https://pastebin.com/raw/FTHQxiWy",
					"Tests"
				}, {
					"Morphious86's Test",
					"Morphious86",
					"https://pastebin.com/raw/0MBv7bpK",
					"Tests"
				}, {
					"Gryphon Fight",
					"Cade",
					"https://www.dropbox.com/s/xh0pcb6lypm8av6/Gryphon%20Fight.txt?dl=1",
					"Ports"
				}
			};

			int defsNum = ArrayLength(defs);

			for (int i = 0; i < defsNum; i++) {
				ModEntry *entry = &writer->urlMods[writer->urlModsNum++];
				memset(entry, 0, sizeof(ModEntry));

				strcpy(entry->name, defs[i].name);
				strcpy(entry->author, defs[i].author);
				strcpy(entry->url, defs[i].url);
				strcpy(entry->category, defs[i].category);
			}
		}

		changeState(STATE_MENU);
	}

	void changeState(GameState newState) {
		if (newState == STATE_MENU) {
			writer->currentEntriesNum = 0;
			writer->categoryButtonsNum = 0;

			{ /// Title
				MintSprite *spr = createMintSprite();
				spr->setupEmpty(writer->bg->width, 100);
				writer->bg->addChild(spr);
				strcpy(spr->defaultFont, "OpenSans-Regular_20");
				spr->setText("Writer");
				spr->gravitate(0.01, 0.005);

				writer->title = spr;
			}

			{ /// Subtitle
				MintSprite *spr = createMintSprite();
				spr->setupEmpty(writer->bg->width, 100);
				writer->title->addChild(spr);
				strcpy(spr->defaultFont, "OpenSans-Regular_20");

				spr->setText("A story tool");
				spr->y += spr->getHeight() + 5;

				writer->subtitle = spr;
			}

			{ /// Category bg
				MintSprite *spr = createMintSprite();
				spr->setupRect(writer->bg->width*0.1, writer->bg->height*0.5, 0x111111);
				writer->bg->addChild(spr);
				spr->gravitate(0.01, 0.3);

				writer->categoryBg = spr;
			}

			{ /// Browser bg
				MintSprite *spr = createMintSprite();
				spr->setupRect(writer->bg->width*0.5, writer->bg->height*0.70, 0x111111);
				writer->bg->addChild(spr);
				spr->x = writer->categoryBg->x + writer->categoryBg->getWidth() + 10;
				spr->y = writer->categoryBg->y;

				writer->browserBg = spr;
			}


			{ /// Browser buttons
				for (int i = 0; i < writer->urlModsNum; i++) {
					ModEntry *entry = &writer->urlMods[i];
					enableEntry(entry);
				}
			}

			{ /// Category buttons
				const char *usedCategoryNames[CATEGORIES_MAX] = {};
				int usedCategoryNamesNum = 0;

				for (int i = 0; i < writer->urlModsNum; i++) {
					ModEntry *entry = &writer->urlMods[i];

			if (streq(entry->category, "Internal")) {
#if !defined(SEMI_INTERNAL)
				continue;
#endif
			}

					{ /// Check reuse
						bool beenUsed = false;
						for (int j = 0; j < usedCategoryNamesNum; j++)
							if (streq(entry->category, usedCategoryNames[j]))
								beenUsed = true;

						if (beenUsed) continue;
						usedCategoryNames[usedCategoryNamesNum++] = entry->category;
					}

					{ /// Category button
						Button *btn = createButton(entry->category, writer->categoryBg->width, 64);
						writer->categoryBg->addChild(btn->sprite);
						btn->sprite->y = (btn->sprite->getHeight() + 5) * writer->categoryButtonsNum;

						writer->categoryButtons[writer->categoryButtonsNum++] = btn;
					}
				}
			}

			{ /// Load button
				Button *btn = createButton("Load your own", 256, 32);
				writer->bg->addChild(btn->sprite);
				btn->sprite->gravitate(0.5, 0.95);

				writer->loadButton = btn;
			}

			{ /// Mod source text
				MintSprite *spr = createMintSprite();
				spr->setupEmpty(writer->bg->width*0.5, writer->bg->height);
				writer->bg->addChild(spr);
				strcpy(spr->defaultFont, "OpenSans-Regular_20");
				spr->setText("");
				spr->x = writer->browserBg->x + writer->browserBg->getWidth() + 10;
				spr->y = writer->browserBg->y;

				writer->modSourceText = spr;
			}

			enableCategory("Story");
		}

		if (newState == STATE_MOD) {
			{ /// Main text
				MintSprite *spr = createMintSprite();
				spr->setupEmpty(writer->bg->width, writer->bg->height*0.75);
				writer->bg->addChild(spr);
				strcpy(spr->defaultFont, "OpenSans-Regular_20");
				spr->setText("Mod load failed");
				spr->y += 30;

				writer->mainText = spr;
			}

			{ /// Exit button
				Button *btn = createButton("X", 50, 50);
				writer->bg->addChild(btn->sprite);
				btn->sprite->gravitate(1, 0.1);

				writer->exitButton = btn;
			}

			{ /// Refresh button
				Button *btn = createButton("R", 50, 50);
				writer->bg->addChild(btn->sprite);
				btn->sprite->gravitate(1, 0.2);

				writer->refreshButton = btn;
			}
		}

		if (writer->state == STATE_MENU) {
			disableAllEntries();

			for (int i = 0; i < writer->categoryButtonsNum; i++) {
				destroyButton(writer->categoryButtons[i]);
			}

			writer->title->destroy();
			writer->browserBg->destroy();
			writer->modSourceText->destroy();
			writer->categoryBg->destroy();
			destroyButton(writer->loadButton);
		}

		if (writer->state == STATE_MOD) {
			clear();
			for (int i = 0; i < writer->passagesNum; i++) free(writer->passages[i]);
			writer->passagesNum = 0;

			writer->mainText->destroy();
			destroyButton(writer->exitButton);
			destroyButton(writer->refreshButton);
		}

		writer->state = newState;
	}


	void updateWriter() {
		if (keyIsJustPressed('M')) msg("This is a test", MSG_ERROR);

		if (writer->state == STATE_MENU) {
			if (writer->loadButton->sprite->justPressed) {
#ifdef SEMI_WIN32
				loadMod((char *)jsTest);
#else
				platformLoadFromDisk(loadMod);
#endif
			}

			for (int i = 0; i < writer->categoryButtonsNum; i++) {
				Button *btn = writer->categoryButtons[i];
				if (btn->sprite->justPressed) {
					enableCategory(btn->tf->rawText);
				}
			}

			for (int i = 0; i < writer->currentEntriesNum; i++) {
				ModEntry *entry = writer->currentEntries[i];
				if (!entry->enabled) continue;

				if (entry->button->sprite->justPressed) {
					writer->currentMod = entry;
					platformLoadFromUrl(entry->url, urlModLoaded);
				}

				if (entry->peakButton->sprite->justPressed) platformLoadFromUrl(entry->url, urlModSourceLoaded);
				if (entry->sourceButton->sprite->justPressed) gotoUrl(entry->url);
			}
		}

		if (writer->state == STATE_MOD) {
			for (int i = 0; i < writer->choicesNum; i++) {
				if (writer->choices[i]->sprite->justPressed) {
					gotoPassage(writer->choices[i]->destPassageName);
				}
			}

			if (writer->exitButton->sprite->justPressed) changeState(STATE_MENU);

			if (writer->refreshButton->sprite->justPressed) {
				changeState(STATE_MENU);
				platformLoadFromUrl(writer->currentMod->url, urlModLoaded);
			}
		}

		{ /// Msgs
			for (int i = 0; i < MSG_MAX; i++) {
				Msg *msg = &writer->msgs[i];
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

	void enableCategory(const char *category) {
		disableAllEntries();

		for (int i = 0; i < writer->urlModsNum; i++) {
			ModEntry *entry = &writer->urlMods[i];
			if (streq(entry->category, category)) enableEntry(entry);
		}
	}

	void disableEntry(ModEntry *entry) {
		if (!entry->enabled) return;

		destroyButton(entry->button);
		destroyButton(entry->sourceButton);
		destroyButton(entry->peakButton);
	}

	void disableAllEntries() {
		for (int i = 0; i < writer->currentEntriesNum; i++) {
			ModEntry *entry = writer->currentEntries[i];
			disableEntry(entry);
		}

		writer->currentEntriesNum = 0;
	}

	void enableEntry(ModEntry *entry) {
		entry->enabled = true;
		writer->currentEntries[writer->currentEntriesNum++] = entry;

		{ /// Play button
			char buf[LARGE_STR];
			sprintf(buf, "%s by %s", entry->name, entry->author);
			Button *btn = createButton(buf, writer->browserBg->width*0.50, 64);
			writer->browserBg->addChild(btn->sprite);
			btn->sprite->y = (btn->sprite->getHeight() + 5) * (writer->currentEntriesNum-1);

			entry->button = btn;
		}

		{ /// Peak button
			Button *btn = createButton("peek source", writer->browserBg->width*0.20, 64);
			writer->browserBg->addChild(btn->sprite);
			btn->sprite->gravitate(1, 0);
			btn->sprite->y = entry->button->sprite->y;

			entry->peakButton = btn;
		}

		{ /// Source button
			Button *btn = createButton("view source", writer->browserBg->width*0.20, 64);
			writer->browserBg->addChild(btn->sprite);
			btn->sprite->gravitate(1, 0);
			btn->sprite->y = entry->button->sprite->y;
			btn->sprite->x -= entry->peakButton->sprite->getWidth() + 5;

			entry->sourceButton = btn;
		}
	}

	void urlModSourceLoaded(char *serialData) {
		writer->modSourceText->setText(serialData);
	}

	void urlModLoaded(char *serialData) {
		if (serialData) {
			loadMod(serialData);
		} else {
			msg("Failed to load.", MSG_ERROR);
		}
	}

	void loadMod(char *serialData) {
		for (int i = 0; i < writer->passagesNum; i++) free(writer->passages[i]);
		writer->passagesNum = 0;

		if (writer->state != STATE_MOD) changeState(STATE_MOD);
		// printf("Loaded data: %s\n", serialData);
		char *inputData = (char *)zalloc(SERIAL_SIZE);

		char *realData = tempBytes;
		tempBytes[0] = '\0';
		strcat(realData, "var __passage = \"\";\n\n");

		int serialLen = strlen(serialData);
		int inputLen = 0;
		for (int i = 0; i < serialLen; i++)
			if (serialData[i] != '\r')
				inputData[inputLen++] = serialData[i];

		const char *lineStart = inputData;
		bool inPassage = false;
		bool inImages = false;
		bool inAudio = false;
		for (int i = 0;; i++) {
			const char *lineEnd = strstr(lineStart, "\n");
			if (!lineEnd) {
				if (strlen(lineStart) == 0) break;
				else lineEnd = lineStart+strlen(lineStart);
			}

			char *line = tempBytes2;
			line[0] = '\0';
			strncpy(line, lineStart, lineEnd-lineStart);
			line[lineEnd-lineStart] = '\0';
			// printf("Line: %s\n", line);
			// dumpHex(line, strlen(line));

			if (strstr(line, "START_IMAGES")) {
				inImages = true;
				strcat(realData, "__image = \"\";");
			} else if (strstr(line, "END_IMAGES")) {
				inImages = false;
				strcat(realData, "submitImage(__image);");
			} else if (strstr(line, "START_AUDIO")) {
				inAudio = true;
				strcat(realData, "__audio = \"\";");
			} else if (strstr(line, "END_AUDIO")) {
				inAudio = false;
				strcat(realData, "submitAudio(__audio);");
			} else if (strstr(line, "START_PASSAGES")) {
				inPassage = true;
				strcat(realData, "__passage = \"\";");
			} else if (strstr(line, "END_PASSAGES")) {
				strcat(realData, "submitPassage(__passage);");
				inPassage = false;
			} else if (strstr(line, "---")) {
				if (inPassage) {
					strcat(realData, "submitPassage(__passage);");
					strcat(realData, "__passage = \"\";");
				} else if (inImages) {
					strcat(realData, "submitImage(__image);");
					strcat(realData, "__image = \"\";");
				} else if (inAudio) {
					strcat(realData, "submitAudio(__audio);");
					strcat(realData, "__audio = \"\";");
				}
			} else if (inPassage) {
				strcat(realData, "__passage += \"");
				for (int lineIndex = 0; lineIndex < strlen(line); lineIndex++) {
					if (line[lineIndex] == '"') strcat(realData, "\\\"");
					else strncat(realData, &line[lineIndex], 1);
				}
				strcat(realData, "\\n\";");
			} else if (inImages) {
				strcat(realData, "__image += \"");
				strcat(realData, line);
				strcat(realData, "\n\";");
			} else if (inAudio) {
				strcat(realData, "__audio += \"");
				strcat(realData, line);
				strcat(realData, "\n\";");
			} else {
				strcat(realData, line);
			}
			strcat(realData, "\n");

			lineStart = lineEnd+1;
		}

		// printf("Gonna exec:\n%s\n", realData);
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
			if (!writer->buttons[slot].exists)
				break;

		if (slot >= BUTTON_MAX) {
			msg("Too many buttons", MSG_ERROR);
			return NULL;
		}

		Button *btn = &writer->buttons[slot];
		btn->exists = true;

		{ /// Button sprite
			MintSprite *spr = createMintSprite();
			// spr->setupRect(width, height, 0x444444);
			spr->setup9Slice("ui/dialog/basicDialog", width, height, 15, 15, 30, 30);

			btn->sprite = spr;
		}

		{ /// Button text
			MintSprite *spr = createMintSprite();
			spr->setupEmpty(btn->sprite->getFrameWidth(), btn->sprite->getFrameHeight());
			strcpy(spr->defaultFont, "OpenSans-Regular_20");
			spr->setText(text);
			btn->sprite->addChild(spr);
			spr->gravitate(0.5, 0.5);

			btn->tf = spr;
		}

		return btn;
	}

	void clear() {
		for (int i = 0; i < IMAGES_MAX; i++) removeImage(&writer->images[i]);

		writer->mainText->setText("");
		for (int i = 0; i < writer->choicesNum; i++) destroyButton(writer->choices[i]);
		writer->choicesNum = 0;
	}

	void gotoPassage(const char *passageName, bool skipClear) {
		if (!skipClear) clear();

		// printf("Passages %d\n", writer->passagesNum);
		for (int i = 0; i < writer->passagesNum; i++) {
			Passage *passage = writer->passages[i];
			// printf("Checking passage %s\n", passage->name);
			if (streq(passage->name, passageName)) {
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
						bool printResult = true;
						if (lineStart != lineEnd) {
							if (*(lineEnd-1) == '!') {
								printResult = false;
								writer->mainText->unAppend(1);
							}
						}

						lineStart = lineEnd+1;
						lineEnd = strstr(lineStart, "`");
						if (!lineEnd) assert(0); // @incomplete need other backtick error

						strncpy(line, lineStart, lineEnd-lineStart);
						line[lineEnd-lineStart] = '\0';
						// printf("Gonna eval: %s\n", line);

						std::string resultString;
						try {
							if (printResult) {
								resultString = jsInterp->evaluate(line);
							} else {
								jsInterp->execute(line);
							}
						} catch (CScriptException *e) {
							msg(e->text.c_str(), MSG_ERROR);
							resultString = e->text.c_str();
							printResult = true;
						}
						if (writer->state != STATE_MOD) return;
						if (printResult) {
							if (streq(resultString.c_str(), "undefined")) append("`");
							else append(resultString.c_str());
						}
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
		strcat(writer->mainText->rawText, text);
		writer->mainText->setText(writer->mainText->rawText);
	}

	void addChoice(const char *text, const char *dest) {
		if (writer->choicesNum+1 > CHOICE_BUTTON_MAX) {
			msg("Too many choices", MSG_ERROR);
			return;
		}

		Button *btn = createButton(text);
		if (!btn) return;
		writer->bg->addChild(btn->sprite);
		btn->sprite->gravitate(0, 1);
		btn->sprite->x = (btn->sprite->width+5) * writer->choicesNum;

		strcpy(btn->destPassageName, dest);
		writer->choices[writer->choicesNum++] = btn;
	}

	void destroyMsg(Msg *msg) {
		msg->exists = false;
		msg->sprite->destroy();
	}

	void msg(const char *str, MsgType type, ...) {
		int slot;
		for (slot = 0; slot < MSG_MAX; slot++)
			if (!writer->msgs[slot].exists)
				break;

		assert(slot < MSG_MAX);
		Msg *msg = &writer->msgs[slot];
		msg->exists = true;
		msg->type = type;

		char buffer[HUGE_STR];
		{ /// Parse format
			va_list argptr;
			va_start(argptr, type);
			vsprintf(buffer, str, argptr);
			va_end(argptr);
		}

		{ /// Msg body text
			MintSprite *spr = createMintSprite();
			spr->setupEmpty(writer->bg->width*0.5, writer->bg->height*0.5);
			writer->bg->addChild(spr);

			msg->sprite = spr;
		}

		{ /// Type specific
			if (type == MSG_INFO) {
				msg->sprite->setText(buffer);
				msg->sprite->y = writer->bg->height;
			} else if (type == MSG_WARNING) {
				msg->sprite->setText(buffer);
				msg->sprite->y = writer->bg->height;
			} else if (type == MSG_ERROR) {
				msg->sprite->setText("<ed38>%s</ed38>", buffer);
				msg->sprite->gravitate(0.5, 0.9);
			}
		}
	}

	void addImage(const char *path, const char *name) {
		int slot;
		for (slot = 0; slot < IMAGES_MAX; slot++)
			if (!writer->images[slot].exists)
				break;

		if (slot >= IMAGES_MAX) {
			msg("Too many images", MSG_ERROR);
			return;
		}

		Image *img = &writer->images[slot];
		img->exists = true;
		img->name = stringClone(name);
		img->sprite = createMintSprite(path);
		writer->bg->addChild(img->sprite);
	}

	Image *getImage(const char *name) {
		for (int i = 0; i < IMAGES_MAX; i++)
			if (writer->images[i].exists)
				if (streq(writer->images[i].name, name))
					return &writer->images[i];

		return NULL;
	}

	void alignImage(const char *name, const char *gravity) {
		Image *img = getImage(name);

		if (!img) {
			msg("Image named %s doesn't exist", MSG_ERROR, name);
			return;
		}

		Point grav = {};
		if (streq(gravity, CENTER)) grav.setTo(0.5, 0.5);
		if (streq(gravity, TOP)) grav.setTo(0.5, 0);
		if (streq(gravity, BOTTOM)) grav.setTo(0.5, 1);
		if (streq(gravity, LEFT)) grav.setTo(0, 0.5);
		if (streq(gravity, RIGHT)) grav.setTo(1, 0.5);

		img->sprite->gravitate(grav.x, grav.y);
	}

	void removeImage(const char *name) {
		Image *img = getImage(name);
		if (!img) return;
		removeImage(img);
	}

	void removeImage(Image *img) {
		if (!img->exists) return;

		img->exists = false;
		img->sprite->destroy();
		free(img->name);
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

			if (i == 0) {
				if (line[0] != ':') {
					msg("Titles must start with a colon (%s)", MSG_ERROR, line);
					return;
				}

				strcpy(passage->name, &line[1]);
			} else if (line[0] == '[') {
				strcpy(passage->choices[passage->choicesNum++], line); //@incomplete Assert this push
			} else {
				strcat(passage->appendData, line);
				strcat(passage->appendData, "\n");
			}

			lineStart = lineEnd+1;
		}
		// printf("-----\nPassage name: %s\nData:\n%s\n", passage->name, passage->appendData);
		// for (int i = 0; i < passage->choicesNum; i++) printf("Button: %s\n", passage->choices[i]);

		if (writer->passagesNum+1 > PASSAGE_MAX) {
			msg("Too many passages", MSG_ERROR);
			return;
		}
		writer->passages[writer->passagesNum++] = passage;
	}

	void js_submitImage(CScriptVar *v, void *userdata) {
		const char *arg1 = v->getParameter("text")->getString().c_str();
		// printf("Got image: %s\n", arg1);

		char imageName[PATH_LIMIT];
		strcpy(imageName, "modPath/");

		const char *newline = strstr(arg1, "\n");
		strncat(imageName, arg1, newline-arg1);
		strcat(imageName, ".png");
		// printf("Image name: %s\n", imageName);

		char *b64Data = tempBytes;
		strcpy(b64Data, newline+1);
		b64Data[strlen(b64Data)-1] = '\0';

		const char *junkHeader = "data:image/png;base64,";
		if (stringStartsWith(b64Data, junkHeader)) {
			b64Data = tempBytes+strlen(junkHeader);
		}

		size_t dataLen;
		char *data = (char *)base64_decode((unsigned char *)b64Data, strlen(b64Data), &dataLen);
		addAsset(imageName, data, dataLen);
	}

	void js_submitAudio(CScriptVar *v, void *userdata) {
		const char *arg1 = v->getParameter("text")->getString().c_str();
		// printf("Got image: %s\n", arg1);

		char audioName[PATH_LIMIT];
		strcpy(audioName, "modPath/");

		const char *newline = strstr(arg1, "\n");
		strncat(audioName, arg1, newline-arg1);
		strcat(audioName, ".ogg");
		// printf("Audio name: %s\n", audioName);

		char *b64Data = tempBytes;
		strcpy(b64Data, newline+1);
		b64Data[strlen(b64Data)-1] = '\0';

		size_t dataLen;
		char *data = (char *)base64_decode((unsigned char *)b64Data, strlen(b64Data), &dataLen);
		addAsset(audioName, data, dataLen);
	}

	void js_gotoPassage(CScriptVar *v, void *userdata) {
		const char *arg1 = v->getParameter("text")->getString().c_str();
		gotoPassage(arg1);
	}

	void js_jumpToPassage(CScriptVar *v, void *userdata) {
		const char *arg1 = v->getParameter("text")->getString().c_str();
		gotoPassage(arg1, true);
	}

	void js_addImage(CScriptVar *v, void *userdata) {
		const char *arg1 = v->getParameter("path")->getString().c_str();
		const char *arg2 = v->getParameter("name")->getString().c_str();
		addImage(arg1, arg2);
	}

	void js_removeImage(CScriptVar *v, void *userdata) {
		const char *arg1 = v->getParameter("name")->getString().c_str();
		removeImage(arg1);
	}

	void js_alignImage(CScriptVar *v, void *userdata) {
		const char *arg1 = v->getParameter("name")->getString().c_str();
		const char *arg2 = v->getParameter("gravity")->getString().c_str();
		alignImage(arg1, arg2);
	}

	void js_moveImage(CScriptVar *v, void *userdata) {
		const char *name = v->getParameter("name")->getString().c_str();
		double x = v->getParameter("x")->getDouble();
		double y = v->getParameter("y")->getDouble();

		Image *img = getImage(name);

		if (!img) {
			msg("Image named %s cannot be moved because it doesn't exist", MSG_ERROR, name);
			return;
		}

		img->sprite->gravitate(x, y);
	}

	void js_scaleImage(CScriptVar *v, void *userdata) {
		const char *name = v->getParameter("name")->getString().c_str();
		double x = v->getParameter("x")->getDouble();
		double y = v->getParameter("y")->getDouble();

		Image *img = getImage(name);

		if (!img) {
			msg("Image named %s cannot be scaled because it doesn't exist", MSG_ERROR, name);
			return;
		}

		img->sprite->scale(x, y);
	}

	void js_rotateImage(CScriptVar *v, void *userdata) {
		const char *name = v->getParameter("name")->getString().c_str();
		double angle = v->getParameter("angle")->getDouble();
		Image *img = getImage(name);

		if (!img) {
			msg("Image named %s cannot be rotated because it doesn't exist", MSG_ERROR, name);
			return;
		}

		img->sprite->rotation = angle;
	}

	void js_tintImage(CScriptVar *v, void *userdata) {
		const char *name = v->getParameter("name")->getString().c_str();
		double tint = v->getParameter("tint")->getInt();
		Image *img = getImage(name);

		if (!img) {
			msg("Image named %s cannot be rotated because it doesn't exist", MSG_ERROR, name);
			return;
		}

		img->sprite->tint = tint;
	}

	void js_playAudio(CScriptVar *v, void *userdata) {
		const char *arg1 = v->getParameter("path")->getString().c_str();
		const char *arg2 = v->getParameter("name")->getString().c_str();
		playSound(arg1, arg2);
	}

	void js_exitMod(CScriptVar *v, void *userdata) {
		changeState(STATE_MENU);
	}

	void js_centerImage(CScriptVar *v, void *userdata) {
		const char *arg1 = v->getParameter("name")->getString().c_str();
		alignImage(arg1, CENTER);
	}

	void deinitWriter() {
		if (writer->state != STATE_NULL) changeState(STATE_NULL);
		writer->bg->destroy();
		exists = false;
	}
}
