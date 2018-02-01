#define DESKTOP_ICONS_MAX 16
#define DESKTOP_ICONS_MAX_COLS 5
#define DESKTOP_PROGRAMS_MAX 4
#define DESKTOP_PROGRAM_IMAGES_MAX 16
#define DESKTOP_EVENTS_MAX 64
#define DESKTOP_CHOICES_MAX 8

namespace WriterDesktop {
	bool exists = false;

	struct DesktopProgram;
	struct DesktopIcon;
	struct DesktopEvent;

	void js_addIcon(CScriptVar *v, void *userdata);
	void js_installDesktopExtentions(CScriptVar *v, void *userdata);
	void js_createDesktop(CScriptVar *v, void *userdata);
	void js_attachImageToProgram(CScriptVar *v, void *userdata);
	void js_startProgram(CScriptVar *v, void *userdata);
	void js_addBookmarkBar(CScriptVar *v, void *userdata);
	void js_pushDesktopEvent(CScriptVar *v, void *userdata);
	void js_pushChoiceEvent(CScriptVar *v, void *userdata);

	void createDesktop();
	void destroyDesktop();
	void updateDesktop();

	void addIcon(const char *iconName, const char *iconImg);
	void removeIcon(const char *iconName);
	void removeIcon(DesktopIcon *icon);

	void startProgram(const char *programName, float width, float height);
	void exitProgram(DesktopProgram *program);

	void pushDesktopEvent(DesktopEvent *newEvent);

	struct DesktopChoice {
		MintSprite *tf;
		MintSprite *bg;
	};

	struct DesktopEvent {
		bool exists;
		bool started;
		bool done;
		char type[SHORT_STR];
		char text[MED_STR];
		char choices[DESKTOP_CHOICES_MAX][MED_STR];
		int choicesNum;
		int chosenIndex;
	};

	struct DesktopProgram {
		bool exists;
		int index;
		float scrollAmount;
		MintSprite *bg;
		MintSprite *content;
		MintSprite *titleBar;
		MintSprite *exitButton;
		Writer::Image *images[DESKTOP_PROGRAM_IMAGES_MAX];
		char programName[SHORT_STR];
		char state[MED_STR];
	};

	struct DesktopIcon {
		bool exists;
		int index;
		char name[SHORT_STR];
		MintSprite *sprite;
		MintSprite *tf;
	};

	struct DesktopStruct {
		bool desktopStarted;
		bool shuttingDown;

		MintSprite *bg;
		MintSprite *sleepButton;

		DesktopIcon icons[DESKTOP_ICONS_MAX];
		int iconsNum;

		DesktopProgram programs[DESKTOP_PROGRAMS_MAX];
		int programsNum;

		DesktopProgram *draggedProgram;

		MintSprite *dialogTf;

		DesktopChoice choices[DESKTOP_CHOICES_MAX];
		int choicesNum;

		DesktopEvent events[DESKTOP_EVENTS_MAX];
		int eventsNum;
		int currentEventIndex;
	};

	DesktopStruct *desktop;

	void js_installDesktopExtentions(CScriptVar *v, void *userdata) {
		using namespace Writer;
		jsInterp->addNative("function addIcon(iconText, iconImg)", &js_addIcon, 0);
		jsInterp->addNative("function createDesktop()", &js_createDesktop, 0);
		jsInterp->addNative("function attachImageToProgram(imageName, programName)", &js_attachImageToProgram, 0);
		jsInterp->addNative("function startProgram(programName, width, height)", &js_startProgram, 0);
		jsInterp->addNative("function addBookmarkBar(programName)", &js_addBookmarkBar, 0);
		jsInterp->addNative("function pushDesktopEvent(event)", &js_pushDesktopEvent, 0);
		jsInterp->addNative("function pushChoiceEvent(text, choices)", &js_pushChoiceEvent, 0);
		execJs(""
			"PROGRAM_STARTED = \"PROGRAM_STARTED\";"
			"CHOICE = \"CHOICE\";"
			"IMAGE_CLICK = \"IMAGE_CLICK\";"
			);
		clear();
	}

	void createDesktop() {
		using namespace Writer;
		desktop = (DesktopStruct *)zalloc(sizeof(DesktopStruct));

		exists = true;
		// Writer::destroyButton(writer->exitButton);
		// Writer::destroyButton(writer->refreshButton);

		{ /// Desktop bg
			MintSprite *spr = createMintSprite();
			// spr->setupRect(writer->bg->width, writer->bg->height, 0x222222);
			spr->setupRect(writer->bg->width*0.95, writer->bg->height*0.95, 0x222222);
			writer->bg->addChild(spr);
			spr->alignInside(DIR8_CENTER);
			spr->alpha = 0;

			desktop->bg = spr;
		}

		{ /// Sleep button
			MintSprite *spr = createMintSprite();
			spr->setupRect(64, 64, 0x333388);
			desktop->bg->addChild(spr);
			spr->alignInside(DIR8_DOWN_RIGHT, 5, 5);

			desktop->sleepButton = spr;
		}

		{ /// Dialog text
			MintSprite *spr = createMintSprite();
			spr->setupEmpty(engine->width*0.8, engine->height*0.25);
			strcpy(spr->defaultFont, "Espresso-Dolce_38");
			desktop->bg->addChild(spr);

			desktop->dialogTf = spr;
		}
	}

	void destroyDesktop() {
		exists = false;

		ForEach (DesktopIcon *icon, desktop->icons) {
			if (icon->exists) removeIcon(icon);
		}

		ForEach (DesktopProgram *program, desktop->programs) {
			if (program->exists) exitProgram(program);
		}

		desktop->bg->destroy();
		free(desktop);
	}

	void js_createDesktop(CScriptVar *v, void *userdata) {
		createDesktop();
	}

	void js_addIcon(CScriptVar *v, void *userdata) {
		addIcon(v->getParameter("iconText")->getString().c_str(), v->getParameter("iconImg")->getString().c_str());
	}

	void js_attachImageToProgram(CScriptVar *v, void *userdata) {
		const char *imageName = v->getParameter("imageName")->getString().c_str();
		const char *programName = v->getParameter("programName")->getString().c_str();

		using namespace Writer;
		Image *img = getImage(imageName);

		if (!img) {
			msg("Can't set the window of %s because it doesn't exist", MSG_ERROR, imageName);
			return;
		}

		if (img->sprite->parent) {
			img->sprite->parent->removeChild(img->sprite);
		}

		ForEach (DesktopProgram *program, desktop->programs) {
			if (!program->exists) continue;

			if (streq(program->programName, programName)) {
				for (int i = 0; i < DESKTOP_PROGRAM_IMAGES_MAX; i++) {
					if (!program->images[i]) {
						program->images[i] = img;
						program->content->addChild(img->sprite);
						return;
					}
				}

				msg("Can't put image %s in program because there are too many images", MSG_ERROR, imageName);
				return;
			}
		}

		msg("Couldn't find program named %s", MSG_ERROR, programName);
	}

	void js_startProgram(CScriptVar *v, void *userdata) {
		startProgram(v->getParameter("programName")->getString().c_str(), v->getParameter("width")->getDouble(), v->getParameter("height")->getDouble());
	}

	void updateDesktop() {
		if (!desktop->desktopStarted) {
			Writer::execJs("onDesktopStart();");
			desktop->desktopStarted = true;
		}

		//@cleanup Make this more of a state machine?
		bool canClickIcons = true;
		bool canClickPrograms = true;

		if (desktop->shuttingDown) {
			canClickIcons = false;
			canClickPrograms = false;
			desktop->bg->alpha -= 0.05;

			if (desktop->bg->alpha <= 0) {
				destroyDesktop();
				createDesktop();
				return;
			}
		}

		if (!desktop->shuttingDown && desktop->bg->alpha < 1) desktop->bg->alpha += 0.05;

		if (desktop->eventsNum > 0) {
			canClickIcons = false;
			canClickPrograms = false;

			DesktopEvent *evt = &desktop->events[desktop->currentEventIndex];

			if (!evt->started) {
				evt->started = true;
				//
				/// Event startup
				//
				if (streq(evt->type, "dialog")) {
					desktop->dialogTf->setText(evt->text);
					desktop->dialogTf->alignInside(DIR8_DOWN, 0, 5);
				} else if (streq(evt->type, "choice")) {
					desktop->dialogTf->setText(evt->text);
					desktop->dialogTf->alignInside(DIR8_DOWN, 0, 5);

					for (int i = 0; i < evt->choicesNum; i++) {
						DesktopChoice *curChoice = &desktop->choices[desktop->choicesNum++];

						{ /// Dialog background
							MintSprite *spr = createMintSprite();
							spr->setupRect(128, 64, 0x222244);
							desktop->bg->addChild(spr);
							spr->y += i * 100;

							curChoice->bg = spr;
						}

						{ /// Dialog text
							MintSprite *spr = createMintSprite();
							spr->setupEmpty(curChoice->bg->getWidth(), curChoice->bg->getHeight());
							curChoice->bg->addChild(spr);
							spr->setText(evt->choices[i]);
							spr->alignInside(DIR8_CENTER);

							curChoice->tf = spr;
						}

					}
				}
			}

			//
			/// Event update
			//
			if (streq(evt->type, "dialog")) {
				if (engine->leftMouseJustReleased) evt->done = true;
			} else if (streq(evt->type, "choice")) {
				for (int i = 0; i < evt->choicesNum; i++) {
					MintSprite *choiceBg = desktop->choices[i].bg;
					choiceBg->alignInside(DIR8_CENTER);

					choiceBg->x += sin((engine->time*(M_PI*0.25) + ((float)(i+1)/(evt->choicesNum))*(M_PI*2))) * 64;
					choiceBg->y += cos((engine->time*(M_PI*0.25) + ((float)(i+1)/(evt->choicesNum))*(M_PI*2))) * 64;

					if (choiceBg->justReleased) {
						evt->chosenIndex = i;
						evt->done = true;
						break;
					}
				}
			}

			//
			/// Event shutdown
			//
			if (evt->done) {
				if (streq(evt->type, "dialog")) {
					desktop->dialogTf->setText("");
				} else if (streq(evt->type, "choice")) {
					Writer::execJs("onEvent({type: \"CHOICE\", chosenIndex: %d, chosenText: \"%s\"});", evt->chosenIndex, desktop->choices[evt->chosenIndex].tf->rawText);

					desktop->dialogTf->setText("");
					for (int i = 0; i < evt->choicesNum; i++) {
						desktop->choices[i].bg->destroy();
						desktop->choicesNum = 0;
					}
				}

				if (desktop->eventsNum <= desktop->currentEventIndex+1) {
					desktop->eventsNum = 0;
					desktop->currentEventIndex = 0;
				} else {
					desktop->currentEventIndex++;
				}
			}
		}

		if (canClickIcons && desktop->sleepButton->justReleased) {
			desktop->shuttingDown = true;
		}

		/// Figure out if dragging needs to happen
		ForEach (DesktopProgram *program, desktop->programs) {
			if (!program->exists) continue;

			{ /// Figure out scrolling while I'm here
				if (canClickPrograms && program->bg->hovering) {
					if (keyIsJustPressed(KEY_UP) || platformMouseWheel < 0) program->scrollAmount += 0.05;
					if (keyIsJustPressed(KEY_DOWN) || platformMouseWheel > 0) program->scrollAmount -= 0.05;
					program->scrollAmount = Clamp(program->scrollAmount, 0, 1);

					float maxScroll = program->bg->height - program->content->getCombinedHeight();

					program->content->y -= (program->content->y - program->scrollAmount*maxScroll)/10;
				}
			}

			if (canClickPrograms && program->exitButton->justPressed) {
				canClickPrograms = false;
				canClickIcons = false;
				exitProgram(program);
			}

			if (canClickPrograms && program->titleBar->justPressed) {
				canClickIcons = false;
				canClickPrograms = false;
				desktop->draggedProgram = program;
			}
		}
		if (desktop->draggedProgram && !desktop->draggedProgram->titleBar->holding) {
			desktop->draggedProgram->bg->alpha = 1;
			desktop->draggedProgram = NULL;
		}

		/// Do actual dragging
		if (desktop->draggedProgram) {
			DesktopProgram *prog = desktop->draggedProgram;
			prog->bg->alpha = 0.5;

			//@cleanup Can this be simplified?
			Point mPoint;
			mPoint.setTo(engine->mouseX, engine->mouseY);
			Matrix inv = prog->titleBar->parentTransform;
			matrixInvert(&inv);
			matrixMultiplyPoint(&inv, &mPoint);
			prog->titleBar->x = mPoint.x - prog->titleBar->holdPivot.x;
			prog->titleBar->y = mPoint.y - prog->titleBar->holdPivot.y;

			canClickIcons = false;
			canClickPrograms = false;
		}

		/// Update the programs
		ForEach (DesktopProgram *program, desktop->programs) {
			if (!program->exists) continue;

			using namespace Writer;

			ForEach (Image **img, program->images) {
				if (!*img) continue;
				MintSprite *spr = (*img)->sprite;
				Rect startRect;
				startRect.x = 0;
				startRect.y = 0;
				startRect.width = program->bg->width;
				startRect.height = program->bg->height;

				program->bg->localToGlobal(&startRect);
				spr->clipRect.setTo(startRect.x, startRect.y, startRect.width, startRect.height);
			}


			if (canClickPrograms && program->bg->hovering) {
				canClickIcons = false;
				ForEach (Image **imgPtr, program->images) {
					Image *img = *imgPtr;
					if (!img) continue;

					if (img->sprite->justReleased) {
						Writer::execJs("onEvent({type: \"IMAGE_CLICK\", imageName: \"%s\"});", img->name);
					}
				}
				if (program->bg->justReleased) {
					// char buf[LARGE_STR];
					// sprintf(buf, "onProgramInteract(\"%s\", \"%s\", \"%s\", \"%s\");", program->programName, "none", "none", "none");
					// Writer::jsInterp->execute(buf);
				}
			}
		}

		/// Update the icons
		ForEach (DesktopIcon *icon, desktop->icons) {
			if (!icon->exists) continue;

			if (canClickIcons && icon->sprite->justReleased) {
				Writer::execJs("onIconClick(\"%s\");", icon->name);
			}
		}
	}

	void addIcon(const char *iconName, const char *iconImg) {
		DesktopIcon *icon = NULL;

		ForEach (DesktopIcon *curIcon, desktop->icons) {
			if (!curIcon->exists) {
				icon = curIcon;
				break;
			}
		}

		if (!icon) {
			Writer::msg("Failed to create icon", Writer::MSG_ERROR);
			return;
		}

		memset(icon, 0, sizeof(DesktopIcon));
		icon->exists = true;
		icon->index = desktop->iconsNum++;
		strcpy(icon->name, iconName);

		{ /// Icon image
			MintSprite *spr = createMintSprite();
			if (streq(iconImg, "none")) spr->setupRect(64, 64, 0xFF0000);
			else spr->setupAnimated(iconImg);
			desktop->bg->addChild(spr);

			icon->sprite = spr;
		}

		{ /// Position icon
			int xIndex = icon->index % DESKTOP_ICONS_MAX_COLS;
			int yIndex = icon->index / DESKTOP_ICONS_MAX_COLS;
			int desktopEdgePad = 40;

			icon->sprite->x = xIndex * (icon->sprite->width * 3) + desktopEdgePad;
			icon->sprite->y = yIndex * (icon->sprite->height * 3) + desktopEdgePad;
		}

		{ /// Icon text
			MintSprite *spr = createMintSprite();
			spr->setupEmpty(128, 64);
			icon->sprite->addChild(spr);
			spr->setText(icon->name);
			spr->alignInside(DIR8_UP);
			spr->y = spr->parent->height + 5;

			icon->tf = spr;
		}
	}

	void removeIcon(const char *iconName) {
		ForEach (DesktopIcon *icon, desktop->icons) {
			if (icon->exists && streq(icon->name, iconName)) {
				removeIcon(icon);
				break;
			}
		}
	}

	void removeIcon(DesktopIcon *icon) {
		icon->exists = false;
		icon->sprite->destroy();
	}

	DesktopProgram *getProgram(const char *programName) {
		ForEach (DesktopProgram *programIter, desktop->programs) {
			if (programIter->exists && streq(programIter->programName, programName)) {
				return programIter;
			}
		}

		return NULL;
	}

	void startProgram(const char *programName, float width, float height) {
		DesktopProgram *program = NULL;

		if (streq(programName, "none")) return;

		ForEach (DesktopProgram *curProgram, desktop->programs) {
			if (curProgram->exists && streq(curProgram->programName, programName)) return;
		}

		ForEach (DesktopProgram *curProgram, desktop->programs) {
			if (!curProgram->exists) {
				program = curProgram;
				break;
			}
		}

		if (!program) {
			Writer::msg("Failed to create program", Writer::MSG_ERROR);
			return;
		}

		memset(program, 0, sizeof(DesktopProgram));
		program->exists = true;
		program->index = desktop->programsNum++;
		strcpy(program->programName, programName);

		{ /// Tile bar
			MintSprite *spr = createMintSprite();
			spr->setupRect(width*engine->width, 20, 0x777777);
			desktop->bg->addChild(spr);
			spr->alignInside(DIR8_UP, 0, 5);
			spr->x += (program->index % 10) * 20;
			spr->y += (program->index % 10) * 20;

			program->titleBar = spr;
		}

		{ /// Program background
			MintSprite *spr = createMintSprite();
			spr->setupRect(program->titleBar->width, height*engine->height, 0x333333);
			program->titleBar->addChild(spr);
			spr->y += program->titleBar->height;

			program->bg = spr;
		}

		{ /// Program content
			MintSprite *spr = createMintSprite();
			spr->setupRect(1, 1, 0x000000);
			program->titleBar->addChild(spr);
			spr->y += program->titleBar->height;

			program->content = spr;
		}

		{ /// Exit button
			MintSprite *spr = createMintSprite();
			spr->setupRect(20, 20, 0x770000);
			program->titleBar->addChild(spr);
			spr->alignInside(DIR8_UP_RIGHT);

			program->exitButton = spr;
		}

		Writer::execJs("onEvent({type: \"PROGRAM_STARTED\", programName: \"%s\"});", program->programName);
	}

	void exitProgram(DesktopProgram *program) {
		using namespace Writer;

		program->exists = false;
		program->titleBar->destroy();

		ForEach (Image **img, program->images) {
			if (!*img) continue;
			removeImage(*img);
		}
	}

	void pushDesktopEvent(DesktopEvent *newEvent) {
		using namespace Writer;

		if (strlen(newEvent->type) == 0) {
			msg("Need event type", MSG_ERROR);
			return;
		}

		desktop->events[desktop->eventsNum++] = *newEvent;
	}

	void js_addBookmarkBar(CScriptVar *v, void *userdata) {
		const char *programName = v->getParameter("programName")->getString().c_str();
		using namespace Writer;

		DesktopProgram *prog = getProgram(programName);
		if (!prog) {
			msg("Can't add a bookmakr bar on %s because it doesn't exist", MSG_ERROR, programName);
			return;
		}
	}

	void js_pushDesktopEvent(CScriptVar *v, void *userdata) {
		using namespace Writer;

		CScriptVarLink *eventTypeVar = v->getParameter("event")->findChild("type");
		CScriptVarLink *textVar = v->getParameter("event")->findChild("text");
		CScriptVarLink *choicesVar = v->getParameter("event")->findChild("choices");

		DesktopEvent evt = {};
		if (eventTypeVar) strcpy(evt.type, eventTypeVar->var->getString().c_str());
		if (textVar) strcpy(evt.text, textVar->var->getString().c_str());
		pushDesktopEvent(&evt);
	}

	void js_pushChoiceEvent(CScriptVar *v, void *userdata) {
		using namespace Writer;
		const char *text = v->getParameter("text")->getString().c_str();

		CScriptVar *choicesArr = v->getParameter("choices");

		DesktopEvent evt = {};
		strcpy(evt.type, "choice");
		strcpy(evt.text, text);
		evt.choicesNum = choicesArr->getArrayLength();
		for (int i = 0; i < evt.choicesNum; i++) {
			strcpy(evt.choices[i], choicesArr->getArrayIndex(i)->getString().c_str());
		}

		pushDesktopEvent(&evt);
	}
}
