#define DESKTOP_ICONS_MAX 16
#define DESKTOP_ICONS_MAX_COLS 5

namespace WriterDesktop {
	void js_addDesktopIcon(CScriptVar *v, void *userdata);
	void js_installDesktopExtentions(CScriptVar *v, void *userdata);
	void js_createDesktop(CScriptVar *v, void *userdata);

	struct DesktopIcon {
		bool exists;
		int index;
		MintSprite *sprite;
		MintSprite *tf;
	};

	struct DesktopStruct {
		MintSprite *bg;
		DesktopIcon icons[DESKTOP_ICONS_MAX];
		int iconsNum;
	};

	DesktopStruct *desktop;
	Writer::WriterStruct *writer;

	void js_installDesktopExtentions(CScriptVar *v, void *userdata) {

		desktop = (DesktopStruct *)zalloc(sizeof(DesktopStruct));
		writer = Writer::writer;
		Writer::jsInterp->addNative("function addDesktopIcon(iconText, iconImg, window)", &js_addDesktopIcon, 0);
		Writer::jsInterp->addNative("function createDesktop()", &js_createDesktop, 0);
		Writer::clear();
	}

	void js_createDesktop(CScriptVar *v, void *userdata) {
		{ /// Desktop bg
			MintSprite *spr = createMintSprite();
			spr->setupRect(writer->bg->width*0.95, writer->bg->height*0.95, 0x222222);
			writer->bg->addChild(spr);
			spr->gravitate(0.5, 0.5);

			desktop->bg = spr;
		}
	}

	void js_addDesktopIcon(CScriptVar *v, void *userdata) {
		const char *iconText = v->getParameter("iconText")->getString().c_str();
		const char *iconImg = v->getParameter("iconImg")->getString().c_str();
		const char *window = v->getParameter("window")->getString().c_str();

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
}
