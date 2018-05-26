// THIS FILE IS A TEST RE-WRITE OF THE GAME, NOTHING IN HERE IS IN USE CURRENTLY

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

	/// Graph
};

void switchState(GameState state);
void changeState();
void updateState();

duk_ret_t append(duk_context *ctx);

Game *game = NULL;

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
	addJsFunction("append", append, 1);

	game = (Game *)zalloc(sizeof(Game));

	///
	//@incomplete Setup mod repo
	///

	char *initCode = (char *)getAsset("info/newInterConfig.js")->data;
	runJs(initCode);

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

duk_ret_t append(duk_context *ctx) {
	const char *str = duk_get_string(ctx, -1);
	strcat(game->mainTextStr, str);

	return 0;
}
