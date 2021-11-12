#include "game.hpp"
#include "profiler.hpp"
#include "debugOverlay.hpp"
#include "textArea.hpp"
#include "unzipper.hpp"
#include "mintSprite.hpp"
#include "asset.hpp"
#include "js.hpp"
#include "stringTools.hpp"
#include "engine.hpp"
#include "memoryTools.hpp"
#include "random.hpp"
#include "newSound.hpp"

#define PASSAGE_MAX 1024
#define IMAGES_MAX 256
#define AUDIOS_MAX 256
#define STREAM_MAX 256
#define ASSETS_MAX 256

#define BUTTON_HEIGHT 128

#define MAIN_TEXT_LAYER 20

#define PROFILE_JS_UPDATE 0
#define PROFILE_GAME_UPDATE 1

struct Passage {
	char *name;
	char *data;
};

struct Game {
	Profiler profiler;
	DebugOverlay debugOverlay;
	TextArea area;

	MintSprite *bg;

	MintSprite *root;

	/// Streaming
	char *streamNames[STREAM_MAX];
	int streamNamesNum;

	char *streamUrls[STREAM_MAX];
	int streamUrlsNum;

	bool isStreaming;
	int curStreamIndex;

	Asset *loadedAssets[ASSETS_MAX];

	char **slowLoadNames;
	void **slowLoadDatas;
	int *slowLoadDataSizes;
	int slowLoadDatasNum;
	int slowLoadIndex;
	Zip slowLoadZip;
	char *slowLoadCodeToRun;


	char mainTextStr[HUGE_STR];
	MintSprite *mainText;
	Passage *passages[PASSAGE_MAX];
	int passagesNum;

	MintSprite *images[IMAGES_MAX];
	Channel *audios[AUDIOS_MAX];
};

void runMod(char *serialData);
void msg(const char *str, ...);
void jsError(const char *message);

duk_ret_t append(duk_context *ctx);
duk_ret_t setMainText(duk_context *ctx);
duk_ret_t addPassage(duk_context *ctx);
duk_ret_t streamAsset(duk_context *ctx);
void assetStreamed(char *serialData, int size);
duk_ret_t execAsset(duk_context *ctx);

duk_ret_t gotoPassage(duk_context *ctx);
duk_ret_t saveGame(duk_context *ctx);
duk_ret_t loadGame(duk_context *ctx);
void gameLoaded(char *data, int size);
duk_ret_t loadMod(duk_context *ctx);
void modLoaded(char *data, int size);

duk_ret_t interTweenEase(duk_context *ctx);
duk_ret_t setFontTag(duk_context *ctx);
duk_ret_t streamEmbeddedTexture(duk_context *ctx);
duk_ret_t interRnd(duk_context *ctx);
duk_ret_t iterTweens(duk_context *ctx);
duk_ret_t getUrl(duk_context *ctx);

duk_ret_t moveTextArea(duk_context *ctx);
duk_ret_t resizeTextArea(duk_context *ctx);
duk_ret_t setTextArea(duk_context *ctx);
duk_ret_t getTextAreaWidth(duk_context *ctx);
duk_ret_t getTextAreaHeight(duk_context *ctx);
duk_ret_t setTextAreaTint(duk_context *ctx);
duk_ret_t setTextAreaZoomTime(duk_context *ctx);
duk_ret_t setTextAreaZoomOut(duk_context *ctx);
duk_ret_t setTextAreaZoomIn(duk_context *ctx);
duk_ret_t setTextAreaJiggle(duk_context *ctx);
duk_ret_t setTextAreaRainbow(duk_context *ctx);
duk_ret_t setTextAreaWave(duk_context *ctx);
duk_ret_t resetTextAreaModes(duk_context *ctx);

/// Images
duk_ret_t addImage(duk_context *ctx);
duk_ret_t addCanvasImage(duk_context *ctx);
duk_ret_t addRectImage(duk_context *ctx);
duk_ret_t add9SliceImage(duk_context *ctx);
duk_ret_t addEmptyImage(duk_context *ctx);
duk_ret_t setImageProps(duk_context *ctx);
duk_ret_t setImageText(duk_context *ctx);
duk_ret_t getImageSize(duk_context *ctx);
duk_ret_t getTextSize(duk_context *ctx);
duk_ret_t getImageFrames(duk_context *ctx);
duk_ret_t getImageProps(duk_context *ctx);
duk_ret_t destroyImage(duk_context *ctx);
duk_ret_t addChild(duk_context *ctx);
duk_ret_t gotoFrameNamed(duk_context *ctx);
duk_ret_t gotoFrameNum(duk_context *ctx);
duk_ret_t copyPixels(duk_context *ctx);
duk_ret_t setImageFont(duk_context *ctx);

duk_ret_t getTextureWidth(duk_context *ctx);
duk_ret_t getTextureHeight(duk_context *ctx);
duk_ret_t setWordWrapWidth(duk_context *ctx);

/// Audio
duk_ret_t addSoundTweakJs(duk_context *ctx);
duk_ret_t playMusic(duk_context *ctx);
duk_ret_t playEffect(duk_context *ctx);
duk_ret_t destroyAudio(duk_context *ctx);
duk_ret_t setAudioFlags(duk_context *ctx);
duk_ret_t setMasterVolume(duk_context *ctx);

#ifdef SEMI_ANDROID
duk_ret_t startTextInput(duk_context *ctx);
duk_ret_t inputtingText(duk_context *ctx);
#endif

Game *game = NULL;
char tempBytes[Megabytes(2)];

void initGame() {
	windowsDiskLoadPath = stringClone("currentMod.phore");

	getTextureAsset("NunitoSans-Light_22")->level = 3;
	getTextureAsset("NunitoSans-Light_30")->level = 3;
	getTextureAsset("NunitoSans-Light_26")->level = 3;

	strcpy(engine->spriteData.defaultFont, "OpenSans-Regular_40");

	initJs();
	jsErrorFn = jsError;

    const duk_function_list_entry global_functions[] = {
        { "addPassage", addPassage, 2 },
        { "streamAsset", streamAsset, 2 },
        { "execAsset", execAsset, 1 },

        { "append_internal", append, 1 },
        { "setMainText", setMainText, 1 },
        { "gotoPassage_internal", gotoPassage, 1 },
        { "tweenEase", interTweenEase, 2 },
        { "setFontTag", setFontTag, 2 },
        { "streamEmbeddedTexture", streamEmbeddedTexture, 1 },
        { "rnd_internal", interRnd, 0 },
        { "iterTweens_internal", iterTweens, 1 },
        { "getUrl_internal", getUrl, 1 },

        { "addImage_internal", addImage, 1 },
        { "addCanvasImage_internal", addCanvasImage, 3 },
        { "addRectImage_internal", addRectImage, 3 },
        { "add9SliceImage_internal", add9SliceImage, 7 },
        { "addEmptyImage_internal", addEmptyImage, 2 },
        { "setImageProps", setImageProps, 14 },
        { "setImageText_internal", setImageText, 2 },
        { "getImageSize", getImageSize, 2 },
        { "getTextSize", getTextSize, 2 },
        { "getImageFrames", getImageFrames, 1 },
        { "getImageProps_internal", getImageProps, 1 },
        { "destroyImage", destroyImage, 1 },
        { "addChild_internal", addChild, 2 },
        { "gotoFrameNamed", gotoFrameNamed, 3 },
        { "gotoFrameNum", gotoFrameNum, 3 },
        { "copyPixels_internal", copyPixels, 7 },
        { "setImageFont", setImageFont, 2 },
        { "getTextureWidth_internal", getTextureWidth, 1 },
        { "getTextureHeight_internal", getTextureHeight, 1 },
        { "setWordWrapWidth_internal", setWordWrapWidth, 2 },

        { "addSoundTweak", addSoundTweakJs, 2 },
        { "playMusic_internal", playMusic, 1 },
        { "playEffect_internal", playEffect, 1 },
        { "destroyAudio", destroyAudio, 1 },
        { "setAudioFlags", setAudioFlags, 3 },
        { "setMasterVolume", setMasterVolume, 1 },

        { "saveGame_internal", saveGame, 1 },
        { "loadGame_internal", loadGame, 1 },
        { "loadMod_internal", loadMod, 0 },

        { "moveTextArea_internal", moveTextArea, 2 },
        { "resizeTextArea_internal", resizeTextArea, 2 },
        { "setTextArea_internal", setTextArea, 1 },
        { "getTextAreaWidth_internal", getTextAreaWidth, 0 },
        { "getTextAreaHeight_internal", getTextAreaHeight, 0 },
        { "setTextAreaTint_internal", setTextAreaTint, 1 },
        { "setTextAreaZoomTime_internal", setTextAreaZoomTime, 1 },
        { "setTextAreaZoomOut_internal", setTextAreaZoomOut, 0 },
        { "setTextAreaZoomIn_internal", setTextAreaZoomIn, 0 },
        { "setTextAreaJiggle_internal", setTextAreaJiggle, 2 },
        { "setTextAreaRainbow_internal", setTextAreaRainbow, 0 },
        { "setTextAreaWave_internal", setTextAreaWave, 3 },
        { "resetTextAreaModes_internal", resetTextAreaModes, 0 },
        { nullptr, nullptr, 0 }
    };
    duk_push_global_object(jsContext);
    duk_put_function_list(jsContext, -1, global_functions);

    addJsVariable(
        "isMobile",
#ifdef SEMI_ANDROID
        true
#else
        false
#endif
    );
#ifdef SEMI_ANDROID
    addJsFunction("startTextInput", startTextInput, 0);
    addJsFunction("inputtingText", inputtingText, 0);
#endif

	game = (Game *)zalloc(sizeof(Game));

	initProfiler(&game->profiler);
	initDebugOverlay(&game->debugOverlay);

	runJs((char *)getAsset("info/passageParser.js")->data);
	runJs((char *)getAsset("info/interConfig.js")->data);

	// runMod((char *)getAsset("info/basic.phore")->data);
	runMod((char *)getAsset("info/main.phore")->data);

#ifdef FAST_SCRATCH
	runJs("gotoPassage(\"scratchModStart\");");
#endif

#ifdef FORCE_RPG
	runJs("gotoPassage(\"scratchModStart\");");
	runJs("gotoPassage(\"rpgTest\");");
#endif

#ifdef TEST_PARTICLES
	runJs("gotoPassage(\"scratchModStart\");");

	MintParticleSystem *system = createMintParticleSystem("/particles.png", 128);
	system->sprite->layer = 9999;
	system->x = 200;
	system->y = 200;
	for (int i = 0; i < system->particlesMax; i++) {
		system->emit();
	}
#endif

	initTextArea(&game->area);
	game->area.resize(512, 512);
	game->area.sprite->layer = 9999;
	// game->area.setFont("NunitoSans-Light_22");

	game->root = createMintSprite();
	game->root->setupContainer(engine->width, engine->height);

// #ifdef SEMI_FLASH
// 	const char *modNamePrefix = "modName=";

// 	char autoRunMod[PATH_LIMIT] = {};

// 	char *curUrl = flashGetUrl();
// 	char *modNamePos = strstr(curUrl, modNamePrefix);
// 	if (modNamePos) {
// 		modNamePos += strlen(modNamePrefix);
// 		strcpy(autoRunMod, modNamePos);
// 	}
// 	Free(curUrl);

// 	printf("cur url is %s\n", curUrl);
// 	printf("auto run mod is %s\n", autoRunMod);
// 	if (autoRunMod[0] != '\0') runMod((char *)getAsset(autoRunMod)->data);
// #endif
}

void deinitGame() {
	for (int i = 0; i < IMAGES_MAX; i++) {
		MintSprite *img = game->images[i];
		if (img) img->destroy();
	}

	for (int i = 0; i < game->passagesNum; i++) {
		Passage *passage = game->passages[i];
		Free(passage->name);
		Free(passage->data);
		Free(passage);
	}

	deinitJs();
	Free(game);

	// stb_leakcheck_dumpmem();
	printf("deinited\n");
}

void updateGame() {
	game->profiler.startProfile(PROFILE_JS_UPDATE);

	char buf[1024];
	sprintf(
		buf,
		"time = %f;"
		"mouseX = %d;"
		"mouseY = %d;"
		"mouseJustDown = %d;"
		"mouseJustUp = %d;"
		"mouseDown = %d;"
		"mouseWheel = %d;"
		"assetStreamsLeft = %d;"
		"__update();",
		engine->time,
		engine->mouseX,
		engine->mouseY,
		engine->leftMouseJustPressed,
		engine->leftMouseJustReleased,
		engine->leftMousePressed,
		platformMouseWheel,
		game->streamUrlsNum - game->curStreamIndex
	);
	runJs(buf);

	game->profiler.endProfile(PROFILE_JS_UPDATE);
	game->profiler.startProfile(PROFILE_GAME_UPDATE);

	if (game->area.exists) game->area.update();

	{ /// Update streaming
		if (!game->isStreaming && game->streamNamesNum > game->curStreamIndex) {
			game->isStreaming = true;
			platformLoadFromUrl(game->streamUrls[game->curStreamIndex], assetStreamed);
		}

		if (game->curStreamIndex == game->streamUrlsNum) {
			game->streamUrlsNum = 0;
			game->streamNamesNum = 0;
			game->curStreamIndex = 0;
		}
	}

	{ /// Check for ended audios
		for (int i = 0; i < AUDIOS_MAX; i++) {
			Channel *channel = game->audios[i];
			if (!channel) continue;
			if (!channel->exists) {
				game->audios[i] = NULL;
				char buf[1024];
				sprintf(buf, "getAudioById(%d).destroy();", i);
				runJs(buf);
			}
		}
	}

	if (!game->mainText) {
		game->mainText = createMintSprite();
		game->mainText->setupEmpty(engine->width - 128, 2048);
		game->mainText->clipRect.setTo(0, 0, engine->width, engine->height - BUTTON_HEIGHT - 16);
		strcpy(game->mainText->defaultFont, "NunitoSans-Light_26");
		game->mainText->tint = 0xFFdff9ff;
	}

	int viewHeight = engine->height - BUTTON_HEIGHT - 16;
	float newY = game->mainText->y;
	int minY = -game->mainText->textHeight + viewHeight;
	int maxY = 20;

	if (game->mainText->textHeight > viewHeight) {
		if (game->mainText->holding) newY = engine->mouseY - game->mainText->holdPivot.y;
		if (platformMouseWheel < 0) newY -= 20;
		if (platformMouseWheel > 0) newY += 20;
		newY = Clamp(newY, minY, maxY);
	}

	game->mainText->x = engine->width/2 - game->mainText->width/2;
	game->mainText->y = newY;
	game->mainText->layer = MAIN_TEXT_LAYER;
	game->mainText->setText(game->mainTextStr);

	game->profiler.endProfile(PROFILE_GAME_UPDATE);

	if (keyIsJustReleased('`')) {
		printf("JS: %0.2f CPP: %0.2f\n", game->profiler.getAverage(PROFILE_JS_UPDATE), game->profiler.getAverage(PROFILE_GAME_UPDATE));
	}

	game->debugOverlay.update();

	if (game->slowLoadDatasNum) {
		char *curAssetName = game->slowLoadNames[game->slowLoadIndex];
		void *curAssetData = game->slowLoadDatas[game->slowLoadIndex];
		int curAssetDataSize = game->slowLoadDataSizes[game->slowLoadIndex];

		char buf[PATH_LIMIT + 256];
		sprintf(
			buf,
			"clear();\n"
			"append(\"Loading mod asset %d/%d\");",
			game->slowLoadIndex,
			game->slowLoadDatasNum
		);
		runJs(buf);

		for (int j = 0; j < engine->assets.assetsNum; j++) {
			Asset *curAsset = &engine->assets.assets[j];
			if (streq(curAsset->name, curAssetName)) {
				// printf("Destroyed %s\n", curAssetName);
				destroyAsset(curAsset);
			}
		}

		addAsset(curAssetName, (char *)curAssetData, curAssetDataSize);

		if (strstr(curAssetName, ".png")) {
			for (int spriteI = 0; spriteI < SPRITE_LIMIT; spriteI++) {
				MintSprite *spr = &engine->spriteData.sprites[spriteI];
				if (!spr->exists) continue;

				if (streq(spr->assetId, curAssetName)) {
					Asset *texAsset = getTextureAsset(spr->assetId);
					spr->assetId = texAsset->name;
					spr->reloadGraphic();
				}
			}
		}

		if (streq(curAssetName, "assets/main.phore")) {
			game->slowLoadCodeToRun = stringClone((char *)curAssetData);
		}

		game->slowLoadIndex++;
		if (game->slowLoadIndex == game->slowLoadDatasNum) {
			for (int i = 0; i < game->slowLoadDatasNum; i++) Free(game->slowLoadNames[i]);
			Free(game->slowLoadNames);
			Free(game->slowLoadDatas);
			Free(game->slowLoadDataSizes);
			game->slowLoadDatasNum = 0;

			closeZip(&game->slowLoadZip);
			if (game->slowLoadCodeToRun) {
				runMod(game->slowLoadCodeToRun);
				Free(game->slowLoadCodeToRun);
				return;
			}
		}
	}
}

void runMod(char *serialData) {
	// printf("About to parse: %s\n", serialData);
	int serialDataLen = strlen(serialData);
	int lineNum = 0;
	for (int i = 0; i < serialDataLen; i++) {
		int c = serialData[i];
		if (c == '\n') lineNum++;
		if ((c < 0 || c > 256) && (c != -30 && c != -128 && c != -108)) {
			char *buf = (char *)Malloc(2048);
			sprintf(buf, "msg(\"Warning: character (ascii %d) on line %d is invalid and is going to be replaced by _\", {smallFont: true, extraTime: 20});", c, lineNum+1);
			runJs(buf);
			Free(buf);

			serialData[i] = '_';
		}
	}

	duk_get_global_string(jsContext, "runMod");
	duk_push_string(jsContext, serialData);
	duk_call(jsContext, 1);

	const char *data = duk_get_string(jsContext, -1);
	char *realData = stringClone(data);
	duk_pop(jsContext);

	// printf("Running: %s\n", realData);
	runJs(realData);
	Free(realData);
}

void msg(const char *str, ...) {
	char buffer[HUGE_STR];
	va_list argptr;
	va_start(argptr, str);
	vsprintf(buffer, str, argptr);
	va_end(argptr);

	printf("msg: %s\n", buffer);

    // Show a js message
    duk_get_global_string(jsContext, "msg");
    duk_push_string(jsContext, buffer);
    duk_call(jsContext, 1);
}

void jsError(const char *message) {
	printf("Got js Error: %s\n", message);

	char *buf = (char *)Malloc(strlen(message) + 1024);
	sprintf(buf, "msg(\"Error: %s\", {smallFont: true, hugeTexture: true, extraTime: 20});", message);
	runJs(buf);
	Free(buf);
}

duk_ret_t append(duk_context *ctx) {
	const char *data = duk_get_string(ctx, -1);
	strcat(game->mainTextStr, data);

	return 0;
}

duk_ret_t addPassage(duk_context *ctx) {
	const char *data = duk_get_string(ctx, -1);
	const char *name = duk_get_string(ctx, -2);

	Passage *passage = (Passage *)zalloc(sizeof(Passage));
	passage->name = stringClone(name);
	passage->data = stringClone(data);
	// printf("Passage |%s| added, contains:\n%s\n", name, data);

	bool addNew = true;
	for (int i = 0; i < game->passagesNum; i++) {
		if (streq(game->passages[i]->name, passage->name)) {
			Free(game->passages[i]->name);
			Free(game->passages[i]->data);
			Free(game->passages[i]);
			game->passages[i] = passage;
			addNew = false;
		}
	}

	if (addNew) game->passages[game->passagesNum++] = passage;

	return 0;
}

duk_ret_t streamAsset(duk_context *ctx) {
	const char *url = duk_get_string(ctx, -1);
	const char *assetName = duk_get_string(ctx, -2);

	game->streamNames[game->streamNamesNum++] = stringClone(assetName);
	game->streamUrls[game->streamUrlsNum++] = stringClone(url);

	return 0;
}

void assetStreamed(char *serialData, int size) {
	const char *name = game->streamNames[game->curStreamIndex];
	const char *url = game->streamUrls[game->curStreamIndex];

	// printf("Asset done streaming: %s\n", name);
	addAsset(name, serialData, platformLoadedStringSize);

	for (int i = 0; i < ASSETS_MAX; i++) {
		if (!game->loadedAssets[i]) {
			game->loadedAssets[i] = getAsset(name);
			break;
		}
	}

	if (stringEndsWith(name, ".phore") || stringEndsWith(name, ".js")) runMod((char *)getAsset(name)->data);

	game->curStreamIndex++;
	game->isStreaming = false;

	Free((void *)name);
	Free((void *)url);
}

duk_ret_t execAsset(duk_context *ctx) {
	const char *assetId = duk_get_string(ctx, -1);

	runMod((char *)getAsset(assetId)->data);

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
		if (streq(passage->name, passageName)) {
			// printf("Running passage |%s|\n", passage->data);
			runJs(passage->data);
			return 0;
		}
	}

	msg("Failed to find passage %s", passageName);
    // Go to the last map as a fallack
    duk_get_global_string(ctx, "gotoLastMap");
    duk_call(ctx, 0);
    // Pop returned undefined from the stack
    duk_pop(ctx);

	return 0;
}

duk_ret_t saveGame(duk_context *ctx) {
	const char *data = duk_get_string(ctx, -1);
	msg("Game saved!");
	platformSaveToDisk(data);

	return 0;
}

duk_ret_t loadGame(duk_context *ctx) {
    // Save the callback
    duk_require_function(ctx, 0);
    duk_push_global_stash(ctx);
    duk_dup(ctx, 0);
    duk_put_prop_string(ctx, -2, "loadGameCallback");

	platformLoadFromDisk(gameLoaded);

	return 0;
}

void gameLoaded(char *data, int size) {
	// printf("Loaded: %s\n", data);

	if (!streq(data, "none") && !streq(data, "(null)")) {
		msg("Game loaded!");

        duk_push_string(jsContext, data);
        // Duplicate the string for use in both checkpointStr and data
        duk_dup(jsContext, -1);
        // Set checkpointStr
        duk_put_global_string(jsContext, "checkpointStr");
        // Set data
        duk_json_decode(jsContext, -1);
        duk_put_global_string(jsContext, "data");

		// printf("Running: %s\n", buf);
	} else {
		msg("No save game found");
	}

    // Call the callback now
    duk_push_global_stash(jsContext);
    duk_push_string(jsContext, "loadGameCallback");
    duk_call_prop(jsContext, -2, 0);
    duk_pop(jsContext);

    duk_push_global_stash(jsContext);
    duk_del_prop_string(jsContext, -1, "loadGameCallback");

	Free(data);
}

duk_ret_t loadMod(duk_context *ctx) {
	platformLoadFromDisk(modLoaded);
	return 0;
}

void modLoaded(char *data, int size) {
	// printf("Loaded: %s\n", data);

	// 

	if (streq(data, "none") || streq(data, "(null)")) {
		msg("No mod found");
		Free(data);
		return;
	}

	msg("Mod loaded!");

	int firstInt = ((int *)data)[0];
	if (firstInt == 0x04034b50) { // Is zip file
		//@cleanup This will eventually overflow the assets
		printf("Is zip file that's %0.2fkb (%d bytes)\n", (float)size/(float)Kilobytes(1), size);

		openZip((unsigned char *)data, size, &game->slowLoadZip);

		game->slowLoadNames = (char **)malloc(sizeof(char *) * game->slowLoadZip.headersNum);
		game->slowLoadDatas = (void **)malloc(sizeof(void *) * game->slowLoadZip.headersNum);
		game->slowLoadDataSizes = (int *)malloc(sizeof(int) * game->slowLoadZip.headersNum);
		game->slowLoadDatasNum = game->slowLoadZip.headersNum;
		game->slowLoadIndex = 0;
		game->slowLoadCodeToRun = NULL;

		for (int i = 0; i < game->slowLoadZip.headersNum; i++) {
			LocalFileHeader *curHeader = &game->slowLoadZip.headers[i];
			if (curHeader->uncompressedSize == 0) continue;

			char realName[PATH_LIMIT] = {};
			strcpy(realName, "assets/");
			strcat(realName, curHeader->fileName);

			game->slowLoadNames[i] = stringClone(realName);

			void *curAssetData = Malloc(curHeader->uncompressedSize+1);
			memcpy(curAssetData, curHeader->uncompressedData, curHeader->uncompressedSize);
			((char *)curAssetData)[curHeader->uncompressedSize] = '\0';

			game->slowLoadDatas[i] = curAssetData;
			game->slowLoadDataSizes[i] = curHeader->uncompressedSize+1;
		}

		Free(data);
		return;
	}

	int modDataSize = sizeof(char) * strlen(data) + 1024;
	char *str = (char *)Malloc(modDataSize);
	memset(str, 0, modDataSize);

	strcat(str, "try {\n");
	strcat(str, data);
	strcat(str, "\n} catch (e) { msg(String(e.stack), {smallFont: true, hugeTexture: true, extraTime: 20}); }\n");

	runMod(str);
	Free(str);

	Free(data);
}


duk_ret_t setMainText(duk_context *ctx) {
	const char *text = duk_get_string(ctx, -1);
	strcpy(game->mainTextStr, text);
	if (game->mainText) game->mainText->y = 20;
	return 0;
}

duk_ret_t interTweenEase(duk_context *ctx) {
	int easeType = duk_get_int(ctx, -1);
	float perc = duk_get_number(ctx, -2);

	duk_push_number(ctx, tweenEase(perc, (Ease)easeType));

	return 1;
}

duk_ret_t setFontTag(duk_context *ctx) {
	const char *fontName = duk_get_string(ctx, -1);
	const char *tag = duk_get_string(ctx, -2);

	engine->spriteData.tagMap[tag] = fontName;

	return 0;
}

duk_ret_t streamEmbeddedTexture(duk_context *ctx) {
	const char *assetId = duk_get_string(ctx, -1);

	const char *textures[1] = {assetId};

	streamTextures(textures, 1, 1);

	return 0;
}

duk_ret_t interRnd(duk_context *ctx) {
	duk_push_number(ctx, rnd());
	return 1;
}

duk_ret_t iterTweens(duk_context *ctx) {
	int n = duk_get_length(ctx, 0);
	// printf("elements: %d\n", n);
	for (int i = 0; i < n; i++) {
		char keyNames[10][20]; // 10 possible keys of max length 20
		int keyNamesNum = 0;

		float startValues[10];
		int startValuesNum = 0;

		float endValues[10];
		int endValuesNum = 0;

		duk_get_prop_index(ctx, 0, i);

		/// Start params
		duk_push_string(ctx, "startParams");
		duk_get_prop(ctx, -2);

		duk_enum(ctx, -1, 0);
		while (duk_next(ctx, -1, true)) {
			const char *paramName = duk_safe_to_string(ctx, -2);
			float value = duk_get_number(ctx, -1);
			strcpy(keyNames[keyNamesNum++], paramName);
			startValues[startValuesNum++] = value;
			duk_pop_2(ctx);
		}
		duk_pop(ctx); // enum

		duk_pop(ctx); // string

		/// End params
		duk_push_string(ctx, "params");
		duk_get_prop(ctx, -2);

		duk_enum(ctx, -1, 0);
		while (duk_next(ctx, -1, true)) {
			const char *paramName = duk_safe_to_string(ctx, -2);
			float value = duk_get_number(ctx, -1);
			for (int keysI = 0; keysI < keyNamesNum; keysI++) {
				if (streq(keyNames[keysI], paramName)) {
					endValues[keysI++] = value;
					break;
				}
			}
			duk_pop_2(ctx);
		}
		duk_pop(ctx); // enum

		duk_pop(ctx); // string

		/// Perc
		duk_push_string(ctx, "elapsed");
		duk_get_prop(ctx, -2);
		float elapsed = duk_get_number(ctx, -1);
		duk_pop(ctx); // string

		duk_push_string(ctx, "elapsed");
		elapsed += engine->elapsed;
		duk_push_number(ctx, elapsed);
		duk_put_prop(ctx, -3);

		duk_push_string(ctx, "totalTime");
		duk_get_prop(ctx, -2);
		float totalTime = duk_get_number(ctx, -1);
		duk_pop(ctx); // string

		float perc = elapsed/totalTime;
		//@todo easing and reverse

		/// Source
		duk_push_string(ctx, "source");
		duk_get_prop(ctx, -2);
		for (int keysI = 0; keysI < keyNamesNum; keysI++) {
			const char *keyName = keyNames[keysI];
			float start = startValues[keysI];
			float end = endValues[keysI];
			float newValue = mathLerp(perc, start, end);

			duk_push_string(ctx, keyName);
			duk_push_number(ctx, newValue);
			duk_put_prop(ctx, -3);
		}
		duk_pop(ctx); // string

		// printf("e/t: %0.1f %0.1f\n", elapsed, totalTime);
		// for (int keysI = 0; keysI < keyNamesNum; keysI++) {
		// 	printf("Got key %s, start: %0.1f end: %0.1f\n", keyNames[keysI], startValues[keysI], endValues[keysI]);
		// }
	}
	return 0;
}

duk_ret_t getUrl(duk_context *ctx) {
	const char *url = duk_get_string(ctx, -1);

	gotoUrl(url);

	return 0;
}

duk_ret_t moveTextArea(duk_context *ctx) {
	float ypos = duk_get_number(ctx, -1);
	float xpos = duk_get_number(ctx, -2);

	game->area.sprite->x = xpos;
	game->area.sprite->y = ypos;

	return 0;
}

duk_ret_t resizeTextArea(duk_context *ctx) {
	float height = duk_get_number(ctx, -1);
	float width = duk_get_number(ctx, -2);

	game->area.resize(width, height);
	game->area.sprite->layer = 9999;

	return 0;
}

duk_ret_t setTextArea(duk_context *ctx) {
	const char *value = duk_get_string(ctx, -1);

	game->area.setText(value);

	return 0;
}

duk_ret_t getTextAreaWidth(duk_context *ctx) {
	duk_push_number(ctx, game->area.textWidth);
	return 1;
}

duk_ret_t getTextAreaHeight(duk_context *ctx) {
	duk_push_number(ctx, game->area.textHeight);
	return 1;
}

duk_ret_t setTextAreaZoomTime(duk_context *ctx) {
	float time = duk_get_number(ctx, -1);

	game->area.zoomTime = time;

	return 0;
}

duk_ret_t setTextAreaZoomOut(duk_context *ctx) {
	game->area.addMode(TextAreaMode::ZoomOut);
	return 0;
}

duk_ret_t setTextAreaZoomIn(duk_context *ctx) {
	game->area.addMode(TextAreaMode::ZoomIn);
	return 0;
}

duk_ret_t setTextAreaJiggle(duk_context *ctx) {
	int jiggleY = duk_get_int(ctx, -1);
	int jiggleX = duk_get_int(ctx, -2);

	game->area.addMode(TextAreaMode::Jiggle);
	game->area.jiggleX = jiggleX;
	game->area.jiggleY = jiggleY;

	return 0;
}

duk_ret_t setTextAreaRainbow(duk_context *ctx) {
	game->area.addMode(TextAreaMode::Rainbow);
	return 0;
}

duk_ret_t setTextAreaWave(duk_context *ctx) {
	int waveSpeed = duk_get_int(ctx, -1);
	int waveY = duk_get_int(ctx, -2);
	int waveX = duk_get_int(ctx, -3);
	game->area.waveX = waveX;
	game->area.waveY = waveY;
	game->area.waveSpeed = waveSpeed;
	game->area.addMode(TextAreaMode::Wave);
	return 0;
}

duk_ret_t setTextAreaTint(duk_context *ctx) { 
	int tint = duk_get_uint(ctx, -1);
	game->area.fontColour = tint;
	return 0;
}

duk_ret_t resetTextAreaModes(duk_context *ctx) {
	game->area.resetModes();
	return 0;
}
//
//
//         IMAGES START
//
//

duk_ret_t addImage(duk_context *ctx) {
	const char *assetId = duk_get_string(ctx, -1);

	int slot;
	for (slot = 0; slot < IMAGES_MAX; slot++) if (!game->images[slot]) break;

	if (slot >= IMAGES_MAX) {
		msg("Too many images");
		duk_push_int(ctx, -1);
		Assert(0);
		return 1;
	}

	MintSprite *spr = createMintSprite(assetId);
	game->images[slot] = spr;

	duk_push_int(ctx, slot);
	return 1;
}

duk_ret_t addCanvasImage(duk_context *ctx) {
	int height = duk_get_number(ctx, -1);
	int width = duk_get_number(ctx, -2);
	const char *assetId = duk_get_string(ctx, -3);

	int slot;
	for (slot = 0; slot < IMAGES_MAX; slot++) if (!game->images[slot]) break;

	if (slot >= IMAGES_MAX) {
		msg("Too many images");
		duk_push_int(ctx, -1);
		Assert(0);
		return 1;
	}

	MintSprite *spr = createMintSprite();
	spr->setupCanvas(assetId, width, height);
	game->images[slot] = spr;

	duk_push_int(ctx, slot);
	return 1;
}

duk_ret_t addRectImage(duk_context *ctx) {
	int colour = duk_get_number(ctx, -1);
	int height = duk_get_number(ctx, -2);
	int width = duk_get_number(ctx, -3);

	int slot;
	for (slot = 0; slot < IMAGES_MAX; slot++) if (!game->images[slot]) break;

	if (slot >= IMAGES_MAX) {
		msg("Too many images");
		duk_push_int(ctx, -1);
		Assert(0);
		return 1;
	}

	MintSprite *spr = createMintSprite();
	spr->setupRect(width, height, colour);
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
		Assert(0);
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
		Assert(0);
		return 1;
	}

	MintSprite *spr = createMintSprite();
	spr->setupEmpty(width, height);
	game->images[slot] = spr;

	duk_push_int(ctx, slot);
	return 1;
}

duk_ret_t setImageProps(duk_context *ctx) {
	int ignoreMouseRect = duk_get_boolean(ctx, -1);
	int centerPivot = duk_get_boolean(ctx, -2);
	int smoothing = duk_get_boolean(ctx, -3);
	int layer = duk_get_int(ctx, -4);
	int tint = duk_get_uint(ctx, -5);
	double rotation = duk_get_number(ctx, -6);
	double alpha = duk_get_number(ctx, -7);
	double skewY = duk_get_number(ctx, -8);
	double skewX = duk_get_number(ctx, -9);
	double scaleY = duk_get_number(ctx, -10);
	double scaleX = duk_get_number(ctx, -11);
	double y = duk_get_number(ctx, -12);
	double x = duk_get_number(ctx, -13);
	int id = duk_get_number(ctx, -14);

	MintSprite *img = game->images[id];
	if (!img) return 0;

	img->x = x;
	img->y = y;
	img->scaleX = scaleX;
	img->scaleY = scaleY;
	img->skewX = skewX;
	img->skewY = skewY;
	img->alpha = alpha;
	img->rotation = rotation;
	img->tint = tint;
	img->layer = layer;
	img->smoothing = smoothing;
	img->centerPivot = centerPivot;
	img->ignoreMouseRect = ignoreMouseRect;

	return 0;
}

duk_ret_t setImageText(duk_context *ctx) {
	const char *text = duk_get_string(ctx, -1);
	int id = duk_get_number(ctx, -2);

	MintSprite *img = game->images[id];
	img->setText(text);

	return 0;
}

duk_ret_t getImageSize(duk_context *ctx) {
	int arrayIndex = duk_get_number(ctx, -1);
	int id = duk_get_number(ctx, -2);
	MintSprite *img = game->images[id];

	char buf[1024];
	sprintf(buf, "images[%d].width = %d;\nimages[%d].height = %d;\n", arrayIndex, img->width, arrayIndex, img->height);
	runJs(buf);

	return 0;
}

duk_ret_t getTextSize(duk_context *ctx) {
	int arrayIndex = duk_get_number(ctx, -1);
	int id = duk_get_number(ctx, -2);
	MintSprite *img = game->images[id];

	char buf[1024];
	sprintf(buf, "images[%d].textWidth = %d;\nimages[%d].textHeight = %d;\n", arrayIndex, img->textWidth, arrayIndex, img->textHeight);
	runJs(buf);

	return 0;
}

duk_ret_t getImageFrames(duk_context *ctx) {
	int id = duk_get_number(ctx, -1);
	MintSprite *img = game->images[id];
	duk_push_int(ctx, img->framesNum);
	return 1;
}

duk_ret_t getImageProps(duk_context *ctx) {
	int id = duk_get_number(ctx, -1);
	MintSprite *img = game->images[id];

	char *data = (char *)duk_push_buffer(ctx, sizeof(char) * 7, false);
	data[0] = img->justPressed;
	data[1] = img->justReleased;
	data[2] = img->pressing;
	data[3] = img->justHovered;
	data[4] = img->justUnHovered;
	data[5] = img->hovering;
	data[6] = img->currentFrame;

	return 1;
}

duk_ret_t destroyImage(duk_context *ctx) {
	int id = duk_get_number(ctx, -1);
	MintSprite *img = game->images[id];
	game->images[id] = NULL;
	img->destroy();

	return 0;
}

duk_ret_t addChild(duk_context *ctx) {
	int otherId = duk_get_number(ctx, -1);
	int id = duk_get_number(ctx, -2);

	MintSprite *img = game->images[id];
	MintSprite *otherImg = game->images[otherId];
	img->addChild(otherImg);

	return 0;
}

duk_ret_t gotoFrameNamed(duk_context *ctx) {
	int andStop = duk_get_int(ctx, -1);
	const char *frameName = duk_get_string(ctx, -2);
	int id = duk_get_number(ctx, -3);

	MintSprite *img = game->images[id];
	img->gotoFrame(frameName);
	if (andStop) img->playing = false;

	return 0;

}

duk_ret_t gotoFrameNum(duk_context *ctx) {
	int andStop = duk_get_int(ctx, -1);
	int frameNum = duk_get_number(ctx, -2);
	int id = duk_get_number(ctx, -3);

	MintSprite *img = game->images[id];
	img->gotoFrame(frameNum);
	if (andStop) img->playing = false;

	return 0;
}

duk_ret_t copyPixels(duk_context *ctx) {
	int dy = duk_get_number(ctx, -1);
	int dx = duk_get_number(ctx, -2);
	int sh = duk_get_number(ctx, -3);
	int sw = duk_get_number(ctx, -4);
	int sy = duk_get_number(ctx, -5);
	int sx = duk_get_number(ctx, -6);
	int id = duk_get_number(ctx, -7);

	MintSprite *img = game->images[id];
	img->copyPixels(sx, sy, sw, sh, dx, dy);

	return 0;
}

duk_ret_t setImageFont(duk_context *ctx) {
	const char *fontName = duk_get_string(ctx, -1);
	int id = duk_get_int(ctx, -2);

	MintSprite *img = game->images[id];
	strcpy(img->defaultFont, fontName);

	return 0;
}

duk_ret_t getTextureWidth(duk_context *ctx) {
	const char *assetId = duk_get_string(ctx, -1);
	Asset *textureAsset = getTextureAsset(assetId);
	duk_push_int(ctx, textureAsset->width);
	return 1;
}

duk_ret_t getTextureHeight(duk_context *ctx) {
	const char *assetId = duk_get_string(ctx, -1);
	Asset *textureAsset = getTextureAsset(assetId);
	duk_push_int(ctx, textureAsset->height);
	return 1;
}

duk_ret_t setWordWrapWidth(duk_context *ctx) {
	int wordWrapWidth = duk_get_int(ctx, -1);
	int id = duk_get_int(ctx, -2);

	MintSprite *img = game->images[id];
	img->wordWrapWidth = wordWrapWidth;
	return 0;
}

//
//
//         IMAGES END
//
//


//
//
//         AUDIO START
//
//

duk_ret_t addSoundTweakJs(duk_context *ctx) {
	float volume = duk_get_number(ctx, -1);
	const char *assetId = duk_get_string(ctx, -2);

	addSoundTweak(assetId, volume);

	return 0;
}

duk_ret_t playMusic(duk_context *ctx) {
	const char *assetId = duk_get_string(ctx, -1);

	int slot;
	for (slot = 0; slot < AUDIOS_MAX; slot++) if (!game->audios[slot]) break;

	if (slot >= AUDIOS_MAX) {
		msg("Too many audios");
		duk_push_int(ctx, -1);
		Assert(0);
		return 1;
	}

	Channel *channel = playMusic(assetId);
	game->audios[slot] = channel;

	duk_push_int(ctx, slot);

	return 1;
}

duk_ret_t playEffect(duk_context *ctx) {
	const char *assetId = duk_get_string(ctx, -1);

	int slot;
	for (slot = 0; slot < AUDIOS_MAX; slot++) if (!game->audios[slot]) break;

	if (slot >= AUDIOS_MAX) {
		msg("Too many audios");
		duk_push_int(ctx, -1);
		Assert(0);
		return 1;
	}

	Channel *channel = playEffect(assetId);
	game->audios[slot] = channel;

	duk_push_int(ctx, slot);

	return 1;
}

duk_ret_t destroyAudio(duk_context *ctx) {
	int id = duk_get_number(ctx, -1);

	if (!game->audios[id]) return 0;

	game->audios[id]->destroy();
	game->audios[id] = NULL;

	return 0;
}

duk_ret_t setAudioFlags(duk_context *ctx) {
	int volume = duk_get_number(ctx, -1);
	int looping = duk_get_boolean(ctx, -2);
	int id = duk_get_number(ctx, -3);

	Channel *channel = game->audios[id];
	if (!channel) {
		// @hack @bug Sounds linger in here sometimes
		// printf("No channel\n");
		return 0;
	}
	channel->looping = looping;
	channel->userVolume = volume;
	return 0;
}

duk_ret_t setMasterVolume(duk_context *ctx) {
	float volume = duk_get_number(ctx, -1);

	engine->soundData.masterVolume = volume;

	return 0;
}

#ifdef SEMI_ANDROID
#include <SDL.h>

extern SDL_Window* sdlWindow;

duk_ret_t startTextInput(duk_context *ctx) {
    SDL_StartTextInput();

    return 0;
}

duk_ret_t inputtingText(duk_context *ctx) {
    duk_push_boolean(ctx, SDL_IsScreenKeyboardShown(sdlWindow));

    return 1;
}
#endif

//
//
//         AUDIO END
//
//
