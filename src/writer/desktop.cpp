namespace WriterDesktop {
	struct DesktopStruct {
	};

	DesktopStruct *desktop;

	void js_addDesktopIcon(CScriptVar *v, void *userdata);

	void js_installDesktopExtentions(CScriptVar *v, void *userdata) {
		printf("Would install\n");

		desktop = (DesktopStruct *)zalloc(sizeof(DesktopStruct));
		Writer::jsInterp->addNative("function addDesktopIcon(iconText, iconImg, window)", &js_addDesktopIcon, 0);
	}

	void js_addDesktopIcon(CScriptVar *v, void *userdata) {
		const char *iconText = v->getParameter("iconText")->getString().c_str();
		const char *iconImg = v->getParameter("iconImg")->getString().c_str();
		const char *window = v->getParameter("window")->getString().c_str();
		printf("Would of created %s %s %s\n", iconText, iconImg, window);
	}
}
