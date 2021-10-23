#include <stdio.h>
#include <time.h>

#define VERSION "0.0.1"
#include "gameEngine.h"

#ifndef STATIC_ASSETS
#include "../bin/assetLib.cpp"
#endif

/// Beta debugging
// #define LOG_ASSET_LOAD
// #define LOG_ENGINE_UPDATE
#define LOG_ASSET_GET

extern "C" void entryPoint();
void init();
void update();

float fpsSum = 0;

MintSprite *keyTestSprite = NULL;
MintSprite *tweenSprite = NULL;
int tilemapSizeLoopNumber = 0;
MintSprite *globalSpr;
MintSprite *spinSpr;

char *blitBuffer1 = NULL;
char *blitBuffer2 = NULL;
char *lastBuffer = NULL;
int blitW;
int blitH;

void testHoverFn(MintSprite *spr);
void testUnHoverFn(MintSprite *spr);
void testReleaseOnceFn(MintSprite *spr);
void tweenComplete();
void blitTimerComplete();
void largeImageTimerComplete();
// void tilemapBenchTimerComplete(Tilemap *tilemap);
// void tilemapSizeTest(Tilemap *tilemap);

#ifdef SEMI_WIN32
INT WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) {
#else
int main(int argc, char **argv) {
	platformArgC = argc;
	platformArgV = argv;
#endif
	// engineArgc = argc;
	// engineArgv = argv;
	srand(time(NULL));
	printf("--\n\n\nGame start.\n");

	entryPoint();

	return 0;
}

extern "C" void entryPoint() {
	printf("Entry point\n");

	initEngine(init, update);
}

void init() {
	printf("Game has been inited.\n");

#if 0
	/// Animated MintSprite test
	MintSprite *sprite = mintSpriteCreate();
	sprite->setupAnimated("cavePlayer");
#endif

#if 0
	/// MintSprite single animation
	MintSprite *sprite = mintSpriteCreate();
	sprite->setupAnimated("cavePlayer");
	sprite->animFrameRate = 4;
	sprite->playAnim("downWalk", false);
#endif

#if 0
	/// MintSprite animation benchmark
	MintSprite *sprite = mintSpriteCreate();

	for (int i = 0; i < 500; i++) {
		MintSprite *sprite = mintSpriteCreate();
		sprite->alpha = rndFloat(0.5, 1);

		sprite->setupAnimated("cavePlayer");
		sprite->animFrameRate = 20;
		sprite->playAnim("downWalk", false);

		sprite->x = rnd()*engineWidth;
		sprite->y = rnd()*engineHeight;

		tween(&sprite->scaleX, sprite->scaleX + rndFloat(1, 5), 1, NULL);
		tween(&sprite->scaleY, sprite->scaleY + rndFloat(1, 5), 1, NULL);
		tween(&sprite->x, sprite->x + rndFloat(-50, 50), 5, NULL);
		tween(&sprite->y, sprite->y + rndFloat(-50, 50), 5, NULL);
		tween(&sprite->rotation, 360, 5, NULL);
	}
#endif

#if 0
	/// Mouse event test
	MintSprite *spr1 = mintSpriteCreate();
	spr1->centerPivot = true;
	spr1->setupRect(50, 50, 0x00FF00);
	spr1->onHoverOnce = testHoverFn;
	spr1->onUnHoverOnce = testUnHoverFn;
	spr1->onReleaseOnce = testReleaseOnceFn;
	spr1->x = 10;
	spr1->y = 10;

	MintSprite *spr2 = mintSpriteCreate();
	spr2->centerPivot = true;
	spr2->setupRect(50, 50, 0x00FF00);
	spr2->onHoverOnce = testHoverFn;
	spr2->onUnHoverOnce = testUnHoverFn;
	spr2->onReleaseOnce = testReleaseOnceFn;
	spr2->x = 100;
	spr2->y = 100;
	spr2->scaleX = 3;

	MintSprite *spr3 = mintSpriteCreate();
	spr3->centerPivot = true;
	spr3->setupRect(50, 50, 0x00FF00);
	spr3->onHoverOnce = testHoverFn;
	spr3->onUnHoverOnce = testUnHoverFn;
	spr3->x = 200;
	spr3->y = 300;
	spr3->scaleX = 2;
	spr3->scaleY = 2;
	tween(&spr3->rotation, 360, 5, NULL);

	MintSprite *spr4 = mintSpriteCreate();
	spr4->setupRect(50, 50, 0x00FF00);
	spr4->onHoverOnce = testHoverFn;
	spr4->onUnHoverOnce = testUnHoverFn;
	spr4->x = 400;
	spr4->y = 300;
	spr4->scaleX = 2;
	spr4->scaleY = 2;
	tween(&spr4->rotation, 360, 5, NULL);
#endif

#if 0
	/// Key test
	keyTestSprite = mintSpriteCreate();
	keyTestSprite->centerPivot = false;
	keyTestSprite->setupAnimated("cavePlayer");
	keyTestSprite->animFrameRate = 4;
	keyTestSprite->playAnim("downWalk", false);
	keyTestSprite->scale(4, 4);
#endif

#if 0
	/// CopyPixels test
	MintSprite *spr = mintSpriteCreate();
	spr->setupCanvas("img/card", 0, 0);
	spr->copyPixels(0, 0, 10, 10, 0, 0, 0x00000000);
	spr->copyPixels(0, 0, 50, 50, 30, 30, 0x00000000);
#endif

#if 0
	/// Draw pixels test
	MintSprite *spr = mintSpriteCreate();
	spr->setupCanvas("img/card", 0, 0);
	spr->drawPixels(0, 0, 10, 10, 0, 0, 0x00000000, 2, 1);
	spr->drawPixels(0, 0, 50, 50, 30, 30, 0x00000000, 1, 2);
#endif

#if 0
	/// Fill pixels test
	MintSprite *spr = mintSpriteCreate();
	spr->setupEmpty(100, 100);
	spr->fillPixels(0, 0, 10, 10, 0xFF0000FF);
	spr->fillPixels(50, 50, 100, 20, 0xFFFF0000);
	spr->fillPixels(0, 0, 100, 100, 0);
#endif

#if 0
	/// Copy tile test
	MintSprite *spr = mintSpriteCreate();
	spr->setupCanvas("img/card", 0, 0);
	spr->copyTile("front", 0, 0, 0);
	spr->copyTile("queen", 10, 10, 0);
#endif

#if 0
	/// Tween onComplete test
	tweenSprite = mintSpriteCreate();
	MintSprite *spr1 = tweenSprite;
	spr1->setupImage("Capsule.png");
	spr1->alpha = 0.5;

	TweenOptions opt1 = {0};
	opt1.onComplete = tweenComplete;
	tween(&spr1->x, 100, 2, &opt1);
#endif

#if 0
	/// Tween ease and type test
	int numSprites = 11;
	for (int i = 0; i < numSprites; i++) {
		MintSprite *spr =  mintSpriteCreate();
		spr->setupImage("Capsule.png");
		spr->x = 200;
		spr->y = 20*i;

		TweenOptions opt = {0};
		opt.type = PINGPONG;

		if (i == 0) opt.ease = LINEAR;
		if (i == 1) opt.ease = QUAD_OUT;
		if (i == 2) opt.ease = CUBIC_OUT;
		if (i == 3) opt.ease = QUART_OUT;
		if (i == 4) opt.ease = QUINT_OUT;
		if (i == 5) opt.ease = SINE_OUT;
		if (i == 6) opt.ease = CIRC_OUT;
		if (i == 7) opt.ease = EXP_OUT;
		if (i == 8) opt.ease = ELASTIC_OUT;
		if (i == 9) opt.ease = BACK_OUT;
		if (i == 10) opt.ease = BOUNCE_OUT;

		tween(&spr->x, spr->x+100, 3, &opt);
		tween(&spr->scaleX, 1.5, 3, &opt);
		tween(&spr->rotation, 20, 3, &opt);
	}
#endif

#if 0
	blitW = 1280;
	blitH = 720;
	blitBuffer1 = (char *)Malloc(blitW*blitH*4*sizeof(char));
	blitBuffer2 = (char *)Malloc(blitW*blitH*4*sizeof(char));
	for (int y = 0; y < blitH; y++) {
		for (int x = 0; x < blitW; x++) {
			blitBuffer1[(y*blitW+x)*4 + 0] = 255;
			blitBuffer1[(y*blitW+x)*4 + 1] = rndInt(0, 255);
			blitBuffer1[(y*blitW+x)*4 + 2] = rndInt(0, 255);
			blitBuffer1[(y*blitW+x)*4 + 3] = rndInt(0, 255);

			blitBuffer2[(y*blitW+x)*4 + 0] = 255;
			blitBuffer2[(y*blitW+x)*4 + 1] = rndInt(0, 255);
			blitBuffer2[(y*blitW+x)*4 + 2] = rndInt(0, 255);
			blitBuffer2[(y*blitW+x)*4 + 3] = rndInt(0, 255);
		}
	}

	MintSprite *spr = mintSpriteCreate();
	spr->setupEmpty(blitW, blitH);

	lastBuffer = blitBuffer2;

	float timerVal = 0;
	TweenOptions opts = {0};
	opts.type = LOOP;
	opts.onComplete = blitTimerComplete;
	tween(&timerVal, 1, 0.001, &opts);
#endif

#if 0
	/// Text parsing test
	MintSprite *text = mintSpriteCreate();
	text->setupText(engineWidth, engineHeight);
	text->textFieldFormat.defaultFontName = stringClone("OpenSans-Regular_22");
	text->textFieldFormat.wordWrapWidth = 100;

	mintSpriteTagMap->set("a", "OpenSans-Italic_22"); // This is actually ok because these string only need to be in scope until the setText call
	mintSpriteTagMap->set("123", "OpenSans-Bold_16");
	mintSpriteTagMap->set("c", "OpenSans-Regular_10");
	text->setText("This <a>is</a> a <123>test</123> of <c>having</c> tags in text");

	MintSprite *spr = mintSpriteCreate();
	spr->setupRect(text->textFieldFormat.wordWrapWidth, text->height, 0xFF0000);
	spr->alpha = 0.2;
	spr->scrolls = false;
	spr->move(text->x, text->y);
#endif

#if 0
	/// Large static image test
	MintSprite *spr = mintSpriteCreate("largeImage2");
#endif

#if 0
	/// Large image test
	TweenOptions opts = {0};
	opts.type = LOOP;
	opts.onComplete = (void *)largeImageTimerComplete;
	tweenTimer(1, &opts);
#endif

#if 0
	/// Tilemap size test
	Tilemap *tilemap = tilemapCreate();
	tilemap->load("emptyMap");
	printf("Size: %d %d\n", tilemap->width, tilemap->height);

	TweenOptions opts = {0};
	opts.type = LOOP;
	opts.onComplete = (void *)tilemapSizeTest;
	opts.onCompleteArg = tilemap;
	printf("Starting timer\n");
	tweenTimer(1/10.0, &opts);
#endif

#if 0
	/// Tilemap test
	Tilemap *tilemap = tilemapCreate();
	tilemap->load("testMap2");
	// tilemap->setTile(0, 5, 5, 2);
#endif

#if 0
	/// Tilemap benchmark
	Tilemap *tilemap = tilemapCreate();
	tilemap->load("testMap3");

	TweenOptions opts = {0};
	opts.type = LOOP;
	opts.onComplete = (void *)tilemapBenchTimerComplete;
	opts.onCompleteArg = tilemap;
	tweenTimer(1/60.0, &opts);
#endif

#if 0
#define CAMERA_TEST
	/// Camera test
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			MintSprite *spr = mintSpriteCreate();
			spr->setupRect(100, 50, 0xFF0000);
			if (j == i) spr->scaleX = -1.5;
			spr->x = j * 200;
			spr->y = i * 200;
		}
	}
#endif

#if 0
	/// Tint test

	const int sprNum = 4;
	MintSprite *sprs[sprNum];
	for (int i = 0; i < sprNum; i++) {
		MintSprite *spr = mintSpriteCreate();
		spr->setupImage("Cub.png");
		spr->x = (spr->width+10) * i;
		sprs[i] = spr;
	}

	sprs[0]->tint = 0xFFFF0000;
	sprs[1]->tint = 0x88FF0000;
	sprs[2]->tint = 0xFFFFFF00;
	sprs[3]->tint = 0xFFFFFFFF;
#endif

#if 0
	/// Map test
	StringMap *gameData = stringMapCreate();
	int a = 255;
	float b = -1.5;
	int c = 0xFF;
	gameData->set("a", a);
	gameData->set("b", b);
	gameData->set("c", c);
	gameData->set("test", (char *)"This is wrongly on the stack");
	a = b = c = 0;
	gameData->get("a", &a);
	gameData->get("b", &b);
	gameData->get("c", &c);
	char *str;
	gameData->get("test", &str);

	printf("%d %0.2f %d %s\n", a, b, c, str);

	char *serialData = gameData->serialize();
	printf("Serial: \n%s\n", serialData);

	a = b = c = 0;
	gameData->destroy();

	gameData = stringMapCreate();
	gameData->deserialize(serialData);

	gameData->get("a", &a);
	gameData->get("b", &b);
	gameData->get("c", &c);
	char *newStr;
	gameData->get("test", &newStr);

	printf("%d %0.2f %d %s\n", a, b, c, newStr);
#endif

#if 0
#define TEXT_CHANGING_TEST
	globalSpr = mintSpriteCreate();
	globalSpr->setupText(400, 100);
#endif

#if 0
	/// engineRenderTextureToSprite test
	MintSprite *spr = mintSpriteCreate();
	spr->setupEmpty(100, 100);


	char *assetId = (char *)assetResolveName("cavePlayer.png", ".png");
	// char *assetId = (char *)assetResolveName("pixelSquare.png", ".png");
	engineCacheTexture(assetId, 3);

	engineRenderTextureToSprite(
		assetId, spr->id,
		// 0, 0, 3, 3,
		0, 0, 14, 24,
		0, 0,
		0,
		1, 1
		);

	spr->scale(10, 10);
#endif

#if 0
#define COPY_PIXEL_SPEED_TEST
	globalSpr = mintSpriteCreate();
	globalSpr->setupCanvas("pixelSquare", engineWidth, engineHeight);
#endif

	// spinSpr = mintSpriteCreate();
	// spinSpr->setupRect(200, 200, 0xBB0000);
	// spinSpr->move(300, 300);
}

// void tilemapSizeTest(Tilemap *tilemap) {
// 	printf("Drawing tile %d\n", tilemapSizeLoopNumber);
// 	tilemap->setTile(0, 5, tilemapSizeLoopNumber, 2);
// 	tilemapSizeLoopNumber++;
// }

// void tilemapBenchTimerComplete(Tilemap *tilemap) {
// 	for (int i = 0; i < 500; i++) {
// 		tilemap->setTile(
// 			0,
// 			rndInt(0, tilemap->tilesWide-1),
// 			rndInt(0, tilemap->tilesHigh-1),
// 			rndInt(0, 40),
// 			true
// 		);
// 	}
// }

void largeImageTimerComplete() {
	// for (int i = 0; i < 3; i++) {
	// 	MintSprite *spr = mintSpriteCreate();

	// 	if (rndInt(0, 1) == 0) spr->setupImage("largeImage1");
	// 	else spr->setupImage("largeImage2");

	// 	spr->x = rndFloat(0, engineWidth-spr->width);
	// 	spr->y = rndFloat(0, engineHeight-spr->height);

	// 	TweenOptions opts = {0};
	// 	opts.onComplete = (void *)mintSpriteDestroy;
	// 	opts.onCompleteArg = spr;
	// 	tween(&spr->alpha, 0, 2, &opts);
	// }
}

void blitTimerComplete() {
// #ifdef FLASH_BUILD
// 	if (lastBuffer == blitBuffer1) lastBuffer = blitBuffer2;
// 	else lastBuffer = blitBuffer1;
// 	inline_as3("Console.setPixels(%0, %1, %2, %3)"::"r"(0), "r"(blitW), "r"(blitH), "r"(lastBuffer));
// #endif
}

void tweenComplete() {
	// tweenSprite->alpha = 1;
}

void testHoverFn(MintSprite *spr) {
	spr->alpha = 0.5;
}

void testUnHoverFn(MintSprite *spr) {
	spr->alpha = 1;
}

void testReleaseOnceFn(MintSprite *spr) {
	spr->rotation = 0;
	// tween(&spr->rotation, 360, 1);
}

MintSprite *spr;
MintSprite *sprites[SPRITE_LIMIT];
int spritesNum = 0;
void update() {
#if 0
	/// MintSprite test
	if (engine->frameCount == 1) {
		spr = createMintSprite();
		spr->setupAnimated("character/bleat");
		// spr->setupImage("Abuse.png");
		spr->x = 50;
		spr->y = 50;
		// spr->centerPivot = false;
	}

	// spr->rotation += 10;
	if (engine->leftMouseJustPressed) spr->x += 5;
	if (keyIsJustPressed('A')) spr->y += 5;
	return;
#endif

#if 0
	/// Tilemap shader test
	if (engine->frameCount == 1) {
		spr = createMintSprite();
		spr->setupEmpty(16*64, 16*64);
	}

	if (keyIsJustPressed('D')) {
		int tileData[64*64];
		for (int i = 0; i < 64*64; i++) tileData[i] = i % (128 * 10);
		spr->drawTiles("env1.png", 16, 16, 64, 64, (int *)tileData);
	}
	return;
#endif

#if 0
	/// CopyPixels test
	if (engine->frameCount == 1) {
		spr = createMintSprite();
		spr->setupCanvas("img/card");
		spr->copyPixels(0, 50, 50, 50, 30, 30);
		spr->copyPixels(0, 100, 50, 50, 10, 20);
		// spr->move(100, 100);
	}
#endif

#if 0
	/// Draw pixels test
	if (engine->frameCount == 1) {
		spr = createMintSprite();
		spr->setupCanvas("img/card");
		spr->drawPixels(0, 0, 10, 10, 0, 0, 2, 1, 0x880000FF);
		spr->drawPixels(0, 0, 50, 50, 30, 30, 1, 2);
	}
#endif

#if 0
	/// MintSprite Image benchmark
	if (engine->frameCount == 1) {
		for (int i = 0; i < 200; i++) {
			MintSprite *sprite = createMintSprite();
			// sprite->scaleX = 5;
			// sprite->scaleY = 5;
			// sprite->alpha = rndFloat(0.25, 0.75);

			int graphicNum = rndInt(0, 5);
			if (graphicNum == 0) sprite->setupImage("Capsule.png");
			if (graphicNum == 1) sprite->setupImage("Cell Phone.png");
			if (graphicNum == 2) sprite->setupImage("Glowberry.png");
			if (graphicNum == 3) sprite->setupImage("Paw Coin.png");
			if (graphicNum == 4) sprite->setupImage("Glowberry Pie.png");
			if (graphicNum == 5) sprite->setupImage("Sweetgrass Leaf.png");

			sprite->x = rnd()*engine->width;
			sprite->y = rnd()*engine->height;

			sprites[spritesNum++] = sprite;
		}
	}

	for (int i = 0; i < spritesNum; i++) {
		sprites[i]->rotation += 5;
	}
#endif

#if 0
	/// Tilemap test
	if (engine->frameCount == 1) {
		engine->tilemap.load("testMap");
		// tilemap->setTile(0, 5, 5, 2);
	}
#endif

#if 0
	/// Sound test
	playSound("metro.ogg");
#endif

#if 0
	/// Text test
	if (engine->frameCount == 1) {
		MintSprite *text = createMintSprite();
		text->setupEmpty(engine->width, engine->height);
		text->wordWrapWidth = 100;
		text->setText("This is a test of a having a lot of text in a field test test test test test test");
	}

	// MintSprite *spr = mintSpriteCreate();
	// spr->setupRect(text->textFieldFormat.wordWrapWidth, text->height, 0xFF0000);
	// spr->alpha = 0.2;
	// spr->scrolls = false;
	// spr->move(text->x, text->y);

	// engineCamera.x = -100;
	// engineCamera.y = -100;
	// engineCamera.width = engineWidth/2;
	// engineCamera.height = engineHeight/2;
#endif

#if 0
	/// Tint test
	if (engine->frameCount == 1) {
		for (int i = 0; i < 4; i++) {
			MintSprite *spr = createMintSprite();
			spr->setupImage("Cub.png");
			spr->x = (spr->width+10) * i;
			sprites[spritesNum++] = spr;
		}

		sprites[0]->tint = 0xFFFF0000;
		sprites[1]->tint = 0x88FF0000;
		sprites[2]->tint = 0xFFFFFF00;
	}

	MintSprite *spr = sprites[3];

	char a, r, g, b;
	hexToArgb(spr->tint, &a, &r, &g, &b);
	r++;
	b--;
	spr->tint = argbToHex(64, r, g, b);
#endif

#if 0
	/// Looping and playing test
	if (engine->frameCount == 1) {
		spr = createMintSprite("character/bleat.png");
		spr->looping = false;
		spr->play("walkDown");
	}
#endif

#if 0
	/// DisplayList test
	
	if (engine->frameCount == 1) {
		MintSprite *spr = createMintSprite();
		spr->setupRect(128, 128, 0xFF0000);
		spr->alpha = 0.5;

		MintSprite *spr2 = createMintSprite();
		spr2->setupRect(16, 16, 0x00FF00);
		spr2->alpha = 0.5;
		spr2->x = 200;

		spr->addChild(spr2);
		spr2->gravitate(1, 1);

		sprites[spritesNum++] = spr;
		sprites[spritesNum++] = spr2;
	}

	// sprites[0]->rotation += 1;
	// sprites[0]->x++;
	// sprites[0]->y++;
	// sprites[0]->scaleX = cos(engine->time);

	// sprites[0]->alpha = sin(engine->time);
#endif

#if 1
		MintSprite *spr = createMintSprite("mushroom.png");
		spr->clipRect.setTo(20, 20, spr->width, spr->height-50);
#endif

// 	spinSpr->rotation += 10;
// 	printf("ms: %.2f\n", elapsed*1000);

// #ifdef TEXT_CHANGING_TEST
// 	if (engineFrameCount % 100 == 0) {
// 		char buffer[64];
// 		sprintf(buffer, "%f %f", rnd(), rnd());
// 		globalSpr->setText(buffer);
// 	}
// #endif

// #ifdef COPY_PIXEL_SPEED_TEST
// 	if (engineGetKeyStatus(32) == JUST_RELEASED) {
// 		for (int y = 0; y < 50; y++) {
// 			for (int x = 0; x < 50; x++) {
// 				globalSpr->copyPixels(0, 0, 9, 9, x*10, y*10);
// 			}
// 		}
// 	}
// #endif

// #ifdef CAMERA_TEST
// 	if (engineGetKeyStatus(74)) engineCamera.x--;
// 	if (engineGetKeyStatus(75)) engineCamera.x++;
// #endif

// 	if (keyTestSprite != NULL) keyTestSprite->alpha = engineGetKeyStatus(65) ? 0.5 : 1;

// 	mintSpriteUpdateAll(elapsed);
}
