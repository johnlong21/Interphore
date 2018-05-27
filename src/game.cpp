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
	char *name;
	char *appendData;
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

	runJs(realData->get());
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
	char *data = stringClone(str);
	// printf("Passage is: %s\n", data);

	char delim[3];
	if (strstr(data, "\r\n")) strcpy(delim, "\r\n");
	else strcpy(delim, "\n\0");

	const char *lineStart = data;

	Passage *passage = (Passage *)zalloc(sizeof(Passage));
	int appendDataMax = 0;
	bool firstLine = true;
	for (int i = 0;; i++) {
		const char *lineEnd = strstr(lineStart, delim);
		if (!lineEnd) break;

		char *line;
		line = (char *)zalloc(lineEnd - lineStart + 1);
		strncpy(line, lineStart, lineEnd-lineStart);

		if (firstLine) {
			if (strlen(line) == 0) {
				lineStart = lineEnd+1;
				continue;
			}
			firstLine = false;
			if (line[0] != ':') {
				msg("Titles must start with a colon (%s, %d)", line, line[0]);
				Free(data);
				Free(passage);
				return 0;
			}

			passage->name = stringClone(&line[1]);
		} else if (line[0] == '[') {
			char *barPos = strstr(line, "|");
			char *text;
			char *dest;

			if (barPos) {
				char *textStart = &line[1];
				char *textEnd = barPos;
				char *destStart = textEnd+1;
				char *destEnd = line+strlen(line)-1;
				int textLen = textEnd - textStart;
				int destLen = destEnd - destStart;
				text = (char *)zalloc(textLen+1);
				dest = (char *)zalloc(destLen+1);
				strncpy(text, textStart, textLen);
				strncpy(dest, destStart, destLen);
			} else {
				int textLen = strlen(line)-2;
				text = (char *)zalloc(textLen+1);
				dest = (char *)zalloc(textLen+1);
				strncpy(text, line+1, textLen);
				strcpy(dest, text);
			}

			char *newLine = (char *)Malloc(strlen(text) + strlen(dest) + 32);
			sprintf(newLine, "`addChoice(\"%s\", \"%s\");`\n", text, dest);
			Free(text);
			Free(dest);

			appendDataMax += strlen(newLine) + 1;
			char *newAppendData = (char *)Malloc(appendDataMax);
			newAppendData[0] = '\0';
			if (passage->appendData) {
				strcpy(newAppendData, passage->appendData);
				Free(passage->appendData);
			}
			strcat(newAppendData, newLine);
			passage->appendData = newAppendData;
			Free(newLine);
		} else {
			appendDataMax += strlen(line) + 2;
			char *newAppendData = (char *)Malloc(appendDataMax);
			newAppendData[0] = '\0';
			if (passage->appendData) {
				strcpy(newAppendData, passage->appendData);
				Free(passage->appendData);
			}
			strcat(newAppendData, line);
			strcat(newAppendData, "\n");
			passage->appendData = newAppendData;
		}

		Free(line);
		lineStart = lineEnd+1;
	}
	// printf("-----\nPassage name: %s\nData:\n%s\n", passage->name, passage->appendData);

	if (game->passagesNum+1 > PASSAGE_MAX) {
		msg("Too many passages");
		Free(data);
		Free(passage);
		return 0;
	}

	bool addNew = true;

	for (int i = 0; i < game->passagesNum; i++) {
		if (streq(game->passages[i]->name, passage->name)) {
			Free(game->passages[i]);
			game->passages[i] = passage;
			addNew = false;
		}
	}

	if (addNew) game->passages[game->passagesNum++] = passage;

	Free(data);
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
	const char *str = duk_get_string(ctx, -1);
	char *passageName = stringClone(str);
	// printf("Going to %s\n", passageName);

	// if (game->state == STATE_MENU) changeState(STATE_MOD);

	// game->scrollAmount = 0;
	// game->passageStartTime = engine->time;
	// game->mainText->alpha = 0;

	// printf("Passages %d\n", game->passagesNum);
	for (int i = 0; i < game->passagesNum; i++) {
		Passage *passage = game->passages[i];
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

				int lineLen = lineEnd - lineStart;
				char *line = (char *)zalloc(lineLen + 1);
				strncpy(line, lineStart, lineLen);

				char *appendLine = (char *)zalloc(strlen(line) + 128); //@incomplte New lines might overflow this
				strcpy(appendLine, "append(\"");
				for (int lineI = 0; lineI < lineLen; lineI++) {
					if (line[lineI] == '\n') {
						strcat(appendLine, "\\n");
					} else {
						appendLine[strlen(appendLine)] = line[lineI];
					}
				}
				strcat(appendLine, "\");");
				runJs(appendLine);
				Free(appendLine);

				if (needToken) {
					bool printResult = true;
					if (lineStart != lineEnd) {
						if (*(lineEnd-1) == '!') {
							printResult = false;
							game->mainTextStr[strlen(game->mainTextStr-1)] = '\0';
						}
					}

					lineStart = lineEnd+1;
					lineEnd = strstr(lineStart, "`");
					if (!lineEnd) assert(0); // @incomplete need other backtick error

					char *codeLine = (char *)Malloc(lineEnd-lineStart);
					strncpy(codeLine, lineStart, lineEnd-lineStart);
					codeLine[lineEnd-lineStart] = '\0';
					// printf("Gonna eval: %s\n", codeLine);

					if (printResult) {
						char *evalLine = (char *)Malloc(strlen(codeLine) + 128);
						strcpy(evalLine, "append(");
						if (codeLine[strlen(codeLine)-1] == ';') codeLine[strlen(codeLine)-1] = '\0';
						strcat(evalLine, codeLine);
						strcat(evalLine, ");");
						printf("Gonna really eval: %s\n", evalLine);
						runJs(evalLine);
					} else {
						runJs(codeLine);
					}

					Free(codeLine);
				}

				printf("Would append: %s\n", line);
				Free(line);
				lineStart = lineEnd+1;
			}
			Free(passageName);
			return 0;
		}
	}

	msg("Failed to find passage %s\n", passageName);
	Free(passageName);
	return 0;
}

duk_ret_t addImage(duk_context *ctx) {
	const char *assetId = duk_get_string(ctx, -1);

	int slot;
	for (slot = 0; slot < IMAGES_MAX; slot++) if (!game->images[slot]) break;

	if (slot >= IMAGES_MAX) {
		//@incomplete Too many images error
	}

	MintSprite *spr = createMintSprite(assetId);
	game->images[slot] = spr;

	duk_push_int(ctx, slot);
	return 1;
}
