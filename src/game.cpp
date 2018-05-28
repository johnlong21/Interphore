// THIS FILE IS A TEST RE-WRITE OF THE GAME, NOTHING IN HERE IS IN USE CURRENTLY

#define PASSAGE_MAX 1024
#define IMAGES_MAX 256

void initGame(MintSprite *bgSpr);
void deinitGame();
void updateGame();

namespace Writer {
	void initWriter(MintSprite *bgSpr) { initGame(bgSpr); }
	void deinitWriter() { deinitGame(); }
	void updateWriter() { updateGame(); }
}

//
//
//         LEGACY END
//
//

enum GameState {
	STATE_NULL=0,
	STATE_PASSAGE,
	STATE_GRAPH
};

struct Passage {
	String *name;
	String *data;
};

struct Game {
	MintSprite *bg;
	GameState state;
	GameState nextState;

	float stateStartTime;
	bool firstOfState;
	bool lastOfState;
	MintSprite *root;

	/// Passage
	char mainTextStr[HUGE_STR];
	MintSprite *mainText;
	Passage *passages[PASSAGE_MAX];
	int passagesNum;
	MintSprite *images[IMAGES_MAX];

	/// Graph
};

void switchState(GameState state);
void changeState();
void updateState();

void runMod(char *serialData);
void msg(const char *str, ...);

duk_ret_t append(duk_context *ctx);
duk_ret_t submitPassage(duk_context *ctx);
duk_ret_t submitImage(duk_context *ctx);
duk_ret_t submitAudio(duk_context *ctx);

duk_ret_t gotoPassage(duk_context *ctx);

duk_ret_t addImage(duk_context *ctx);
duk_ret_t add9SliceImage(duk_context *ctx);
duk_ret_t addEmptyImage(duk_context *ctx);
duk_ret_t setImageProps(duk_context *ctx);
duk_ret_t setImageText(duk_context *ctx);
duk_ret_t getImageWidth(duk_context *ctx);
duk_ret_t getImageHeight(duk_context *ctx);

Game *game = NULL;
char tempBytes[Megabytes(2)];

void initGame(MintSprite *bgSpr) {
	///
	//@incomplete Copy sound tweaks
	///

	getTextureAsset("Espresso-Dolce_22")->level = 3;
	getTextureAsset("Espresso-Dolce_30")->level = 3;
	getTextureAsset("Espresso-Dolce_38")->level = 3;

	if (engine->platform == PLAT_ANDROID) {
		strcpy(engine->spriteData.defaultFont, "OpenSans-Regular_40");
	} else {
		strcpy(engine->spriteData.defaultFont, "OpenSans-Regular_20");
	}

	initJs();
	addJsFunction("submitPassage", submitPassage, 1);
	addJsFunction("submitImage", submitImage, 1);
	addJsFunction("submitAudio", submitAudio, 1);

	addJsFunction("append", append, 1);
	addJsFunction("gotoPassage", gotoPassage, 1);
	addJsFunction("addImage_internal", addImage, 1);
	addJsFunction("add9SliceImage_internal", add9SliceImage, 7);
	addJsFunction("addEmptyImage_internal", addEmptyImage, 2);
	addJsFunction("setImageProps", setImageProps, 8);
	addJsFunction("setImageText_internal", setImageText, 2);
	addJsFunction("getImageWidth", getImageWidth, 1);
	addJsFunction("getImageHeight", getImageHeight, 1);

	game = (Game *)zalloc(sizeof(Game));

	///
	//@incomplete Setup mod repo
	///

	char *initCode = (char *)getAsset("info/newInterConfig.js")->data;
	runJs(initCode);

	char *tempCode = (char *)getAsset("info/temp.js")->data;
	runMod(tempCode);

	switchState(STATE_PASSAGE);
}

void deinitGame() {
	printf("deinited\n");
}

void switchState(GameState state) {
	if (game->nextState) return;

	game->nextState = state;

	if (game->state == STATE_NULL) changeState();
}

void changeState() {
	GameState oldState = game->state;

	game->firstOfState = false;
	game->lastOfState = true;
	updateState();

	if (game->root) game->root->destroy();
	game->root = createMintSprite();
	game->root->setupContainer(engine->width, engine->height);
	game->stateStartTime = engine->time;

	game->firstOfState = true;
	game->lastOfState = false;
	game->state = game->nextState;
	updateState();

	game->firstOfState = false;
	game->lastOfState = false;

	game->nextState = STATE_NULL;
}

void updateGame() {
	updateState();
}

void updateState() {
	runJs("__update();");

	if (game->state == STATE_PASSAGE) {
		if (game->firstOfState) {
		}

		if (game->lastOfState) {
			return;
		}

		if (!game->mainText) {
			game->mainText = createMintSprite();
			game->mainText->setupEmpty(engine->width - 64, 2048);
		}
		game->mainText->x = engine->width/2 - game->mainText->width/2;
		game->mainText->y = 20;
		game->mainText->setText(game->mainTextStr);
	}

	if (game->state == STATE_GRAPH) {
		if (game->firstOfState) {
		}

		if (game->lastOfState) {
			return;
		}
	}
}

void runMod(char *serialData) {
	// printf("Loaded data: %s\n", serialData);
	char *inputData = (char *)zalloc(SERIAL_SIZE);

	String *realData = newString();
	realData->append("var __passage = \"\";\n");
	realData->append("var __image = \"\";\n");
	realData->append("var __audio = \"\";\n");

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

		char *line = tempBytes;
		line[0] = '\0';
		memcpy(line, lineStart, lineEnd-lineStart);
		line[lineEnd-lineStart] = '\0';

		if (strstr(line, "START_IMAGES")) {
			inImages = true;
			realData->append("var __image = \"\";\n");
		} else if (strstr(line, "END_IMAGES")) {
			inImages = false;
			realData->append("submitImage(__image);");
		} else if (strstr(line, "START_AUDIO")) {
			inAudio = true;
			realData->append("__audio = \"\";");
		} else if (strstr(line, "END_AUDIO")) {
			inAudio = false;
			realData->append("submitAudio(__audio);");
		} else if (strstr(line, "START_PASSAGES")) {
			inPassage = true;
			realData->append("__passage = \"\";");
		} else if (strstr(line, "END_PASSAGES")) {
			realData->append("submitPassage(__passage);");
			inPassage = false;
		} else if (strstr(line, "---")) {
			if (inPassage) {
				realData->append("submitPassage(__passage);\n__passage = \"\";");
			} else if (inImages) {
				realData->append("submitImage(__image);\n__image = \"\";");
			} else if (inAudio) {
				realData->append("submitAudio(__audio);\n__audio = \"\";");
			}
		} else if (inPassage) {
			realData->append("__passage += \"");

			if (strstr(line, "\"")) {
				for (int lineIndex = 0; lineIndex < strlen(line); lineIndex++) {
					if (line[lineIndex] == '"') {
						realData->append("\\\"");
					} else {
						char buf[2] = {};
						buf[0] = line[lineIndex];
						realData->append(buf);
					}
				}
			} else {
				realData->append(line);
			}
			realData->append("\\n\";");
		} else if (inImages) {
			realData->append("__image += \"");
			realData->append(line);
			realData->append("\n\";");
		} else if (inAudio) {
			realData->append("__audio += \"");
			realData->append(line);
			realData->append("\n\";");
		} else {
			realData->append(line);
		}
		realData->append("\n");

		lineStart = lineEnd+1;
	}

	// printf("Gonna exec:\n%s\n", realData);
	// exit(1);

	Free(inputData);

	// printf("Final: %s\n", realData->cStr);
	runJs(realData->cStr);
	realData->destroy();
}

void msg(const char *str, ...) {
	char buffer[HUGE_STR];
	va_list argptr;
	va_start(argptr, str);
	vsprintf(buffer, str, argptr);
	va_end(argptr);

	printf("msg: %s\n", buffer);
}

duk_ret_t append(duk_context *ctx) {
	duk_int_t type = duk_get_type(ctx, -1);
	if (type == DUK_TYPE_STRING) {
		const char *str = duk_get_string(ctx, -1);
		strcat(game->mainTextStr, str);
	} else if (type == DUK_TYPE_NUMBER) {
		double num = duk_get_number(ctx, -1);
		char buf[64];
		if (num == round(num)) {
			sprintf(buf, "%.0f", num);
		} else {
			sprintf(buf, "%f", num);
		}
		strcat(game->mainTextStr, buf);
	}

	return 0;
}

duk_ret_t submitPassage(duk_context *ctx) {
	const char *str = duk_get_string(ctx, -1);
	String *data = newString();
	data->set(str);
	// printf("\n\n\nPassage:\n%s\n", data->cStr);

	int colonPos = data->indexOf(":");
	int nameEndPos = data->indexOf("\n", colonPos);
	String *name = data->subStrAbs(colonPos+1, nameEndPos);

	String *jsData = newString();
	int curPos = nameEndPos;
	bool isCode = false;
	bool appendNextCode = true;
	for (;;) {
		int nextPos = data->indexOf("`", curPos);

		if (nextPos == -1) nextPos = data->length-1;

		String *segment = data->subStrAbs(curPos, nextPos);
		// printf("Seg (code: %d): %s\n", isCode, segment->cStr);
		if (!isCode) {
			if (segment->getLastChar() == '!') {
				appendNextCode = false;
				segment->unAppend(1);
			}
			jsData->append("append(\"");

			String *fixedSeg = segment->replace("\n", "\\n");
			segment->destroy();
			segment = fixedSeg;
			//@incompelte Parse addChoice alias ([myTitle|myPass])

			jsData->append(segment->cStr);
			jsData->append("\");");
		} else {
			if (appendNextCode) {
				jsData->append("append(");
				if (segment->getLastChar() == ';') segment->unAppend(1);
				jsData->append(segment->cStr);
				jsData->append(");");
			} else {
				jsData->append(segment->cStr);
			}

			appendNextCode = true;
		}
		jsData->append("\n");
		segment->destroy();

		curPos = nextPos + 1;
		isCode = !isCode;
		if (curPos >= data->length-1) break;
	}

	Passage *passage = (Passage *)zalloc(sizeof(Passage));
	passage->name = name;
	passage->data = jsData;
	// printf("Code: %s\n", jsData->cStr);

	bool addNew = true;
	for (int i = 0; i < game->passagesNum; i++) {
		if (game->passages[i]->name->equals(passage->name)) {
			game->passages[i]->name->destroy();
			game->passages[i]->data->destroy();
			Free(game->passages[i]);
			game->passages[i] = passage;
			addNew = false;
		}
	}

	if (addNew) game->passages[game->passagesNum++] = passage;

	data->destroy();
	// exit(0);
	return 0;
}

duk_ret_t submitImage(duk_context *ctx) {
	const char *str = duk_get_string(ctx, -1);
	printf("Submitted image: %s\n", str);

	return 0;
}

duk_ret_t submitAudio(duk_context *ctx) {
	const char *str = duk_get_string(ctx, -1);
	printf("Submitted audio: %s\n", str);

	return 0;
}

duk_ret_t gotoPassage(duk_context *ctx) {
	const char *passageName = duk_get_string(ctx, -1);
	// printf("Going to %s\n", passageName);

	// if (game->state == STATE_MENU) changeState(STATE_MOD);

	// game->scrollAmount = 0;
	// game->passageStartTime = engine->time;
	// game->mainText->alpha = 0;

	// printf("Passages %d\n", game->passagesNum);
	for (int i = 0; i < game->passagesNum; i++) {
		Passage *passage = game->passages[i];
		// printf("Checking passage %s\n", passage->name);
		if (streq(passage->name->cStr, passageName)) {
			runJs(passage->data->cStr);
			return 0;
		}
	}

	msg("Failed to find passage %s\n", passageName);
	return 0;
}

duk_ret_t addImage(duk_context *ctx) {
	const char *assetId = duk_get_string(ctx, -1);

	int slot;
	for (slot = 0; slot < IMAGES_MAX; slot++) if (!game->images[slot]) break;

	if (slot >= IMAGES_MAX) {
		msg("Too many images");
		duk_push_int(ctx, -1);
		return 1;
	}

	MintSprite *spr = createMintSprite(assetId);
	game->images[slot] = spr;

	duk_push_int(ctx, slot);
	return 1;
}

duk_ret_t add9SliceImage(duk_context *ctx) {
	int y2 = duk_get_number(ctx, -1);
	int x2 = duk_get_number(ctx, -2);
	int y1 = duk_get_number(ctx, -3);
	int x1 = duk_get_number(ctx, -4);
	int height = duk_get_number(ctx, -5);
	int width = duk_get_number(ctx, -6);
	const char *assetId = duk_get_string(ctx, -7);

	int slot;
	for (slot = 0; slot < IMAGES_MAX; slot++) if (!game->images[slot]) break;

	if (slot >= IMAGES_MAX) {
		msg("Too many images");
		duk_push_int(ctx, -1);
		return 1;
	}

	MintSprite *spr = createMintSprite();
	spr->setup9Slice(assetId, width, height, x1, y1, x2, y2);
	game->images[slot] = spr;

	duk_push_int(ctx, slot);
	return 1;
}

duk_ret_t addEmptyImage(duk_context *ctx) {
	int height = duk_get_number(ctx, -1);
	int width = duk_get_number(ctx, -2);

	int slot;
	for (slot = 0; slot < IMAGES_MAX; slot++) if (!game->images[slot]) break;

	if (slot >= IMAGES_MAX) {
		msg("Too many images");
		duk_push_int(ctx, -1);
		return 1;
	}

	MintSprite *spr = createMintSprite();
	spr->setupEmpty(width, height);
	game->images[slot] = spr;

	duk_push_int(ctx, slot);
	return 1;
}

duk_ret_t setImageProps(duk_context *ctx) {
	int tint = duk_get_number(ctx, -1);
	double rotation = duk_get_number(ctx, -2);
	double alpha = duk_get_number(ctx, -3);
	double scaleY = duk_get_number(ctx, -4);
	double scaleX = duk_get_number(ctx, -5);
	double y = duk_get_number(ctx, -6);
	double x = duk_get_number(ctx, -7);
	int id = duk_get_number(ctx, -8);

	MintSprite *img = game->images[id];
	img->x = x;
	img->y = y;
	img->scaleX = scaleX;
	img->scaleY = scaleY;
	img->alpha = alpha;
	img->rotation = rotation;
	img->tint = tint;

	return 0;
}

duk_ret_t setImageText(duk_context *ctx) {
	const char *text = duk_get_string(ctx, -1);
	int id = duk_get_number(ctx, -2);

	MintSprite *img = game->images[id];
	img->setText(text);

	return 0;

}

duk_ret_t getImageWidth(duk_context *ctx) {
	int id = duk_get_number(ctx, -1);

	MintSprite *img = game->images[id];
	duk_push_int(ctx, img->width);

	return 1;
}

duk_ret_t getImageHeight(duk_context *ctx) {
	int id = duk_get_number(ctx, -1);

	MintSprite *img = game->images[id];
	duk_push_int(ctx, img->height);

	return 1;
}
