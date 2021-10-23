#include "renderer.h"
#include "engine.h"
RendererData *renderer;

void copyFlashPixels(unsigned char *data, int destIndex, int w, int h, int dx, int dy, bool bleed);
void drawFlashPixels(unsigned char *data, int destIndex, int w, int h, int dx, int dy, float scaleX, float scaleY, int tint);
void copyFlashPixelsByIndex(int srcIndex, int destIndex, int w, int h, int dx, int dy, bool bleed);
void drawFlashPixelsByIndex(int srcIndex, int destIndex, int w, int h, int dx, int dy, float scaleX, float scaleY, int tint);

void initRenderer(void *rendererMemory) {
	printf("Flash renderer initing\n");

	inline_as3(
		"import flash.display.*;"
		"import flash.display.Bitmap;\n"
		"Console.tempData = new BitmapData(%0, %1, true, 0);"
		"Console.tempBmp = new Bitmap(Console.tempData);"
		"Console.bytes.endian = \"littleEndian\";"
		:: "r"(TEXTURE_WIDTH_LIMIT), "r"(TEXTURE_HEIGHT_LIMIT)
	);
}

void rendererStartFrame() {
}

void renderSprites(MintSprite **sprites, int spritesNum, Matrix *scaleMatrix, Matrix *scrollMatrix) {
	for (int i = 0; i < spritesNum; i++) {
		MintSprite *spr = sprites[i];
		Matrix transform;
		matrixIdentity(&transform);

		if (spr->zooms) matrixMultiply(&transform, scaleMatrix);
		if (spr->scrolls) matrixMultiply(&transform, scrollMatrix);

		matrixMultiply(&transform, &spr->transform);
		matrixMultiply(&transform, &spr->offsetTransform);

		if (spr->old_tint != spr->tint) {
			inline_as3(
				"Console.buildColorTransform(%0);"
				"Console.bitmaps[%1].transform.colorTransform = Console.colorTrans;"
				:: "r"(spr->tint), "r"(spr->bitmapIndex)
			);
			spr->old_tint = spr->tint;
		}

		if (!spr->clipRect.isZero()) {
			inline_as3(
				"import flash.display.*;"
				"if (Console.masks[%0] == null) {"
				"    Console.masks[%0] = new Sprite();"
				"    Console.masks[%0].graphics.beginFill(0xFF0000, 1);"
				"    Console.masks[%0].graphics.drawRect(0, 0, %3, %4);"
				"    Console.stageRef.addChild(Console.masks[%0]);"
				"    Console.bitmaps[%0].mask = Console.masks[%0];"
				"}"
				""
				"Console.masks[%0].x = %1;"
				"Console.masks[%0].y = %2;"
				"if (%3 != Console.masks[%0].width || %4 != Console.masks[%0].height) {"
				"    Console.masks[%0].width = %3;"
				"    Console.masks[%0].height = %4;"
				"}"
				:: "r"(spr->bitmapIndex), "r"(spr->clipRect.x), "r"(spr->clipRect.y), "r"(spr->clipRect.width), "r"(spr->clipRect.height)
			);
		} else {
			inline_as3(
				"if (Console.masks[%0] != null) {"
				"    Console.stageRef.removeChild(Console.masks[%0]);"
				"    Console.masks[%0] = null;"
				"}"
				:: "r"(spr->bitmapIndex)
			);
		}

		inline_as3(
			"var alpha = %7;"
			"var smoothing = %8;"
			"var visible = %9;"
			"var s = Console.bitmaps[%0];"
			"var m = s.transform.matrix;"
			"m.setTo(%1, %2, %3, %4, %5, %6);"
			"s.transform.matrix = m;"
			"s.alpha = alpha;"
			"s.visible = visible;"
			"s.smoothing = smoothing;"
			::
			"r"(spr->bitmapIndex),
			"r"(transform.data[0]), "r"(transform.data[1]), "r"(transform.data[3]), "r"(transform.data[4]), "r"(transform.data[6]), "r"(transform.data[7]),
			"r"(spr->multipliedAlpha), "r"(spr->smoothing), "r"(spr->visible)
		);

		inline_as3(
			"if (Console.stageRef.contains(Console.bitmaps[%0])) Console.stageRef.setChildIndex(Console.bitmaps[%0], Console.stageRef.numChildren-1);"
			::
			"r"(spr->bitmapIndex)
		);

		inline_as3("Console.stageRef.setChildIndex(Console.topContainer, Console.stageRef.numChildren-1);");
	}
}

void initEmptySprite(MintSprite *spr) {
	int width = spr->width;
	int height = spr->height;
	Assert(width > 0);
	Assert(height > 0);

	inline_as3(
		"import flash.display.*;"
		"var bmp = new Bitmap(new BitmapData(%1, %2, true, 0));"
		"Console.stageRef.addChild(bmp);"
		""
		"var slot:int = -1;"
		"    for (var i:int = 0; i < Console.bitmaps.length; i++) {"
		"    if (Console.bitmaps[i] == null) {"
		"        Console.bitmaps[i] = bmp;"
		"        Console.masks[i] = null;"
		"        slot = i;"
		"        break;"
		"    }"
		"}"
		"if (slot == -1) {"
		"    Console.bitmaps.push(bmp);"
		"    Console.masks.push(null);"
		"    slot = Console.bitmaps.length-1;"
		"}"
		"%0 = slot;"
		: "=r"(spr->bitmapIndex) : "r"(width), "r"(height)
	);

	// inline_as3("%0 = Console.bitmaps.length-1;" : "=r"(spr->bitmapIndex) :);
}

void initAnimatedSprite(MintSprite *spr) {
	initEmptySprite(spr);
}

void deinitSprite(MintSprite *spr) {
	inline_as3(
		"import flash.display.*;"
		"var s = Console.bitmaps[%0];"
		"if (s != null && Console.stageRef.contains(s)) {"
		"    Console.stageRef.removeChild(s);"
		"    s.bitmapData.dispose();"
		// "} else {"
		// "    trace(\"Bad remove: \"+s);"
		// "    trace(\"Bad remove width: \"+s.width);"
		"}"
		""
		"if (Console.masks[%0] != null) {"
		"    Console.stageRef.removeChild(Console.masks[%0]);"
		"    Console.masks[%0] = null;"
		"}"
		:: "r"(spr->bitmapIndex)
	);
}

void switchSpriteFrame(MintSprite *spr) {
	spr->clear();

	Rect *r = &spr->frames[spr->currentFrame].data;
	Rect *off = &spr->frames[spr->currentFrame].offsetData;

	RenderProps props = {};
	props.x = r->x;
	props.y = r->y;
	props.width = r->width;
	props.height = r->height;
	props.dx = off->x;
	props.dy = off->y;
	props.tint = 0;
	props.scaleX = 1;
	props.scaleY = 1;
	props.bleed = 1;

	renderTextureToSprite(spr->assetId, spr, &props);
}

void generateTexture(void *data, Asset *asset, bool argb) {
	disableSound();
	bool unMultiplyAlpha = true;
	if (strstr(asset->name, "modPath") || engine->disablePremultipliedAlpha) unMultiplyAlpha = false;

	for (int y = 0; y < asset->height; y++) {
		for (int x = 0; x < asset->width; x++) {
			unsigned char a, r, b, g;

			if (argb) {
				a = ((unsigned char *)data)[(y*asset->width+x)*4 + 3];
				r = ((unsigned char *)data)[(y*asset->width+x)*4 + 2];
				g = ((unsigned char *)data)[(y*asset->width+x)*4 + 1];
				b = ((unsigned char *)data)[(y*asset->width+x)*4 + 0];
			} else {
				a = ((unsigned char *)data)[(y*asset->width+x)*4 + 3];
				r = ((unsigned char *)data)[(y*asset->width+x)*4 + 0];
				g = ((unsigned char *)data)[(y*asset->width+x)*4 + 1];
				b = ((unsigned char *)data)[(y*asset->width+x)*4 + 2];
			}

			if (unMultiplyAlpha) {
				((unsigned char *)data)[(y*asset->width+x)*4 + 3] = a;
				((unsigned char *)data)[(y*asset->width+x)*4 + 2] = r/(a/255.0);
				((unsigned char *)data)[(y*asset->width+x)*4 + 1] = g/(a/255.0);
				((unsigned char *)data)[(y*asset->width+x)*4 + 0] = b/(a/255.0);
			} else {
				((unsigned char *)data)[(y*asset->width+x)*4 + 3] = a;
				((unsigned char *)data)[(y*asset->width+x)*4 + 2] = r;
				((unsigned char *)data)[(y*asset->width+x)*4 + 1] = g;
				((unsigned char *)data)[(y*asset->width+x)*4 + 0] = b;
			}
		}
	}

	asset->data = data;
	enableSound();
}

void drawTilesOnSprite(MintSprite *spr, const char *assetId, int tileWidth, int tileHeight, int tilesWide, int tilesHigh, int *tiles) {
	Asset *tilesetTextureAsset = getTextureAsset(assetId);

	inline_as3(
		"var imgPtr = %0;"
		"var imgSize = %1;"
		"var bitmapIndex = %2;"
		"var imageWidth = %3;"
		"var imageHeight = %4;"
		"var tileWidth = %5;"
		"var tileHeight = %6;"
		"var tilesWide = %7;"
		"var tilesHigh = %8;"
		"var tilesPtr = %9;"
		"var s = Console.bitmaps[bitmapIndex];"
		""
		"Console.rect.setTo(0, 0, imageWidth, imageHeight);"
		"CModule.ram.position = imgPtr;"
		"Console.tempData.setPixels(Console.rect, CModule.ram);"
		""
		"var imageWidthInTiles:int = imageWidth/tileWidth;"
		"var imageHeightInTiles:int = imageHeight/tileHeight;"
		""
		"s.bitmapData.lock();"
		"Console.buildColorTransform(0);"
		""
		"CModule.ram.position = tilesPtr;"
		"for (var i:int = 0; i < tilesWide*tilesHigh; i++) {"
		"var tile:int = CModule.ram.readInt();"
		"if (tile == 0) continue;"
		"tile--;"
		""
		"var visualTileX:int = (tile %% imageWidthInTiles);"
		"var visualTileY:int = Math.floor(tile / imageWidthInTiles);"
		""
		"var realX:int = (i %% tilesWide);"
		"var realY:int = Math.floor(i / tilesWide);"
		""
		"visualTileX *= tileWidth;"
		"visualTileY *= tileHeight;"
		"realX *= tileWidth;"
		"realY *= tileHeight;"
		""
		"Console.rect.setTo(visualTileX, visualTileY, tileWidth, tileHeight);"
		"Console.point.setTo(realX, realY);"
		"s.bitmapData.copyPixels(Console.tempData, Console.rect, Console.point, null, null, true);"
		// "Console.mat.identity();"
		// "Console.mat.translate(realX - visualTileX, realY - visualTileY);"
		// ""
		// "Console.rect.setTo(realX, realY, tileWidth, tileHeight);"
		// "s.bitmapData.draw(Console.tempData, Console.mat, Console.colorTrans, null, Console.rect, false);"
		"}"
		"s.bitmapData.unlock();"
		:: "r"(tilesetTextureAsset->data), "r"(tilesetTextureAsset->dataLen), "r"(spr->bitmapIndex), "r"(tilesetTextureAsset->width), "r"(tilesetTextureAsset->height), "r"(tileWidth), "r"(tileHeight), "r"(tilesWide), "r"(tilesHigh), "r"(tiles)
		);
}

void drawParticles(MintParticleSystem *system) {
}

void renderSpriteToSprite(MintSprite *source, MintSprite *dest, RenderProps *props) {
	if (props->scaleX == 1 && props->scaleY == 1 && props->tint == 0) {
		printf("Gonna copy pixels\n");
		copyFlashPixelsByIndex(source->bitmapIndex, dest->bitmapIndex, props->width, props->height, props->dx, props->dy, props->bleed);
	} else {
		drawFlashPixelsByIndex(source->bitmapIndex, dest->bitmapIndex, props->width, props->height, props->dx, props->dy, props->scaleX, props->scaleY, props->tint);
	}
}

void renderTextureToSprite(const char *assetId, MintSprite *spr, RenderProps *props) {
	renderTextureToSprite(getTextureAsset(assetId), spr, props);
}
void renderTextureToSprite(Asset *tex, MintSprite *spr, RenderProps *props) {
	int tempSize = props->width * props->height * 4;

	if (tempSize > renderer->tempAnimMemSize) {
		if (renderer->tempAnimMem) Free(renderer->tempAnimMem);
		renderer->tempAnimMem = Malloc(tempSize);
	}

	unsigned char *data = (unsigned char *)renderer->tempAnimMem;

	for (int i = 0; i < props->height; i++) {
		int sourceIndex = (i+props->y)*tex->width + props->x;
		memcpy(&data[i*props->width*4], &tex->data[sourceIndex*4 + 0], props->width*4);
	}

	if (props->scaleX == 1 && props->scaleY == 1 && props->tint == 0) {
		copyFlashPixels(data, spr->bitmapIndex, props->width, props->height, props->dx, props->dy, props->bleed);
	} else {
		drawFlashPixels(data, spr->bitmapIndex, props->width, props->height, props->dx, props->dy, props->scaleX, props->scaleY, props->tint);
	}
}

void copyFlashPixels(unsigned char *data, int destIndex, int w, int h, int dx, int dy, bool bleed) {
	int dataSize = w*h*4;

	inline_as3(
		"import flash.display.*;"
		"var dataPtr = %0;"
		"var dataSize = %1;"
		"var w = %2;"
		"var h = %3;"
		"var dx = %4;"
		"var dy = %5;"
		"var bitmapIndex = %6;"
		"var bleed = %7;"
		""
		"Console.rect.setTo(0, 0, w, h);"
		"CModule.ram.position = dataPtr;"
		"Console.tempData.setPixels(Console.rect, CModule.ram);"
		""
		"var s = Console.bitmaps[bitmapIndex];"
		"Console.rect.setTo(0, 0, w, h);"
		"Console.point.setTo(dx, dy);"
		"s.bitmapData.copyPixels(Console.tempData, Console.rect, Console.point, null, null, !bleed);"
		:: "r"(data), "r"(dataSize), "r"(w), "r"(h), "r"(dx), "r"(dy), "r"(destIndex), "r"(bleed)
		);
}

void copyFlashPixelsByIndex(int srcIndex, int destIndex, int w, int h, int dx, int dy, bool bleed) {
	// printf("Copying from %d to %d, %d %d %d %d (%d)\n", srcIndex, destIndex, w, h, dx, dy, bleed);
	inline_as3(
		"import flash.display.*;"
		"var srcIndex = %0;"
		"var destIndex = %1;"
		"var w = %2;"
		"var h = %3;"
		"var dx = %4;"
		"var dy = %5;"
		"var bleed = %6;"
		""
		"Console.rect.setTo(0, 0, w, h);"
		""
		"var s = Console.bitmaps[srcIndex];"
		"var d = Console.bitmaps[destIndex];"
		"Console.rect.setTo(0, 0, w, h);"
		"Console.point.setTo(dx, dy);"
		"d.bitmapData.copyPixels(s.bitmapData, Console.rect, Console.point, null, null, !bleed);"
		:: "r"(srcIndex), "r"(destIndex), "r"(w), "r"(h), "r"(dx), "r"(dy), "r"(bleed)
		);
}

void drawFlashPixels(unsigned char *data, int destIndex, int w, int h, int dx, int dy, float scaleX, float scaleY, int tint) {
	int dataSize = w*h*4;

	inline_as3(
		"import flash.display.*;"
		"var dataPtr = %0;"
		"var dataSize = %1;"
		"var w = %2;"
		"var h = %3;"
		"var dx = %4;"
		"var dy = %5;"
		"var scaleX = %6;"
		"var scaleY = %7;"
		"var tint = %8;"
		"var bitmapIndex = %9;"
		""
		"Console.rect.setTo(0, 0, w, h);"
		"CModule.ram.position = dataPtr;"
		"Console.tempData.setPixels(Console.rect, CModule.ram);"
		""
		"var s = Console.bitmaps[bitmapIndex];"
		"Console.mat.identity();"
		"Console.mat.scale(scaleX, scaleY);"
		"Console.mat.translate(dx, dy);"
		""
		"Console.buildColorTransform(tint);"
		""
		"Console.rect.setTo(dx, dy, w*scaleX, h*scaleY);"
		"s.bitmapData.draw(Console.tempData, Console.mat, Console.colorTrans, null, Console.rect, false);"
		:: "r"(data), "r"(dataSize), "r"(w), "r"(h), "r"(dx), "r"(dy), "r"(scaleX), "r"(scaleY), "r"(tint), "r"(destIndex)
		);
}

void drawFlashPixelsByIndex(int srcIndex, int destIndex, int w, int h, int dx, int dy, float scaleX, float scaleY, int tint) {
	inline_as3(
		"import flash.display.*;"
		"var srcIndex = %0;"
		"var destIndex = %1;"
		"var w = %2;"
		"var h = %3;"
		"var dx = %4;"
		"var dy = %5;"
		"var scaleX = %6;"
		"var scaleY = %7;"
		"var tint = %8;"
		""
		"Console.rect.setTo(0, 0, w, h);"
		""
		"var s = Console.bitmaps[srcIndex];"
		"var d = Console.bitmaps[destIndex];"
		"Console.mat.identity();"
		"Console.mat.scale(scaleX, scaleY);"
		"Console.mat.translate(dx, dy);"
		""
		"Console.buildColorTransform(tint);"
		""
		"Console.rect.setTo(dx, dy, w*scaleX, h*scaleY);"
		"d.bitmapData.draw(s.bitmapData, Console.mat, Console.colorTrans, null, Console.rect, false);"
		:: "r"(srcIndex), "r"(destIndex), "r"(w), "r"(h), "r"(dx), "r"(dy), "r"(scaleX), "r"(scaleY), "r"(tint)
		);
}

void fillSpritePixels(MintSprite *spr, int x, int y, int width, int height, int argb) {
	inline_as3(
		"var bitmapIndex = %0;"
		"var x = %1;"
		"var y = %2;"
		"var width = %3;"
		"var height = %4;"
		"var argb = %5;"
		"var s = Console.bitmaps[bitmapIndex];"
		"Console.rect.setTo(0, 0, width, height);"
		"Console.tempData.fillRect(Console.rect, argb);"
		"Console.point.setTo(x, y);"
		"s.bitmapData.copyPixels(Console.tempData, Console.rect, Console.point, null, null, true);"
		:: "r"(spr->bitmapIndex), "r"(x), "r"(y), "r"(width), "r"(height), "r"(argb)
	);
}

void clearSprite(MintSprite *spr) {
	inline_as3("Console.bitmaps[%0].bitmapData.fillRect(Console.bitmaps[%0].bitmapData.rect, 0);" :: "r"(spr->bitmapIndex));
}

void destroyTexture(Asset *asset) {
	Free(asset->data);
}
