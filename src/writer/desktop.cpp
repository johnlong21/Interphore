namespace WriterDesktop {
	void js_addDesktopIcon(CScriptVar *v, void *userdata);
	void js_installDesktopExtentions(CScriptVar *v, void *userdata);
	void js_createDesktop(CScriptVar *v, void *userdata);

	struct DesktopStruct {
		MintSprite *bg;
	};

	DesktopStruct *desktop;
	Writer::WriterStruct *writer;

	void js_installDesktopExtentions(CScriptVar *v, void *userdata) {

		desktop = (DesktopStruct *)zalloc(sizeof(DesktopStruct));
		writer = Writer::writer;
		Writer::jsInterp->addNative("function addDesktopIcon(iconText, iconImg, window)", &js_addDesktopIcon, 0);
		Writer::jsInterp->addNative("function createDesktop()", &js_createDesktop, 0);
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
		printf("Would of created %s %s %s\n", iconText, iconImg, window);
	}
}
