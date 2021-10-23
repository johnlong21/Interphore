#include "engine.h"
#include "js.h"
#include <time.h>

void cleanupEngine();
int qsortLayerIndex(const void *a, const void *b);

void initEngine(void (*initCallbackFn)(), void (*updateCallbackFn)()) {
#ifdef SEMI_VS_LEAK_CHECK
	printf("Windows leak check enabled\n");
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	// _CrtSetBreakAlloc(409);
#endif

	setbuf(stdout, NULL);
	printf("Asset system: %3.fmb\n", sizeof(AssetData) / 1024.0 / 1024.0);
	printf("Renderer system: %3.fmb\n", sizeof(RendererData) / 1024.0 / 1024.0);
	printf("Sprite system: %3.fmb\n", sizeof(MintSpriteData) / 1024.0 / 1024.0);
	printf("Tilemap system: %3.fmb\n", sizeof(Tilemap) / 1024.0 / 1024.0);
	printf("Sound system: %3.fmb\n", sizeof(SoundData) / 1024.0 / 1024.0);
	printf("Engine total: %.3f\n", sizeof(EngineData) / 1024.0 / 1024.0);
	Assert(sizeof(EngineData) < MEMORY_LIMIT);

	engine = (EngineData *)initPlatform(sizeof(EngineData));

	engine->platform = platPlatform;
	if (engine->platform == PLAT_ANDROID) engine->portraitMode = true;

#ifdef SEMI_SCREEN_ORIENTATION
		engine->portraitMode = streq(SEMI_STRINGIFY(SEMI_SCREEN_ORIENTATION), "portrait");
#endif

#ifdef SEMI_BUILD_DATE
		engine->buildDate = atoi(SEMI_STRINGIFY(SEMI_BUILD_DATE));
#endif

	engine->width = platWidth;
	engine->height = platHeight;
	engine->camera.width = engine->width;
	engine->camera.height = engine->height;

	engine->initCallback = initCallbackFn;
	engine->updateCallback = updateCallbackFn;
	engine->elapsed = 1/60.0;

	for (int i = 0; i < KEY_LIMIT; i++) engine->keys[i] = KEY_RELEASED;

	initAssets(&engine->assets);
	initRenderer(&engine->renderer);
	initMintSprites(&engine->spriteData);
	initTilemap(&engine->tilemap);
	initSound();

	srand(time(NULL));
	engine->initCallback();

#ifndef SEMI_FLASH
	atexit(cleanupEngine);
#endif

	while (!platformShouldClose) {
		updateEngine();
	}
}

void updateEngine() {
	unsigned long frameStartTime = platformGetTime();
	engine->realElapsed = frameStartTime - engine->lastTime;
	engine->lastTime = frameStartTime;
	engine->time = frameStartTime / 1000.0;
#ifdef LOG_ENGINE_UPDATE
	printf("%f ms have elapsed since last update\n", engine->realElapsed);
#endif
	// if (engine->frameCount % 10 == 0) {
	// 	printf("Cur mem: ");
	// 	printReadableSize(memUsed);
	// 	printf("\n");
	// }

	platformStartFrame();
	rendererStartFrame();

	// engineShouldClose = platShouldClose();

	updateReplay();
	engine->mouseX = platformMouseX * engine->camera.width / engine->width;
	engine->mouseY = platformMouseY * engine->camera.height / engine->height;
	if (!engine->leftMousePressed && platformMouseLeftDown) engine->leftMouseJustPressed = true;
	if (engine->leftMousePressed && !platformMouseLeftDown) engine->leftMouseJustReleased = true;
	engine->leftMousePressed = platformMouseLeftDown;

	//@cleanup Does this really need to happen?
	engine->mouseX = Clamp(engine->mouseX, 0, engine->width);
	engine->mouseY = Clamp(engine->mouseY, 0, engine->height);

	// 	engineCommonPreUpdate();

	engine->frameCount++;

	// 	tweenUpdateAll(realElapsed);
	// 	soundUpdate(realElapsed);
	engine->updateCallback();

	/// Update sprites
	for (int i = 0; i < SPRITE_LIMIT; i++)
		if (engine->spriteData.sprites[i].exists)
			engine->spriteData.sprites[i].update();

	/// Update particle systems
	for (int i = 0; i < PARTICLE_SYSTEM_LIMIT; i++)
		if (engine->particleSystemData.systems[i].exists)
			engine->particleSystemData.systems[i].update();

	/// Update inputs
	for (int i = 0; i < KEY_LIMIT; i++) {
		if (engine->keys[i] == KEY_JUST_PRESSED) {
			engine->keys[i] = KEY_PRESSED;
			if (jsContext) setJsKeyStatus(i, KEY_PRESSED);
		}
		if (engine->keys[i] == KEY_JUST_RELEASED) {
			engine->keys[i] = KEY_RELEASED;
			if (jsContext) setJsKeyStatus(i, KEY_RELEASED);
		}
		if (engine->keysUpInFrames[i] > 0) {
			engine->keysUpInFrames[i]--;
			if (engine->keysUpInFrames[i] == 0) releaseKey(i);
		}
	}
	engine->leftMouseJustPressed = false;
	engine->leftMouseJustReleased = false;

	/// Order sprites
	MintSprite *layered[SPRITE_LIMIT];
	int layeredNum = 0;

	for (int i = 0; i < SPRITE_LIMIT; i++) {
		if (!engine->spriteData.sprites[i].exists) continue;
		if (engine->spriteData.sprites[i].unrenderable) continue;
		layered[layeredNum++] = &engine->spriteData.sprites[i];
	}

	qsort(layered, layeredNum, sizeof(MintSprite *), qsortLayerIndex);

	engine->activeSprites = layeredNum;

	/// Generate camera matrix
	Matrix scrollMatrix, scaleMatrix;
	matrixIdentity(&scrollMatrix);
	matrixIdentity(&scaleMatrix);

	// Round camera to the nearest 0.5px for smoothness
	engine->camera.x = floor(engine->camera.x * 2.0) / 2.0;
	engine->camera.y = floor(engine->camera.y * 2.0) / 2.0;
	engine->camera.width = floor(engine->camera.width);
	engine->camera.height = floor(engine->camera.height);

	matrixTranslate(&scrollMatrix, -engine->camera.x, -engine->camera.y);
	matrixScale(&scaleMatrix, engine->width/engine->camera.width,  engine->height/engine->camera.height);

	/// Render
	renderSprites(layered, layeredNum, &scaleMatrix, &scrollMatrix);
	updateSound();

	platformEndFrame();

	unsigned long totalFrameTime = platformGetTime() - frameStartTime;
	unsigned long sleepTime = (int)(1.0/60.0*1000.0) - totalFrameTime;
	if (sleepTime > 1000000) sleepTime = 0; // It probably overflowed backwards
	platformSleepMs(sleepTime);
}

bool keyIsPressed(int key) { return engine->keys[key] == KEY_JUST_PRESSED || engine->keys[key] == KEY_PRESSED; }
bool keyIsJustPressed(int key) { return engine->keys[key] == KEY_JUST_PRESSED; }
bool keyIsJustReleased(int key) { return engine->keys[key] == KEY_JUST_RELEASED; }

void pressKey(int key) {
	if (key > KEY_LIMIT) return;
	if (engine->keys[key] == KEY_PRESSED) return;

	engine->keys[key] = KEY_JUST_PRESSED;
	if (jsContext) setJsKeyStatus(key, KEY_JUST_PRESSED);
}

void releaseKey(int key) {
	if (key > KEY_LIMIT) return;
	if (engine->keys[key] == KEY_JUST_PRESSED) {
		engine->keysUpInFrames[key] = 2;
		return;
	}
	if (engine->keys[key] == KEY_RELEASED) return;

	engine->keys[key] = KEY_JUST_RELEASED;
	if (jsContext) setJsKeyStatus(key, KEY_JUST_RELEASED);
}

void drawText(MintSprite *spr, FormatRegion *regions, int regionsNum) {
	CharRenderDef defs[HUGE_STR];
	int defsNum = 0;

	const char *defaultFontName = spr->defaultFont[0] != '\0' ? spr->defaultFont : NULL;
	int textWidth;
	int textHeight;
	const char *text = spr->text;
	int wordWrapWidth = spr->wordWrapWidth;

	getTextRects(text, wordWrapWidth, defaultFontName, regions, regionsNum, defs, &defsNum, &textWidth, &textHeight);

	spr->textWidth = textWidth;
	spr->textHeight = textHeight;

	for (int i = 0; i < defsNum; i++) {
		CharRenderDef *def = &defs[i];
		if (def->glyph != ' ') {
			RenderProps props = {};
			props.x = def->sourceRect.x;
			props.y = def->sourceRect.y;
			props.width = def->sourceRect.width;
			props.height = def->sourceRect.height;
			props.dx = def->destPoint.x;
			props.dy = def->destPoint.y;
			props.scaleX = 1;
			props.scaleY = 1;
			props.bleed = false;
			renderTextureToSprite(def->sourceTextureAsset, spr, &props);
		}
	}
}

void getBuildDate(char *retDate32Char) {
	time_t t = engine->buildDate;
	const char *format = "%c";

	tm *lt = localtime(&t);

	strftime(retDate32Char, 32, format, lt);
}

void cleanupEngine() {
	if (engine->exitCallback) engine->exitCallback();

	cleanupSound();
	cleanupAssets();

	for (int i = 0; i < SPRITE_LIMIT; i++) {
		if (engine->spriteData.sprites[i].exists) {
			engine->spriteData.sprites[i].destroy();
		}
	}

	engine->spriteData.tagMap->destroy();
	if (windowsDiskLoadPath) Free(windowsDiskLoadPath);
	Free(engine);
}

int qsortLayerIndex(const void *a, const void *b) {
	MintSprite *spr1 = *(MintSprite **)a;
	MintSprite *spr2 = *(MintSprite **)b;

	if (spr1->layer < spr2->layer) return -1;
	if (spr1->layer > spr2->layer) return 1;

	if (spr1->index < spr2->index) return -1;
	if (spr1->index > spr2->index) return 1;

	return 0;
}

void engineAssert(bool expr, const char *filename, int lineNum) {
	if (!expr) {
		printf("Assert failed at %s line %d\n", filename, lineNum);
#ifdef SEMI_WIN32
		assert(0);
#endif

#ifdef SEMI_ANDROID
		assert(0);
#endif

		exit(1);
	}
}
