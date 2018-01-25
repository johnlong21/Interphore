#define DESKTOP_ICONS_MAX 16
#define DESKTOP_ICONS_MAX_COLS 5
#define DESKTOP_PROGRAMS_MAX 4
#define DESKTOP_PROGRAM_IMAGES_MAX 16

namespace WriterDesktop {
	bool exists = false;

	struct DesktopProgram;

	void js_addDesktopIcon(CScriptVar *v, void *userdata);
	void js_installDesktopExtentions(CScriptVar *v, void *userdata);
	void js_createDesktop(CScriptVar *v, void *userdata);
	void js_setImageWindow(CScriptVar *v, void *userdata);
	void js_startProgram(CScriptVar *v, void *userdata);
	void updateDesktop();
	void startProgram(const char *programName);
	void exitProgram(DesktopProgram *program);

	struct DesktopProgram {
		bool exists;
		int index;
		float scrollAmount;
		MintSprite *bg;
		MintSprite *content;
		MintSprite *titleBar;
		MintSprite *exitButton;
		Writer::Image *images[DESKTOP_PROGRAM_IMAGES_MAX];
		char programName[MED_STR];
	};

	struct DesktopIcon {
		bool exists;
		int index;
		MintSprite *sprite;
		MintSprite *tf;
	};

	struct DesktopStruct {
		MintSprite *bg;
		MintSprite *sleepButton;
		DesktopIcon icons[DESKTOP_ICONS_MAX];
		int iconsNum;

		DesktopProgram programs[DESKTOP_PROGRAMS_MAX];
		int programsNum;

		DesktopProgram *draggedProgram;
	};

	DesktopStruct *desktop;
	Writer::WriterStruct *writer;

	void js_installDesktopExtentions(CScriptVar *v, void *userdata) {

		desktop = (DesktopStruct *)zalloc(sizeof(DesktopStruct));
		writer = Writer::writer;
		Writer::jsInterp->addNative("function addDesktopIcon(iconText, iconImg)", &js_addDesktopIcon, 0);
		Writer::jsInterp->addNative("function createDesktop()", &js_createDesktop, 0);
		Writer::jsInterp->addNative("function setImageWindow(imageName, programName)", &js_setImageWindow, 0);
		Writer::jsInterp->addNative("function startProgram(programName)", &js_startProgram, 0);
		Writer::clear();
	}

	void js_createDesktop(CScriptVar *v, void *userdata) {
		exists = true;
		// Writer::destroyButton(writer->exitButton);
		// Writer::destroyButton(writer->refreshButton);

		{ /// Desktop bg
			MintSprite *spr = createMintSprite();
			// spr->setupRect(writer->bg->width, writer->bg->height, 0x222222);
			spr->setupRect(writer->bg->width*0.95, writer->bg->height*0.95, 0x222222);
			writer->bg->addChild(spr);
			spr->gravitate(0.5, 0.5);

			desktop->bg = spr;
		}

		{ /// Sleep button
			MintSprite *spr = createMintSprite();
			spr->setupRect(64, 64, 0x333388);
			writer->bg->addChild(spr);
			spr->gravitate(0.95, 0.95);

			desktop->sleepButton = spr;
		}
	}

	void js_addDesktopIcon(CScriptVar *v, void *userdata) {
		const char *iconText = v->getParameter("iconText")->getString().c_str();
		const char *iconImg = v->getParameter("iconImg")->getString().c_str();

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

		{ /// Icon image
			MintSprite *spr = createMintSprite();
			if (streq(iconImg, "none")) {
				spr->setupRect(64, 64, 0xFF0000);
			} else {
				spr->setupAnimated(iconImg);
			}
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
			spr->setText(iconText);
			spr->gravitate(0.5, 0);
			spr->y = spr->parent->height + 5;

			icon->tf = spr;
		}
	}

	void js_setImageWindow(CScriptVar *v, void *userdata) {
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
		startProgram(v->getParameter("programName")->getString().c_str());
	}

	void updateDesktop() {
		bool canClickIcons = true;
		bool canClickPrograms = true;

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
				startRect.x = program->bg->x;
				startRect.y = program->bg->y;
				startRect.width = program->bg->width;
				startRect.height = program->bg->height;

				program->bg->localToGlobal(&startRect);
				spr->clipRect.setTo(startRect.x, startRect.y, startRect.width, startRect.height);
			}


			if (canClickPrograms && program->bg->hovering) {
				/// Program interaction goes here
				canClickIcons = false;
			}
		}

		/// Update the icons
		ForEach (DesktopIcon *icon, desktop->icons) {
			if (!icon->exists) continue;

			if (canClickIcons && icon->sprite->justReleased) {
				char buf[MED_STR];
				sprintf(buf, "onIconClick(\"%s\");", icon->tf->rawText);
				Writer::jsInterp->execute(buf);
			}
		}
	}

	void startProgram(const char *programName) {
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
			spr->setupRect(512, 20, 0x777777);
			desktop->bg->addChild(spr);
			spr->gravitate(0.5, 0.05);
			spr->x += program->index * 20;
			spr->y += program->index * 20;

			program->titleBar = spr;
		}

		{ /// Program background
			MintSprite *spr = createMintSprite();
			spr->setupRect(program->titleBar->width, 512, 0x333333);
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
			spr->gravitate(1, 0);

			program->exitButton = spr;
		}

		char buf[MED_STR];
		sprintf(buf, "onProgramStart(\"%s\");", program->programName);
		Writer::jsInterp->execute(buf);
	}

	void exitProgram(DesktopProgram *program) {
		program->exists = false;
		program->titleBar->destroy();

		using namespace Writer;
		ForEach (Image **img, program->images) {
			if (!*img) continue;
			removeImage(*img);
		}
	}
}
