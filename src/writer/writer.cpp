#include "writer.h"
#include "mjs.h"

#define CHOICE_BUTTON_MAX 4
#define BUTTON_MAX 128
#define PASSAGE_NAME_MAX MED_STR // MED_STR is 256
#define CHOICE_TEXT_MAX MED_STR
#define MOD_NAME_MAX MED_STR
#define AUTHOR_NAME_MAX MED_STR
#define CATEGORIES_NAME_MAX MED_STR
#define PASSAGE_MAX 1024
#define MOD_ENTRIES_MAX 64
#define MSG_MAX 64
#define IMAGES_MAX 128
#define ASSETS_MAX 128
#define CATEGORIES_MAX 8
#define ENTRY_LIST_MAX 64
#define VERSION_STR_MAX 16

#define BG1_LAYER 10
#define BG2_LAYER 20
#define DEFAULT_LAYER 30
#define TOOLTIP_BG_LAYER 40
#define TOOLTIP_TEXT_LAYER 50

#define BUTTON_ICONS_MAX 16
#define ICON_NAME_MAX MED_STR

#define TIMERS_MAX 128

namespace Writer {
	const char *CENTER = "CENTER";
	const char *TOP = "TOP";
	const char *BOTTOM = "BOTTOM";
	const char *LEFT = "LEFT";
	const char *RIGHT = "RIGHT";

	char tempBytes[Megabytes(2)];
	char tempBytes2[Megabytes(2)];
	bool exists = false;
	int lowestLayer;
	int oldDefaultLayer;
	float mouseMultiplier = 1;

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

	enum GameState { STATE_NULL=0, STATE_MENU, STATE_MOD, STATE_GRAPH, STATE_LOADING };
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
	void execJs(mjs_val_t expr);

	void loadModEntry(ModEntry *entry);
	void urlModLoaded(char *serialData);
	void urlModSourceLoaded(char *serialData);
	void loadMod(char *serialData);

	Button *createButton(const char *text, int width=256, int height=128);
	void destroyButton(Button *btn);

	void gotoPassage(const char *passageName);
	void append(const char *text);
	void addChoice(const char *text, void (*dest)(void *), void *userdata);
	void msg(const char *str, MsgType type, ...);
	void destroyMsg(Msg *msg);

	void playAudio(const char *path, const char *name);
	void clear();

	void showTooltipCursor(const char *str);

	void submitPassage(const char *data);
	void submitImage(const char *imgData);
	void submitAudio(const char *audioData);

	void print(char *str);
	void exitMod();

	float getTime();
	void addButtonIcon(const char *buttonText, const char *iconName);

	int timer(float delay, void (*onComplete)(void *), void *userdata);

	struct Passage {
		char name[PASSAGE_NAME_MAX];
		char appendData[HUGE_STR];
	};

	struct ModEntry {
		bool enabled;
		bool doneLoading;
		char name[MOD_NAME_MAX];
		char author[AUTHOR_NAME_MAX];
		char url[URL_LIMIT];
		char category[CATEGORIES_NAME_MAX];
		char version[VERSION_STR_MAX];
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
		float creationTime;
		MintSprite *icons[BUTTON_ICONS_MAX];
		int iconsNum;
		MintSprite *sprite;
		MintSprite *tf;
		void (*dest)(void *);
		void *userdata;
	};

	struct Image {
		bool exists;
		bool permanent;
		char *name;
		MintSprite *sprite;
	};

	struct Timer {
		bool exists;
		float delay;
		float timeLeft;
		void (*onComplete)(void *);
		void *userdata;
	};

	struct WriterStruct {
		GameState state;
		ModEntry *currentMod;
		StringMap *saveData;

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
		Button *nodesButton;

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

		bool tooltipShowing;
		MintSprite *tooltipTf;
		MintSprite *tooltipBg;

		char *execWhenDoneLoading;

		float scrollAmount;

		MintSprite *bgSprite1;
		MintSprite *bgSprite2;

		Timer timers[TIMERS_MAX];
	};

	mjs *mjs;
	mjs_val_t interUpdateFn;
	WriterStruct *writer;
}

//
//
//         HEADER END
//
//

#include "images.cpp"

#include "desktop.cpp"
#include "graph.cpp"

namespace Writer {
	void *mjsResolver(void *handle, const char *name) {
		if (streq(name, "print")) return (void *)print;
		if (streq(name, "submitPassage")) return (void *)submitPassage;
		if (streq(name, "submitImage")) return (void *)submitImage;
		if (streq(name, "gotoPassage")) return (void *)gotoPassage;
		if (streq(name, "append")) return (void *)append;
		if (streq(name, "exitMod")) return (void *)exitMod;
		if (streq(name, "addImage")) return (void *)addImage;
		if (streq(name, "alignImage")) return (void *)alignImage;
		if (streq(name, "moveImage")) return (void *)moveImage;
		if (streq(name, "moveImagePx")) return (void *)moveImagePx;
		if (streq(name, "scaleImage")) return (void *)scaleImage;
		if (streq(name, "rotateImage")) return (void *)rotateImage;
		if (streq(name, "tintImage")) return (void *)tintImage;
		if (streq(name, "playAudio")) return (void *)playAudio;
		if (streq(name, "submitAudio")) return (void *)submitAudio;
		if (streq(name, "clear")) return (void *)clear;

		if (streq(name, "permanentImage")) return (void *)permanentImage;

		if (streq(name, "submitNode")) return (void *)submitNode;
		if (streq(name, "attachNode")) return (void *)attachNode;
		if (streq(name, "getTime")) return (void *)getTime;
		if (streq(name, "addChoice")) return (void *)addChoice;
		if (streq(name, "addButtonIcon")) return (void *)addButtonIcon;

		if (streq(name, "getImageWidth")) return (void *)getImageWidth;
		if (streq(name, "getImageHeight")) return (void *)getImageHeight;
		if (streq(name, "getImageX")) return (void *)getImageX;
		if (streq(name, "getImageY")) return (void *)getImageY;
		if (streq(name, "removeImage")) return (void *)(void (*)(const char *))removeImage;

		if (streq(name, "timer")) return (void *)timer;

		if (streq(name, "addIcon")) return (void *)WriterDesktop::addIcon;
		if (streq(name, "createDesktop")) return (void *)WriterDesktop::createDesktop;
		if (streq(name, "attachImageToProgram")) return (void *)WriterDesktop::attachImageToProgram;
		if (streq(name, "startProgram")) return (void *)WriterDesktop::startProgram;
		//@incomplete rndInt

		return NULL;
	}

	void initWriter(MintSprite *bgSpr) {
		printf("Init\n");
		exists = true;

		getTextureAsset("Espresso-Dolce_22")->level = 3;
		getTextureAsset("Espresso-Dolce_30")->level = 3;
		getTextureAsset("Espresso-Dolce_38")->level = 3;

		engine->spriteData.tagMap->setString("ed22", "Espresso-Dolce_22");
		engine->spriteData.tagMap->setString("ed30", "Espresso-Dolce_30");
		engine->spriteData.tagMap->setString("ed38", "Espresso-Dolce_38");
		oldDefaultLayer = engine->spriteData.defaultLayer;
		engine->spriteData.defaultLayer = lowestLayer + DEFAULT_LAYER;

		writer = (WriterStruct *)zalloc(sizeof(WriterStruct));
		writer->bg = bgSpr;
		writer->saveData = stringMapCreate();

		{ /// Setup js interp
			{ /// mjs
				mjs = mjs_create();
				mjs_set_ffi_resolver(mjs, mjsResolver);

				execJs((char *)getAsset("info/interConfig.js")->data);

				mjs_val_t global = mjs_get_global(mjs);
				interUpdateFn = mjs_get(mjs, global, "__update", strlen("__update"));
			}
		}

		{ /// Setup mod repo
			struct ModEntryDef {
				const char *name;
				const char *author;
				const char *url;
				const char *category;
				const char *version;
			};

			ModEntryDef defs[] = {
				{
					"False Moon",
					"Kittery",
					"https://www.dropbox.com/s/0dtce66wclhjcdo/False%20Moon.txt?dl=1",
					"Story",
					"0.0.1"
				}, {
					"Origin Story",
					"Kittery",
					"https://www.dropbox.com/s/2hrse6oyzxcfe64/Origin%20Story.txt?dl=1",
					"Story",
					"0.0.1"
				}, {
					"Waking up",
					"Kittery",
					"https://www.dropbox.com/s/5aa2isiqii5w0q2/Waking%20up.txt?dl=1",
					"Story",
					"0.0.1"
				}, {
					"Sexy Time",
					"Kittery",
					"https://www.dropbox.com/s/xse6y2hp6eve4jp/Sexy%20time.txt?dl=1",
					"Story",
					"0.0.1"
				}, {
					"Basic mod",
					"Fallowwing",
					"https://www.dropbox.com/s/ci9yidufa7zl69c/Basic%20Mod.txt?dl=1",
					"Examples",
					"1.0.0"
				}, {
					"Variables",
					"Fallowwing",
					"https://www.dropbox.com/s/3wf5gj4v013z8kc/Variables.txt?dl=1",
					"Examples",
					"1.0.0"
				}, {
					"Add choice example",
					"Fallowwing",
					"https://www.dropbox.com/s/g0anhgehafkad9c/addChoiceExample.phore?dl=1",
					"Examples",
					"0.1.0"
				}, {
					"Image example",
					"Fallowwing",
					"https://www.dropbox.com/s/d38cai15jsyseif/Images%20Test.txt?dl=1",
					"Examples",
					"1.0.0"
				}, {
					"Audio example",
					"Fallowwing",
					"https://www.dropbox.com/s/mkboihcsw38y077/Audio%20Test.txt?dl=1",
					"Examples",
					"1.0.0"
				}, {
					"Icon Example",
					"Fallowwing",
					"https://www.dropbox.com/s/sg83wwjz7l8ocd9/Icon%20Example.txt?dl=1",
					"Examples",
					"0.0.1"
				}, {
					"Gryphon Fight",
					"Cade",
					"https://www.dropbox.com/s/x72pgxgol5zi3ba/Gryphon%20Fight.txt?dl=1",
					"Ports",
					"1.0.0"
				}, {
					"Brightforest Googirl",
					"Silver",
					"https://www.dropbox.com/s/xfwaqwd38krh04k/Silver%20Mod.txt?dl=1",
					"Ports",
					"1.0.0"
				}, {
					"Morphious86's Test",
					"Morphious86",
					"https://pastebin.com/raw/0MBv7bpK",
					"Tests",
					"0.0.1"
				}, {
					"Main Menu",
					"John Johnson",
					"https://www.dropbox.com/s/naa8evyajjcmjw0/mainMenu.phore?dl=1",
					"Internal",
					"0.0.1"
				}, {
					"Intro",
					"Craig Barrel",
					"https://www.dropbox.com/s/ox4aqpkajswnnlf/introMod.phore?dl=1",
					"Internal",
					"0.0.1"
				}, {
					"Cade's Test",
					"Cade",
					"https://www.dropbox.com/s/8la0k6c12u5ozc7/testMod.txt?dl=1",
					"Tests",
					"0.0.1"
				}, {
					"Desktop Test",
					"Fallowwing",
					"https://www.dropbox.com/s/aselaeb3htueck3/Desktop%20Test.phore?dl=1",
					"Internal",
					"0.1.0"
				}, {
					"Test Nodes",
					"Fallowwing",
					"https://www.dropbox.com/s/c15v66k0opzr5dt/Test%20Nodes.phore?dl=1",
					"Internal",
					"0.0.1"
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
				strcpy(entry->version, defs[i].version);
			}
		}

		initGraph();
		changeState(STATE_MENU);

		{ /// Tooltip
			{ /// Tooltip tf
				MintSprite *spr = createMintSprite();
				spr->setupRect(512, 128, 0xFFFFFF);
				spr->setText("Test tooltip");
				spr->alpha = 0;
				spr->layer = lowestLayer + TOOLTIP_TEXT_LAYER;
				// strcpy(spr->defaultFont, "OpenSans-Regular_20");
				strcpy(spr->defaultFont, "Espresso-Dolce_22");
				spr->zooms = false;

				writer->tooltipTf = spr;
			}
		}

		{ /// Autorun
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
					if (strSimilar(autoRunMod, entry->name)) {
						found = true;
						loadModEntry(entry);
						break;
					}
				}
			}

			if (!found) msg("Failed to autorun mod named %s", MSG_ERROR, autoRunMod);
		}
	}

	void changeState(GameState newState) {
		GameState oldState = writer->state;

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

				spr->setText("A Story Tool");
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
				btn->sprite->alignInside(DIR8_DOWN_RIGHT, 0, 10);

				writer->loadButton = btn;
			}

			{ /// Nodes button
				Button *btn = createButton("See nodes", 256, 32);
				writer->bg->addChild(btn->sprite);
				btn->sprite->alignOutside(writer->loadButton->sprite, DIR8_LEFT, 10, 10);

				writer->nodesButton = btn;
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

		if (oldState == STATE_MENU) {
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
			destroyButton(writer->nodesButton);
		}

		if (newState == STATE_MOD) {
			{ /// Bg Sprite 1
				MintSprite *spr = createMintSprite();
				writer->bg->addChild(spr);
				spr->setupRect(engine->width, engine->height, 0xFF0000);
				spr->layer = lowestLayer + BG1_LAYER;
				spr->alpha = 0.3;

				writer->bgSprite1 = spr;
			}

			{ /// Bg Sprite 2
				MintSprite *spr = createMintSprite();
				writer->bg->addChild(spr);
				spr->setupRect(engine->width, engine->height, 0x00FF00);
				spr->layer = lowestLayer + BG2_LAYER;
				spr->alpha = 0.3;

				writer->bgSprite2 = spr;
			}

			{ /// Main text
				MintSprite *spr = createMintSprite();
				spr->setupEmpty(writer->bg->width - 64, 2048); //@hardcode 64 should be refresh button pos
				writer->bg->addChild(spr);
				// strcpy(spr->defaultFont, "OpenSans-Regular_20");
				strcpy(spr->defaultFont, "Espresso-Dolce_22");
				spr->setText("Mod load failed");
				spr->y += 30;
				spr->x += 30;

				Rect startRect;
				startRect.x = spr->x;
				startRect.y = spr->y;
				startRect.width = spr->width;
				startRect.height = writer->bg->height - startRect.x - 128 - 16; //@hardcode Buttons are 128px, padding is 16px

				writer->bg->localToGlobal(&startRect);
				spr->clipRect.setTo(startRect.x, startRect.y, startRect.width, startRect.height);

				writer->mainText = spr;
			}

			{ /// Exit button
				MintSprite *spr = createMintSprite("writer/exit.png");
				writer->bg->addChild(spr);
				spr->scale(2, 2);
				spr->alignInside(DIR8_UP_RIGHT, 10, 50);

				writer->exitButton = spr;
			}

			{ /// Refresh button
				MintSprite *spr = createMintSprite("writer/restart.png");
				writer->bg->addChild(spr);
				spr->scale(2, 2);
				spr->alignOutside(writer->exitButton, DIR8_DOWN, 10, 10);

				writer->refreshButton = spr;
			}
		}

		if (oldState == STATE_MOD) {
			clear();
			if (WriterDesktop::exists) WriterDesktop::destroyDesktop();
			//@incomplete Should namespace the passage names off somehow? To prevent mods from overwriting each other's passages?
			// for (int i = 0; i < writer->passagesNum; i++) Free(writer->passages[i]);
			// writer->passagesNum = 0;

			writer->mainText->destroy();
			writer->exitButton->destroy();
			writer->refreshButton->destroy();
			writer->bgSprite1->destroy();
			writer->bgSprite2->destroy();

			execJs("removeAllImages();");

			for (int i = 0; i < ASSETS_MAX; i++) {
				if (writer->loadedAssets[i]) {
					Free(writer->loadedAssets[i]->data);
					writer->loadedAssets[i]->exists = false;
					writer->loadedAssets[i] = NULL;
				}
			}
		}

		if (newState == STATE_GRAPH) {
			showGraph();
		}

		if (oldState == STATE_GRAPH) {
			hideGraph();
		}

		writer->state = newState;
	}


	void updateWriter() {
		execJs(interUpdateFn);
		if (keyIsJustPressed('M')) msg("This is a test", MSG_ERROR);

		if (WriterDesktop::exists) WriterDesktop::updateDesktop();

		if (writer->state == STATE_MENU) {
			if (writer->loadButton->sprite->justPressed) {
				playSound("audio/ui/load");
#ifdef SEMI_WIN32
				loadMod((char *)jsTest);
#else
				platformLoadFromDisk(loadMod);
#endif
			}

			if (writer->nodesButton->sprite->justPressed) {
				// playSound("audio/ui/altClick");
				changeState(STATE_GRAPH);
			}

			for (int i = 0; i < writer->categoryButtonsNum; i++) {
				Button *btn = writer->categoryButtons[i];
				if (btn->sprite->justPressed) {
					playSound("audio/ui/choiceClick");
					enableCategory(btn->tf->rawText);
				}
			}

			for (int i = 0; i < writer->currentEntriesNum; i++) {
				ModEntry *entry = writer->currentEntries[i];
				if (!entry->enabled) continue;

				if (entry->button->sprite->hovering) {
					char buf[512];
					sprintf(buf, "Version: %s", entry->version);
					showTooltipCursor(buf);
				}

				if (entry->button->sprite->justPressed) {
					playSound("audio/ui/choiceClick");
					loadModEntry(entry);
				}

				if (entry->peakButton->sprite->justPressed) {
					playSound("audio/ui/choiceClick");
					platformLoadFromUrl(entry->url, urlModSourceLoaded);
				}

				if (entry->sourceButton->sprite->justPressed) {
					playSound("audio/ui/choiceClick");
					gotoUrl(entry->url);
				}
			}
		}

		if (writer->state == STATE_MOD) {
			if (writer->execWhenDoneLoading && writer->currentMod->doneLoading) {
				execJs(writer->execWhenDoneLoading);
				Free(writer->execWhenDoneLoading);
				writer->execWhenDoneLoading = NULL;
			}

			{ /// Section: Scrolling
				if (platformMouseWheel < 0) writer->scrollAmount += 0.1;
				if (platformMouseWheel > 0) writer->scrollAmount -= 0.1;
				writer->scrollAmount = Clamp(writer->scrollAmount, 0, 1);

				float maxScroll = writer->mainText->textHeight - writer->mainText->clipRect.height;
				float minScroll = 30;

				if (maxScroll < minScroll) writer->scrollAmount = 0;

				writer->mainText->y -= (writer->mainText->y - (-writer->scrollAmount*maxScroll+minScroll))/10;
			}

			for (int i = 0; i < writer->choicesNum; i++) {
				Button *choiceButton = writer->choices[i];

				choiceButton->sprite->y = mathClampMap(engine->time, choiceButton->creationTime, choiceButton->creationTime+0.5, engine->height, engine->height - choiceButton->sprite->getHeight(), ELASTIC_OUT);

				for (int iconIndex = 0; iconIndex < choiceButton->iconsNum; iconIndex++) {
					MintSprite *spr = choiceButton->icons[iconIndex];

					char iconName[ICON_NAME_MAX];
					strcpy(iconName, spr->frames[spr->currentFrame].name);
					char *zeroChar = strstr(iconName, "0");
					if (zeroChar) *zeroChar = '\0';

					if (spr->hovering) showTooltipCursor(iconName);
				}

				if (choiceButton->sprite->justPressed) {
					playSound("audio/ui/choiceClick");
					choiceButton->dest(choiceButton->userdata);
				}
			}

			if (writer->exitButton->justPressed) {
				playSound("audio/ui/exit");
				changeState(STATE_MENU);
			}

			if (writer->refreshButton->justPressed) {
				playSound("audio/ui/restart");
				changeState(STATE_LOADING);
				loadModEntry(writer->currentMod);
			}
		}

		if (writer->state == STATE_GRAPH) {
			updateGraph();
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

		{ /// Tooltips
			if (writer->tooltipShowing) {
				writer->tooltipTf->alpha += 0.05;
			} else {
				writer->tooltipTf->alpha -= 0.05;
			}

			writer->tooltipShowing = false;
		}

		{ /// Timers
			for (int i = 0; i < TIMERS_MAX; i++) {
				Timer *curTimer = &writer->timers[i];
				if (!curTimer->exists) continue;

				curTimer->timeLeft -= engine->elapsed;
				if (curTimer->timeLeft <= 0) {
					curTimer->onComplete(curTimer->userdata);
					curTimer->exists = false;
				}
			}
		}
	}

	void loadModEntry(ModEntry *entry) {
		writer->currentMod = entry;
		writer->currentMod->doneLoading = false;
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

			// mjs_err_t mjs_call(struct mjs *mjs, mjs_val_t *res, mjs_val_t func, mjs_val_t this_val, int nargs, ...);

	void execJs(mjs_val_t expr) {
		mjs_err_t err = mjs_call(mjs, NULL, interUpdateFn, NULL, 0);
		if (err) {
			const char *errStr = mjs_strerror(mjs, err);
			msg("JS error: %s", MSG_ERROR, errStr);
			printf("JS error: %s\n", errStr);
			// assert(0);
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

		char *nextVar;
		char *curPos = buffer;
		const char *replacement = "let ";
		while ((nextVar = strstr(curPos, "var ")) != NULL) {
			if (nextVar != buffer) {
				char prevChar = *(nextVar-1);
				if (isalpha(prevChar) || isdigit(prevChar)) {
					curPos = nextVar + strlen(replacement);
					continue;
				}
			}
			strncpy(nextVar, replacement, strlen(replacement));
			curPos = nextVar + strlen(replacement);
		}

		// printf("Interping:\n%s\n", buffer);
		mjs_err_t err = mjs_exec(mjs, buffer, NULL);
		if (err) {
			const char *errStr = mjs_strerror(mjs, err);
			msg("JS error: %s", MSG_ERROR, errStr);
			printf("JS error: %s\n", errStr);
			// assert(0);
		}
	}

	void loadMod(char *serialData) {
		for (int i = 0; i < writer->passagesNum; i++) Free(writer->passages[i]);
		writer->passagesNum = 0;

		if (writer->state != STATE_MOD) changeState(STATE_MOD);
		// printf("Loaded data: %s\n", serialData);
		char *inputData = (char *)zalloc(SERIAL_SIZE);

		char *realData = (char *)zalloc(Megabytes(2));
		memset(realData, 0, Megabytes(2));
		realData[0] = '\0';
		char *realDataEnd = fastStrcat(realData, "var __passage = \"\";\n\n");
		realDataEnd = fastStrcat(realDataEnd, "var __image = \"\";\n\n");
		realDataEnd = fastStrcat(realDataEnd, "var __audio = \"\";\n\n");

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

		writer->execWhenDoneLoading = realData;
		if (!writer->currentMod) {
			ModEntry *entry = &writer->urlMods[writer->urlModsNum++];
			memset(entry, 0, sizeof(ModEntry));

			strcpy(entry->name, "unnamed");
			strcpy(entry->author, "unknown");
			strcpy(entry->url, "local");
			strcpy(entry->category, "unknown");
			strcpy(entry->version, "unknown");
			writer->currentMod = entry;
		}

		writer->currentMod->doneLoading = true; //@incomplete Do streaming
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
		memset(btn, 0, sizeof(Button));
		btn->exists = true;
		btn->creationTime = engine->time;

		{ /// Button sprite
			MintSprite *spr = createMintSprite();
			// spr->setupRect(width, height, 0x444444);
			spr->setup9Slice("ui/dialog/basicDialog", width, height, 15, 15, 30, 30);
			btn->sprite = spr;
		}

		{ /// Button text
			MintSprite *spr = createMintSprite();
			spr->setupEmpty(btn->sprite->getFrameWidth(), btn->sprite->getFrameHeight());
			// strcpy(spr->defaultFont, "OpenSans-Regular_20");
			strcpy(spr->defaultFont, "Espresso-Dolce_22");
			spr->setText(text);
			btn->sprite->addChild(spr);
			spr->alignInside(DIR8_CENTER);

			btn->tf = spr;
		}

		return btn;
	}

	void clear() {
			execJs("removeAllImages();");
		// for (int i = 0; i < IMAGES_MAX; i++) {
		// 	if (writer->images[i].exists && !writer->images[i].permanent) {
		// 		removeImage(&writer->images[i]);
		// 	}
		// }

		writer->mainText->setText("");
		for (int i = 0; i < writer->choicesNum; i++) destroyButton(writer->choices[i]);
		writer->choicesNum = 0;
	}

	void gotoPassage(const char *passageName) {
		writer->scrollAmount = 0;

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
						if (printResult) {
							prependStr(line, "append((");
							if (line[strlen(line)-1] == ';') line[strlen(line)-1] = '\0';
							strcat(line, "));");
							// printf("Gonna really eval: %s\n", line);
							execJs(line);
						} else {
							execJs(line);
						}
						if (writer->state != STATE_MOD) return;
						if (printResult) {
							if (streq(resultString.c_str(), "undefined")) append("`");
							else append(resultString.c_str());
						}
					}

					lineStart = lineEnd+1;
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

	void addChoice(const char *text, void (*dest)(void *), void *userdata) {
		if (writer->choicesNum+1 > CHOICE_BUTTON_MAX) {
			msg("Too many choices", MSG_ERROR);
			return;
		}

		Button *prevBtn = NULL;
		if (writer->choicesNum > 0) prevBtn = writer->choices[writer->choicesNum-1];

		Button *btn = createButton(text);
		if (!btn) return;
		// btn->sprite->scaleX = 0;
		// btn->tf->scaleX = 0;
		writer->bg->addChild(btn->sprite);

		if (!prevBtn) btn->sprite->alignInside(DIR8_DOWN_LEFT, 5, 5);
		else btn->sprite->alignOutside(prevBtn->sprite, DIR8_RIGHT, 5, 0);

		btn->dest = dest;
		btn->userdata = userdata;
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

		// assert(slot < MSG_MAX);
		if (slot >= MSG_MAX) return; //@incomplete This should return an actual error message of some kind
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

	void showTooltipCursor(const char *str) {
		if (!streq(writer->tooltipTf->rawText, str)) {
			writer->tooltipTf->setText(str);

			if (writer->tooltipBg) {
				writer->tooltipBg->destroy();
				writer->tooltipBg = NULL;
			}

			{ /// Tooltip bg
				MintSprite *spr = createMintSprite();
				spr->setupRect(writer->tooltipTf->getFrameWidth(), writer->tooltipTf->getFrameHeight(), 0x111111);
				spr->layer = lowestLayer + TOOLTIP_BG_LAYER;
				writer->tooltipTf->addChild(spr);

				writer->tooltipBg = spr;
			}

		}

		writer->tooltipTf->x = engine->mouseX*mouseMultiplier + 10;
		writer->tooltipTf->y = engine->mouseY*mouseMultiplier + 10;

		writer->tooltipShowing = true;
	}

	void submitPassage(const char *data) {
		char delim[3];
		if (strstr(data, "\r\n")) strcpy(delim, "\r\n");
		else strcpy(delim, "\n\0");

		const char *lineStart = data;

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
				char *barPos = strstr(line, "|");
				char text[CHOICE_TEXT_MAX] = {};
				char dest[PASSAGE_NAME_MAX] = {};

				if (barPos) {
					char *textStart = &line[1];
					char *textEnd = barPos;
					char *destStart = textEnd+1;
					char *destEnd = line+strlen(line)-1;
					strncpy(text, textStart, textEnd-textStart);
					strncpy(dest, destStart, destEnd-destStart);
				} else {
					strncpy(text, line+1, strlen(line)-2);
					strncpy(dest, line+1, strlen(line)-2);
				}

				char newLine[CHOICE_BUTTON_MAX + PASSAGE_NAME_MAX + 32];
				sprintf(newLine, "`addChoice(\"%s\", \"%s\");`\n", text, dest);
				strcat(passage->appendData, newLine);

				// strcpy(passage->choices[passage->choicesNum++], line); //@incomplete Assert this push
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

		bool addNew = true;

		for (int i = 0; i < writer->passagesNum; i++) {
			if (streq(writer->passages[i]->name, passage->name)) {
				Free(writer->passages[i]);
				writer->passages[i] = passage;
				addNew = false;
			}
		}

		if (addNew) writer->passages[writer->passagesNum++] = passage;
	}

	void submitImage(const char *imgData) {
		// printf("Got image: %s\n", imgData);

		char imageName[PATH_LIMIT];
		strcpy(imageName, "modPath/");

		const char *newline = strstr(imgData, "\n");
		strncat(imageName, imgData, newline-imgData);
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

	void print(char *str) {
		printf("print: %s\n", str);
	}

	void exitMod() {
		changeState(STATE_MENU);
	}

	void playAudio(const char *path, const char *name) {
		playSound(path, name);
	}

	void submitAudio(const char *audioData) {
		// printf("Got image: %s\n", audioData);

		char audioName[PATH_LIMIT];
		strcpy(audioName, "modPath/");

		const char *newline = strstr(audioData, "\n");
		strncat(audioName, audioData, newline-audioData);
		strcat(audioName, ".ogg");
		// printf("Audio name: %s\n", audioName);

		char *b64Data = tempBytes;
		strcpy(b64Data, newline+1);
		b64Data[strlen(b64Data)-1] = '\0';

		size_t dataLen;
		char *data = (char *)base64_decode((unsigned char *)b64Data, strlen(b64Data), &dataLen);
		addAsset(audioName, data, dataLen);
	}

	float getTime() {
		return engine->time;
	}

	void addButtonIcon(const char *buttonText, const char *iconName) {
		for (int i = 0; i < writer->choicesNum; i++) {
			Button *btn = writer->choices[i];
			if (!streq(btn->tf->rawText, buttonText)) continue;

			MintSprite *spr = createMintSprite("writer/icon.png");
			spr->playing = false;
			spr->gotoFrame(iconName);
			btn->sprite->addChild(spr);
			btn->icons[btn->iconsNum++] = spr;

#if 1
				spr->x = spr->width * (btn->iconsNum-1);
#else
			if (btn->iconsNum > 1) {
				spr->alignOutside(btn->icons[btn->iconsNum-1], DIR8_RIGHT);
			}
#endif
		}
	}

	int timer(float delay, void (*onComplete)(void *), void *userdata) {
		int slot;
		for (slot = 0; slot < TIMERS_MAX; slot++)
			if (!writer->timers[slot].exists)
				break;

		if (slot >= BUTTON_MAX) {
			msg("Too many timers", MSG_ERROR);
			return NULL;
		}

		Timer *curTimer = &writer->timers[slot];
		memset(curTimer, 0, sizeof(Timer));
		curTimer->exists = true;

		curTimer->delay = curTimer->timeLeft = delay;
		curTimer->onComplete = onComplete;
		curTimer->userdata = userdata;

		return slot;
	}

	void deinitWriter() {
		if (writer->state != STATE_NULL) changeState(STATE_NULL);
		writer->bg->destroy();
		exists = false;
		Free(writer);

		engine->spriteData.defaultLayer = oldDefaultLayer;
	}
}
