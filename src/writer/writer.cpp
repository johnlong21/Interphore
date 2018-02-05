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
#define ASSETS_MAX 128
#define CATEGORIES_MAX 8
#define ENTRY_LIST_MAX 16

namespace Writer {
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
	void execJs(const char *js, ...);
	void loadModEntry(ModEntry *entry);
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
	void js_permanentImage(CScriptVar *v, void *userdata);
	void js_removeImage(CScriptVar *v, void *userdata);
	void js_centerImage(CScriptVar *v, void *userdata);
	void js_alignImage(CScriptVar *v, void *userdata);
	void js_moveImage(CScriptVar *v, void *userdata);
	void js_moveImagePx(CScriptVar *v, void *userdata);
	void js_scaleImage(CScriptVar *v, void *userdata);
	void js_rotateImage(CScriptVar *v, void *userdata);
	void js_tintImage(CScriptVar *v, void *userdata);
	void js_playAudio(CScriptVar *v, void *userdata);
	void js_exitMod(CScriptVar *v, void *userdata);
	void js_installDesktopExtentions(CScriptVar *v, void *userdata);

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
		bool permanent;
		char *name;
		MintSprite *sprite;
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

		MintSprite *exitButton;
		MintSprite *refreshButton;
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

		Asset *loadedAssets[ASSETS_MAX];
	};

	CTinyJS *jsInterp;
	WriterStruct *writer;
}

//
//
//         HEADER END
//
//

#include "desktop.cpp"

namespace Writer {
	void initWriter(MintSprite *bgSpr) {
		printf("Init\n");
		exists = true;

		getTextureAsset("Espresso-Dolce_22")->level = 3;
		getTextureAsset("Espresso-Dolce_30")->level = 3;
		getTextureAsset("Espresso-Dolce_38")->level = 3;

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
			jsInterp->addNative("function permanentImage(name)", &js_permanentImage, 0);
			jsInterp->addNative("function removeImage(name)", &js_removeImage, 0);
			jsInterp->addNative("function centerImage(name)", &js_centerImage, 0);
			jsInterp->addNative("function alignImage(name, gravity)", &js_alignImage, 0);
			jsInterp->addNative("function moveImage(name, x, y)", &js_moveImage, 0);
			jsInterp->addNative("function moveImagePx(name, x, y)", &js_moveImagePx, 0);
			jsInterp->addNative("function scaleImage(name, x, y)", &js_scaleImage, 0);
			jsInterp->addNative("function rotateImage(name, angle)", &js_rotateImage, 0);
			jsInterp->addNative("function tintImage(name, tint)", &js_tintImage, 0);
			jsInterp->addNative("function playAudio(path, name)", &js_playAudio, 0);
			jsInterp->addNative("function exitMod()", &js_exitMod, 0);
			jsInterp->addNative("function installDesktopExtentions()", &WriterDesktop::js_installDesktopExtentions, 0);
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
					"https://www.dropbox.com/s/0dtce66wclhjcdo/False%20Moon.txt?dl=1",
					"Story"
				}, {
					"Origin Story",
					"Kittery",
					"https://www.dropbox.com/s/2hrse6oyzxcfe64/Origin%20Story.txt?dl=1",
					"Story"
				}, {
					"Waking up",
					"Kittery",
					"https://www.dropbox.com/s/5aa2isiqii5w0q2/Waking%20up.txt?dl=1",
					"Story"
				}, {
					"Sexy Time",
					"Kittery",
					"https://www.dropbox.com/s/xse6y2hp6eve4jp/Sexy%20time.txt?dl=1",
					"Story"
				}, {
					"Basic mod",
					"Fallowwing",
					"https://www.dropbox.com/s/ci9yidufa7zl69c/Basic%20Mod.txt?dl=1",
					"Examples"
				}, {
					"Variables",
					"Fallowwing",
					"https://www.dropbox.com/s/3wf5gj4v013z8kc/Variables.txt?dl=1",
					"Examples"
				}, {
					"Image example",
					"Fallowwing",
					"https://www.dropbox.com/s/d38cai15jsyseif/Images%20Test.txt?dl=1",
					"Examples"
				}, {
					"Audio example",
					"Fallowwing",
					"https://www.dropbox.com/s/mkboihcsw38y077/Audio%20Test.txt?dl=1",
					"Examples"
				}, {
					"Gryphon Fight",
					"Cade",
					"https://www.dropbox.com/s/x72pgxgol5zi3ba/Gryphon%20Fight.txt?dl=1",
					"Ports"
				}, {
					"Brightforest Googirl",
					"Silver",
					"https://www.dropbox.com/s/xfwaqwd38krh04k/Silver%20Mod.txt?dl=1",
					"Ports"
				}, {
					"Morphious86's Test",
					"Morphious86",
					"https://pastebin.com/raw/0MBv7bpK",
					"Tests"
				}, {
					"Cade's Test",
					"Cade",
					"https://www.dropbox.com/s/8la0k6c12u5ozc7/testMod.txt?dl=1",
					"Tests"
				}, {
					"Desktop Test",
					"Fallowwing",
					"https://www.dropbox.com/s/aselaeb3htueck3/Desktop%20Test.phore?dl=1",
					"Tests"
				}, {
					"Desktop Test Internal",
					"Fallowwing",
					"https://www.dropbox.com/s/o6rl3oo4n9g4teo/Desktop%20Test%20Fallow.phore?dl=1",
					"Internal"
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

		char autoRunMod[MED_STR] = {};
#ifdef AUTO_RUN
		strcpy(autoRunMod, SEMI_STRINGIFY(AUTO_RUN));
#endif

#ifdef SEMI_FLASH
		const char *modNamePrefix = "modName=";

		char *curUrl = flashGetUrl();
		char *modNamePos = strstr(curUrl, modNamePrefix);
		if (modNamePos) {
			modNamePos += strlen(modNamePrefix);
			strcpy(autoRunMod, modNamePos);
		}
		Free(curUrl);
#endif

		for (int i = 0; i < strlen(autoRunMod); i++)
			if (autoRunMod[i] == '-')
				autoRunMod[i] = ' ';


		bool found = true;
		if (autoRunMod[0] != '\0') {
			found = false;
			for (int i = 0; i < writer->urlModsNum; i++) {
				ModEntry *entry = &writer->urlMods[i];
				if (streq(autoRunMod, entry->name)) {
					found = true;
					loadModEntry(entry);
					break;
				}
			}
		}
		
		if (!found) msg("Failed to autorun mod named %s", MSG_ERROR, autoRunMod);
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
				spr->setText("Interphore");
				spr->alignInside(DIR8_UP_LEFT, 10, 10);

				writer->title = spr;
			}

			{ /// Subtitle
				MintSprite *spr = createMintSprite();
				spr->setupEmpty(writer->bg->width, 100);
				writer->bg->addChild(spr);
				strcpy(spr->defaultFont, "OpenSans-Regular_20");

				spr->setText("A story tool");
				spr->alignOutside(writer->title, DIR8_DOWN, 0, 5);

				writer->subtitle = spr;
			}

			{ /// Category bg
				MintSprite *spr = createMintSprite();
				spr->setupRect(writer->bg->width*0.1, writer->bg->height*0.5, 0x111111);
				writer->bg->addChild(spr);
				spr->alignInside(DIR8_LEFT, 20, 0);

				writer->categoryBg = spr;
			}

			{ /// Browser bg
				MintSprite *spr = createMintSprite();
				spr->setupRect(writer->bg->width*0.5, writer->bg->height*0.70, 0x111111);
				writer->bg->addChild(spr);
				spr->alignOutside(writer->categoryBg, DIR8_RIGHT, 20, 0);

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
				btn->sprite->alignInside(DIR8_DOWN, 0, 10);

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

			if (writer->currentMod) {
				enableCategory(writer->currentMod->category);
				writer->currentMod = NULL;
			} else {
				enableCategory("Story");
			}
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
				MintSprite *spr = createMintSprite("writer/exit.png");
				writer->bg->addChild(spr);
				spr->scale(2, 2);
				spr->alignInside(DIR8_UP_RIGHT, 0, 50);

				writer->exitButton = spr;
			}

			{ /// Refresh button
				MintSprite *spr = createMintSprite("writer/restart.png");
				writer->bg->addChild(spr);
				spr->scale(2, 2);
				spr->alignOutside(writer->exitButton, DIR8_DOWN, 0, 10);

				writer->refreshButton = spr;
			}
		}

		if (writer->state == STATE_MENU) {
			disableAllEntries();

			for (int i = 0; i < writer->categoryButtonsNum; i++) {
				destroyButton(writer->categoryButtons[i]);
			}

			writer->title->destroy();
			writer->subtitle->destroy();
			writer->browserBg->destroy();
			writer->modSourceText->destroy();
			writer->categoryBg->destroy();
			destroyButton(writer->loadButton);
		}

		if (writer->state == STATE_MOD) {
			clear();
			if (WriterDesktop::exists) WriterDesktop::destroyDesktop();
			for (int i = 0; i < writer->passagesNum; i++) Free(writer->passages[i]);
			writer->passagesNum = 0;

			writer->mainText->destroy();
			writer->exitButton->destroy();
			writer->refreshButton->destroy();

			for (int i = 0; i < IMAGES_MAX; i++) {
				if (writer->images[i].exists) removeImage(&writer->images[i]);
			}

			for (int i = 0; i < ASSETS_MAX; i++) {
				if (writer->loadedAssets[i]) {
					Free(writer->loadedAssets[i]->data);
					writer->loadedAssets[i]->exists = false;
					writer->loadedAssets[i] = NULL;
				}
			}
		}

		writer->state = newState;
	}


	void updateWriter() {
		if (keyIsJustPressed('M')) msg("This is a test", MSG_ERROR);

		if (WriterDesktop::exists) WriterDesktop::updateDesktop();

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
					loadModEntry(entry);
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

			if (writer->exitButton->justPressed) changeState(STATE_MENU);

			if (writer->refreshButton->justPressed) {
				changeState(STATE_MENU);
				loadModEntry(writer->currentMod);
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

	void loadModEntry(ModEntry *entry) {
		writer->currentMod = entry;
		platformLoadFromUrl(entry->url, urlModLoaded);
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
		ModEntry *prevEntry = NULL;
		if (writer->currentEntriesNum > 0) prevEntry = writer->currentEntries[writer->currentEntriesNum-1];

		entry->enabled = true;
		writer->currentEntries[writer->currentEntriesNum++] = entry;

		{ /// Play button
			char buf[LARGE_STR];
			sprintf(buf, "%s by %s", entry->name, entry->author);
			Button *btn = createButton(buf, writer->browserBg->width*0.50, 64);
			writer->browserBg->addChild(btn->sprite);

			if (!prevEntry) btn->sprite->alignInside(DIR8_UP_LEFT);
			else btn->sprite->alignOutside(prevEntry->button->sprite, DIR8_DOWN, 0, 5);

			entry->button = btn;
		}

		{ /// Peak button
			Button *btn = createButton("peek source", writer->browserBg->width*0.20, 64);
			writer->browserBg->addChild(btn->sprite);

			if (!prevEntry) btn->sprite->alignInside(DIR8_UP_RIGHT);
			else btn->sprite->alignOutside(prevEntry->peakButton->sprite, DIR8_DOWN, 0, 5);

			entry->peakButton = btn;
		}

		{ /// Source button
			Button *btn = createButton("view source", writer->browserBg->width*0.20, 64);
			writer->browserBg->addChild(btn->sprite);

			btn->sprite->alignOutside(entry->peakButton->sprite, DIR8_LEFT, 5, 5);

			entry->sourceButton = btn;
		}
	}

	void urlModSourceLoaded(char *serialData) {
		writer->modSourceText->setText(serialData);
	}

	void urlModLoaded(char *serialData) {
		if (serialData) {
			loadMod(serialData);
			Free(serialData);
		} else {
			msg("Failed to load.", MSG_ERROR);
		}
	}

	void execJs(const char *js, ...) {
		static char buffer[SERIAL_SIZE];
		{ /// Parse format
			va_list argptr;
			va_start(argptr, js);
			vsprintf(buffer, js, argptr);
			va_end(argptr);
		}

		try {
			jsInterp->execute(buffer);
		} catch (CScriptException *e) {
			msg(e->text.c_str(), MSG_ERROR);
		}
	}

	void loadMod(char *serialData) {
		for (int i = 0; i < writer->passagesNum; i++) Free(writer->passages[i]);
		writer->passagesNum = 0;

		if (writer->state != STATE_MOD) changeState(STATE_MOD);
		// printf("Loaded data: %s\n", serialData);
		char *inputData = (char *)zalloc(SERIAL_SIZE);

		char *realData = tempBytes;
		memset(tempBytes, 0, Megabytes(2));
		realData[0] = '\0';
		char *realDataEnd = fastStrcat(realData, "var __passage = \"\";\n\n");

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
			memcpy(line, lineStart, lineEnd-lineStart);
			line[lineEnd-lineStart] = '\0';
			// dumpHex(line, strlen(line));

			if (strstr(line, "START_IMAGES")) {
				inImages = true;
				realDataEnd = fastStrcat(realDataEnd, "__image = \"\";");
			} else if (strstr(line, "END_IMAGES")) {
				inImages = false;
				realDataEnd = fastStrcat(realDataEnd, "submitImage(__image);");
			} else if (strstr(line, "START_AUDIO")) {
				inAudio = true;
				realDataEnd = fastStrcat(realDataEnd, "__audio = \"\";");
			} else if (strstr(line, "END_AUDIO")) {
				inAudio = false;
				realDataEnd = fastStrcat(realDataEnd, "submitAudio(__audio);");
			} else if (strstr(line, "START_PASSAGES")) {
				inPassage = true;
				realDataEnd = fastStrcat(realDataEnd, "__passage = \"\";");
			} else if (strstr(line, "END_PASSAGES")) {
				realDataEnd = fastStrcat(realDataEnd, "submitPassage(__passage);");
				inPassage = false;
			} else if (strstr(line, "---")) {
				if (inPassage) {
					realDataEnd = fastStrcat(realDataEnd, "submitPassage(__passage);");
					realDataEnd = fastStrcat(realDataEnd, "__passage = \"\";");
				} else if (inImages) {
					realDataEnd = fastStrcat(realDataEnd, "submitImage(__image);");
					realDataEnd = fastStrcat(realDataEnd, "__image = \"\";");
				} else if (inAudio) {
					realDataEnd = fastStrcat(realDataEnd, "submitAudio(__audio);");
					realDataEnd = fastStrcat(realDataEnd, "__audio = \"\";");
				}
			} else if (inPassage) {
				realDataEnd = fastStrcat(realDataEnd, "__passage += \"");

				if (strstr(line, "\"")) {
					for (int lineIndex = 0; lineIndex < strlen(line); lineIndex++) {
						if (line[lineIndex] == '"') {
							realDataEnd = fastStrcat(realDataEnd, "\\\"");
						} else {
							*realDataEnd = line[lineIndex];
							realDataEnd++;
						}
					}
				} else {
					realDataEnd = fastStrcat(realDataEnd, line);
				}
				realDataEnd = fastStrcat(realDataEnd, "\\n\";");
			} else if (inImages) {
				realDataEnd = fastStrcat(realDataEnd, "__image += \"");
				realDataEnd = fastStrcat(realDataEnd, line);
				realDataEnd = fastStrcat(realDataEnd, "\n\";");
			} else if (inAudio) {
				realDataEnd = fastStrcat(realDataEnd, "__audio += \"");
				realDataEnd = fastStrcat(realDataEnd, line);
				realDataEnd = fastStrcat(realDataEnd, "\n\";");
			} else {
				realDataEnd = fastStrcat(realDataEnd, line);
			}
			realDataEnd = fastStrcat(realDataEnd, "\n");

			lineStart = lineEnd+1;
		}

		// printf("Gonna exec:\n%s\n", realData);
		// exit(1);

		Free(inputData);

		execJs(realData);
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
			spr->alignInside(DIR8_CENTER);

			btn->tf = spr;
		}

		return btn;
	}

	void clear() {
		for (int i = 0; i < IMAGES_MAX; i++) {
			if (writer->images[i].exists && !writer->images[i].permanent) {
				removeImage(&writer->images[i]);
			}
		}

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

		msg("Failed to find passage %s\n", MSG_ERROR, passageName);
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

		Button *prevBtn = NULL;
		if (writer->choicesNum > 0) prevBtn = writer->choices[writer->choicesNum-1];

		Button *btn = createButton(text);
		if (!btn) return;
		writer->bg->addChild(btn->sprite);

		if (!prevBtn) btn->sprite->alignInside(DIR8_DOWN_LEFT, 5, 5);
		else btn->sprite->alignOutside(prevBtn->sprite, DIR8_RIGHT, 5, 0);

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
		memset(msg, 0, sizeof(Msg));
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
				msg->sprite->alignInside(DIR8_DOWN, 0, 100);
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
		// img->sprite->centerPivot = true;
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
			msg("Can't align image named %s because it doesn't exist", MSG_ERROR, name);
			return;
		}

		Dir8 dir = DIR8_CENTER;
		if (streq(gravity, CENTER)) dir = DIR8_CENTER;
		if (streq(gravity, TOP)) dir = DIR8_UP;
		if (streq(gravity, BOTTOM)) dir = DIR8_DOWN;
		if (streq(gravity, LEFT)) dir = DIR8_LEFT;
		if (streq(gravity, RIGHT)) dir = DIR8_RIGHT;

		img->sprite->alignInside(dir);
	}

	void removeImage(const char *name) {
		Image *img = getImage(name);

		if (!img || !img->exists) {
			msg("Can't remove image named %s because it doesn't exist", MSG_ERROR, name);
			return;
		}

		removeImage(img);
	}

	void removeImage(Image *img) {
		if (!img || !img->exists) {
			msg("Can't remove image because it doesn't exist or is NULL", MSG_ERROR);
			return;
		}

		img->exists = false;
		img->sprite->destroy();
		Free(img->name);
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

		for (int i = 0; i < ASSETS_MAX; i++) {
			if (!writer->loadedAssets[i]) {
				writer->loadedAssets[i] = getAsset(imageName);
				break;
			}
		}
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

	void js_permanentImage(CScriptVar *v, void *userdata) {
		const char *arg1 = v->getParameter("name")->getString().c_str();
		Image *img = getImage(arg1);

		if (!img) {
			msg("Image named %s cannot be made permanent because it doesn't exist", MSG_ERROR, arg1);
			return;
		}

		img->permanent = true;
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

		img->sprite->x += img->sprite->getWidth() * x;
		img->sprite->y += img->sprite->getHeight() * y;
	}

	void js_moveImagePx(CScriptVar *v, void *userdata) {
		const char *name = v->getParameter("name")->getString().c_str();
		double x = v->getParameter("x")->getDouble();
		double y = v->getParameter("y")->getDouble();

		Image *img = getImage(name);

		if (!img) {
			msg("Image named %s cannot be moved because it doesn't exist", MSG_ERROR, name);
			return;
		}

		img->sprite->x = x;
		img->sprite->y = y;
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
		delete jsInterp;
		Free(writer);
	}
}
