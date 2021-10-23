#include "mintSprite.h"
#include "text.h"
#include "engine.h"

char *textAssetId = (char *)"Text";

int getWidestFrame(MintSprite *spr);
int getTallestFrame(MintSprite *spr);

void initMintSprites(void *mintSpriteMemory) {
	engine->spriteData.tagMap = stringMapCreate();
}

bool textureInUse(Asset *asset) {
	for (int i = 0; i < SPRITE_LIMIT; i++) {
		MintSprite *spr = &engine->spriteData.sprites[i];
		if (!spr->exists) continue;
		if (!spr->assetId) continue;
		if (streq(spr->assetId, asset->name)) return true;
	}

	return false;
}

MintSprite *createMintSprite(const char *assetId, const char *animName, int frameNum) {
	MintSprite *spr = NULL;

	int slot;
	for (slot = 0; slot < SPRITE_LIMIT; slot++)
		if (!engine->spriteData.sprites[slot].exists)
			break;

	spr = &engine->spriteData.sprites[slot];
	memset(spr, 0, sizeof(MintSprite));
	matrixIdentity(&spr->transform);
	matrixIdentity(&spr->offsetTransform);
	matrixIdentity(&spr->parentTransform);
	spr->exists = true;
	spr->active = true;
	spr->visible = true;
	spr->alpha = 1;
	spr->scaleX = 1;
	spr->scaleY = 1;
	// spr->skewX = 1;
	// spr->skewY = 1;
	spr->layer = engine->spriteData.defaultLayer;
	spr->scrolls = true;
	spr->zooms = true;
	spr->looping = true;
	spr->playing = true;
	spr->animFrameRate = 60;
	spr->fontColour = 0xFFFFFF;
	spr->mouseRejectAlpha = -1;
	spr->index = engine->spriteData.maxIndex++;
	spr->creationTime = engine->time;

	if (assetId) spr->setupAnimated(assetId);

	if (animName) {
		spr->play(animName);
		spr->gotoAnimFrame(frameNum);
	}

	return spr;
}

MintSprite *MintSprite::recreate(const char *assetId) {
	float newX = this->x;
	float newY = this->y;
	float newScaleX = this->scaleX;
	float newScaleY = this->scaleY;
	float rotation = this->rotation;
	float alpha = this->alpha;
	int tint = this->tint;
	bool visible = this->visible;
	int layer = this->layer;
	float pivotX = this->pivotX;
	float pivotY = this->pivotY;
	bool centerPivot = this->centerPivot;
	bool trueCenterPivot = this->trueCenterPivot;
	bool scrolls = this->scrolls;
	bool zooms = this->zooms;
	bool smoothing = this->smoothing;
	this->destroy();

	MintSprite *newSprite = createMintSprite(assetId);
	newSprite->x = newX;
	newSprite->y = newY;
	newSprite->scaleX = newScaleX;
	newSprite->scaleY = newScaleY;
	newSprite->rotation = rotation;
	newSprite->alpha = alpha;
	newSprite->tint = tint;
	newSprite->visible = visible;
	newSprite->layer = layer;
	newSprite->pivotX = pivotX;
	newSprite->pivotY = pivotY;
	newSprite->centerPivot = centerPivot;
	newSprite->trueCenterPivot = trueCenterPivot;
	newSprite->scrolls = scrolls;
	newSprite->zooms = zooms;
	newSprite->smoothing = smoothing;
	return newSprite;
}

void MintSprite::setupEmpty(int width, int height) {
	MintSprite *spr = this;
	if (spr->isSetup) return;
	spr->isSetup = true; 
	spr->width = width;
	spr->height = height;
	spr->wordWrapWidth = width;
	initEmptySprite(spr);
}

void MintSprite::setupContainer(int width, int height) {
	MintSprite *spr = this;
	if (spr->isSetup) return;

	spr->width = width;
	spr->height = height;
	spr->unrenderable = true;
	spr->isSetup = true; 
}

void MintSprite::setupRect(int width, int height, int rgb) {
	MintSprite *spr = this;

	spr->setupEmpty(width, height);
	spr->fillPixels(0, 0, width, height, 0xFF000000 | rgb);
}

void MintSprite::setupImage(const char *assetId) {
	MintSprite *spr = this;

	Asset *textureAsset = getTextureAsset(assetId);
	spr->assetId = textureAsset->name;

	char textureAssetNameNoExt[PATH_LIMIT];
	strcpy(textureAssetNameNoExt, textureAsset->name);
	textureAssetNameNoExt[strlen(textureAssetNameNoExt)-4] = '\0';

	Asset *sprFileAsset = getSprFileAsset(textureAsset->name);
	if (sprFileAsset && stringStartsWith(sprFileAsset->name, textureAssetNameNoExt)) parseSprAsset(sprFileAsset, &spr->frames, &spr->framesNum);

	if (spr->framesNum > 0) {
		matrixTranslate(&spr->offsetTransform, spr->frames[0].offsetData.x, spr->frames[0].offsetData.y);
		spr->setupEmpty(spr->frames[0].data.width, spr->frames[0].data.height);
	} else {
		spr->setupEmpty(textureAsset->width, textureAsset->height);
	}

	spr->copyPixels(0, 0, spr->width, spr->height, 0, 0, 0);
}

void MintSprite::setup9Slice(const char *assetId, int width, int height, int x1, int y1, int x2, int y2) {
	MintSprite *spr = this;

	spr->is9Slice = true;

	if (x1 == 0 && y1 == 0 && x1 == 0 && x2 == 0) {
		int pixelsWidth, pixelsHeight;
		unsigned char *pixels = getPngBitmapData(assetId, &pixelsWidth, &pixelsHeight);

		for (int xpos = 0; xpos < pixelsWidth; xpos++) {
			int pixel = GetRgbaPixel(pixels, pixelsWidth, xpos, 0);

			if (pixel == 0xFFFF0000) {
				bool isLine = true;
				for (int ypos = 0; ypos < pixelsHeight; ypos++) {
					if (GetRgbaPixel(pixels, pixelsWidth, xpos, ypos) != 0xFFFF0000) {
						isLine = false;
						break;
					}
				}

				if (isLine) {
					if (x1 == 0) x1 = xpos;
					else x2 = xpos;
				}
			}
		}

		for (int ypos = 0; ypos < pixelsHeight; ypos++) {
			int pixel = GetRgbaPixel(pixels, pixelsWidth, 0, ypos);

			if (pixel == 0xFFFF0000) {
				bool isLine = true;
				for (int xpos = 0; xpos < pixelsHeight; xpos++) {
					if (GetRgbaPixel(pixels, pixelsWidth, xpos, ypos) != 0xFFFF0000) {
						isLine = false;
						break;
					}
				}

				if (isLine) {
					if (y1 == 0) y1 = ypos;
					else y2 = ypos;
				}
			}
		}
	}

	Asset *textureAsset = getTextureAsset(assetId);
	spr->assetId = textureAsset->name;

	spr->setupEmpty(width, height);

	int rows[] = { 0, y1, y2, textureAsset->height };
	int cols[] = { 0, x1, x2, textureAsset->width };
	int dRows[] = { 0, y1, height - (textureAsset->height - y2), height };
	int dCols[] = { 0, x1, width - (textureAsset->width - x2), width };

	Rect origin;
	Rect draw;
	for (int cx = 0; cx < 3; cx++) {
		for (int cy = 0; cy < 3; cy++) {
			origin.setTo(cols[cx], rows[cy], cols[cx + 1] - cols[cx], rows[cy + 1] - rows[cy]);
			draw.setTo(dCols[cx], dRows[cy], dCols[cx + 1] - dCols[cx], dRows[cy + 1] - dRows[cy]);

			spr->drawPixels(origin.x, origin.y, origin.width, origin.height, draw.x, draw.y, draw.width/origin.width, draw.height/origin.height, 0);
		}
	}
}

void MintSprite::reloadGraphic() {
	MintSprite *spr = this;
	if (spr->is9Slice) return; //@todo Make this actually work later

	spr->copyPixels(0, 0, spr->width, spr->height, 0, 0, 0);
}

int getWidestFrame(MintSprite *spr) {
	int widest = 0;
	for (int i = 0; i < spr->framesNum; i++) widest = fmax(widest, (int)spr->frames[i].offsetData.width);
	return widest;
}

int getTallestFrame(MintSprite *spr) {
	int tallest = 0;
	for (int i = 0; i < spr->framesNum; i++) tallest = fmax(tallest, spr->frames[i].offsetData.height);
	return tallest;
}

void MintSprite::setupAnimated(const char *assetId, int width, int height) {
	MintSprite *spr = this;
	// printf("Mallocing frame sounds: %d\n", FRAME_LIMIT);

	Asset *pngAsset = getAsset(assetId, PNG_ASSET);
	spr->assetId = pngAsset->name;

	Asset *textureAsset = getTextureAsset(assetId);

	char textureAssetNameNoExt[PATH_LIMIT];
	strcpy(textureAssetNameNoExt, textureAsset->name);
	textureAssetNameNoExt[strlen(textureAssetNameNoExt)-4] = '\0';

	Asset *sprFileAsset = getSprFileAsset(textureAsset->name);
	if (!sprFileAsset || !stringStartsWith(sprFileAsset->name, textureAssetNameNoExt)) {
		spr->setupImage(assetId);
		return;
	}

	if (spr->frames) {
		Free(spr->frames);
		spr->frames = NULL;
	}
	parseSprAsset(sprFileAsset, &spr->frames, &spr->framesNum);
	if (spr->framesNum > 1) spr->animated = true;
	if (spr->framesNum == 2 && spr->frames[0].data.x == spr->frames[1].data.x) spr->animated = false;

	if (!spr->animated) {
		spr->setupImage(assetId);
		return;
	}

	spr->assetId = textureAsset->name;
	if (width == 0 || height == 0) {
		width = getWidestFrame(spr);
		height = getTallestFrame(spr);
	}
	spr->width = width;
	spr->height = height;

#ifdef SEMI_GL
	spr->textureWidth = textureAsset->width;
	spr->textureHeight = textureAsset->height;
#endif
	initAnimatedSprite(spr);

	spr->currentFrame = -1;
	spr->timeTillNextFrame = 0;

	spr->animationsNum = 0;

	char tempAnimNames[SHORT_STR][ANIMATION_LIMIT];
	int tempAnimNamesNum = 0;
	for (int i = 0; i < spr->framesNum; i++) {
		Frame *frame = &spr->frames[i];
		char curName[SHORT_STR];
		strcpy(curName, frame->name);

		char *splitAt = NULL;
		// if (strstr(curName, "/")) splitAt = strrchr(curName, '/');
		if (strstr(curName, "_")) splitAt = strrchr(curName, '_');
		else {
#if 1 //@checkup Allows anim names to have numbers in them
			int curNameLen = strlen(curName);
			if (
				curNameLen > 4 &&
				!isalpha(curName[curNameLen-1]) &&
				!isalpha(curName[curNameLen-2]) &&
				!isalpha(curName[curNameLen-3]) &&
				!isalpha(curName[curNameLen-4])
			) {
				splitAt = &curName[curNameLen-4];
			}
#else
			for (int j = strlen(curName)-5; j >= 0; j--) {
				if (isalpha(curName[j])) {
					splitAt = &curName[j+1];
					break;
				}
			}
#endif
		}

		if (splitAt != NULL) *splitAt = '\0';

		bool newAnim = true;
		for (int j = 0; j < tempAnimNamesNum; j++)
			if (strcmp(tempAnimNames[j], curName) == 0)
				newAnim = false;

		if (newAnim) strcpy((char *)&tempAnimNames[tempAnimNamesNum++], curName);
	}

	// printf("Anim frames: ");
	spr->animations = (Animation *)Malloc(tempAnimNamesNum*sizeof(Animation));
	memset(spr->animations, 0, tempAnimNamesNum*sizeof(Animation));

	spr->animationsNum = tempAnimNamesNum;
	for (int i = 0; i < tempAnimNamesNum; i++) {
		Animation *anim = &spr->animations[i];
		strcpy(anim->name, tempAnimNames[i]);

		for (int j = 0; j < spr->framesNum; j++) {
			char framePrefix[SHORT_STR];
			strcpy(framePrefix, spr->frames[j].name);

			int prefixLen = strlen(framePrefix);
			if (
				prefixLen > 4 &&
				!isalpha(framePrefix[prefixLen-1]) &&
				!isalpha(framePrefix[prefixLen-2]) &&
				!isalpha(framePrefix[prefixLen-3]) &&
				!isalpha(framePrefix[prefixLen-4])
			) {
				framePrefix[prefixLen-4] = '\0';
			}

			if (streq(framePrefix, anim->name))
				anim->frames[anim->framesNum++] = &spr->frames[j];
		}
	}

	spr->gotoFrame(0);
}

void MintSprite::setupCanvas(const char *assetId, int width, int height) {
	MintSprite *spr = this;
	Asset *textureAsset = getTextureAsset(assetId);
	spr->assetId = textureAsset->name;

	Asset *sprFileAsset = getSprFileAsset(textureAsset->name);
	if (sprFileAsset) {
		if (spr->frames) {
			Free(spr->frames);
			spr->frames = NULL;
		}
		parseSprAsset(sprFileAsset, &spr->frames, &spr->framesNum);
	}

	spr->width = width;
	spr->height = height;

	if (spr->width == 0) {
		Assert(sprFileAsset);
		spr->width = getWidestFrame(spr);
		spr->height = getTallestFrame(spr);
	}

	initEmptySprite(spr);
}

void MintSprite::setupStretchedImage(const char *assetId, int width, int height) {
	MintSprite *spr = this;

	Asset *textureAsset = getTextureAsset(assetId);
	spr->assetId = textureAsset->name;

	Asset *sprFileAsset = getSprFileAsset(textureAsset->name);
	if (sprFileAsset) {
		if (spr->frames) {
			Free(spr->frames);
			spr->frames = NULL;
		}
		parseSprAsset(sprFileAsset, &spr->frames, &spr->framesNum);
	}

	int imageWidth = width;
	int imageHeight = height;

	if (spr->framesNum > 0) {
		matrixTranslate(&spr->offsetTransform, spr->frames[0].offsetData.x, spr->frames[0].offsetData.y);
		imageWidth = spr->frames[0].data.width;
		imageHeight = spr->frames[0].data.height;
	}

	spr->framesNum = 0;

	float ratio = getScaleRatio(imageWidth, imageHeight, width, height);
	spr->setupEmpty(imageWidth*ratio, imageHeight*ratio);
	spr->drawPixels(0, 0, imageWidth, imageHeight, 0, 0, ratio, ratio, 0);
}

void MintSprite::copyPixels(int x, int y, int width, int height, int dx, int dy, bool bleed) {
	MintSprite *spr = this;

	RenderProps props = {};
	props.x = x;
	props.y = y;
	props.width = width;
	props.height = height;
	props.dx = dx;
	props.dy = dy;
	props.tint = 0;
	props.scaleX = 1;
	props.scaleY = 1;
	props.bleed = bleed;

	Asset *texAsset = summonAsset(spr->assetId, TEXTURE_ASSET);
	renderTextureToSprite(texAsset, spr, &props);

	// renderTextureToSprite(spr->assetId, spr, &props);
}

void MintSprite::copyPixelsFromSprite(MintSprite *other, int x, int y, int width, int height, int dx, int dy, bool bleed) {
	MintSprite *spr = this;

	RenderProps props = {};
	props.x = x;
	props.y = y;
	props.width = width;
	props.height = height;
	props.dx = dx;
	props.dy = dy;
	props.tint = 0;
	props.scaleX = 1;
	props.scaleY = 1;
	props.bleed = bleed;

	renderSpriteToSprite(other, spr, &props);
}

void MintSprite::copyPixelsFromAsset(Asset *sourceTextureAsset, int x, int y, int width, int height, int dx, int dy, bool bleed) {
	MintSprite *spr = this;

	RenderProps props = {};
	props.x = x;
	props.y = y;
	props.width = width;
	props.height = height;
	props.dx = dx;
	props.dy = dy;
	props.tint = 0;
	props.scaleX = 1;
	props.scaleY = 1;
	props.bleed = bleed;

	renderTextureToSprite(sourceTextureAsset, spr, &props);
}

void MintSprite::setPixels(Asset *textureAsset, int dx, int dy) {
	MintSprite *spr = this;
	Assert(textureAsset->type == TEXTURE_ASSET);

	RenderProps props = {};
	props.x = 0;
	props.y = 0;
	props.width = textureAsset->width;
	props.height = textureAsset->height;
	props.dx = dx;
	props.dy = dy;
	props.tint = 0;
	props.scaleX = 1;
	props.scaleY = 1;
	props.bleed = false;

	renderTextureToSprite(textureAsset, spr, &props);

	// renderTextureToSprite(spr->assetId, spr, &props);

}

void MintSprite::drawPixels(int x, int y, int width, int height, int dx, int dy, float scaleX, float scaleY, int tint) {
	MintSprite *spr = this;

	RenderProps props = {};
	props.x = x;
	props.y = y;
	props.width = width;
	props.height = height;
	props.dx = dx;
	props.dy = dy;
	props.tint = tint;
	props.scaleX = scaleX;
	props.scaleY = scaleY;
	props.bleed = 0;

	renderTextureToSprite(spr->assetId, spr, &props);
}

void MintSprite::drawPixelsFromSprite(MintSprite *other, int x, int y, int width, int height, int dx, int dy, float scaleX, float scaleY, int tint) {
	MintSprite *spr = this;

	RenderProps props = {};
	props.x = x;
	props.y = y;
	props.width = width;
	props.height = height;
	props.dx = dx;
	props.dy = dy;
	props.tint = tint;
	props.scaleX = scaleX;
	props.scaleY = scaleY;
	props.bleed = 0;

	renderSpriteToSprite(other, spr, &props);
}

void MintSprite::drawPixelsFromAsset(Asset *sourceTextureAsset, int x, int y, int width, int height, int dx, int dy, float scaleX, float scaleY, int tint) {
	MintSprite *spr = this;

	RenderProps props = {};
	props.x = x;
	props.y = y;
	props.width = width;
	props.height = height;
	props.dx = dx;
	props.dy = dy;
	props.tint = tint;
	props.scaleX = scaleX;
	props.scaleY = scaleY;
	props.bleed = 0;

	renderTextureToSprite(sourceTextureAsset, spr, &props);
}

void MintSprite::gotoFrame(const char *frameName) {
	MintSprite *spr = this;

	for (int i = 0; i < spr->framesNum; i++) {
		if (stringStartsWith(spr->frames[i].name, frameName)) {
			spr->gotoFrame(i);
			return;
		}
	}
}

void MintSprite::gotoFrame(int frameNum) {
	MintSprite *spr = this;

	if (spr->currentFrame == frameNum) return;
	// int lastFrameNum = spr->currentFrame;
	spr->currentFrame = frameNum;

	spr->curFrameRect = spr->frames[frameNum].data;
	spr->curOffsetPoint.x = spr->frames[frameNum].offsetData.x;
	spr->curOffsetPoint.y = spr->frames[frameNum].offsetData.y;
	switchSpriteFrame(spr);
}

void MintSprite::gotoAnimFrame(int frameNum) {
	MintSprite *spr = this;
	int animFrame = spr->currentAnim->frames[frameNum]->absFrame;
	spr->gotoFrame(animFrame);
	spr->currentAnimFrame = frameNum;
}

void MintSprite::update() {
	MintSprite *spr = this;
	if (!spr->active || !spr->exists) return;
	if (!zooms) scrolls = false;

	if (!spr->disableMouse && spr->alpha > spr->mouseRejectAlpha) {
		float realWidth = spr->width;
		float realHeight = spr->height;
		if (spr->textWidth) {
			realWidth = spr->textWidth;
			realHeight = spr->textHeight;
		}

#if 0
		Rect rect;

		float exWidth = realWidth*spr->scaleX - realWidth;
		float exHeight = realHeight*spr->scaleY - realHeight;
		if (spr->centerPivot) rect.setTo(spr->mouseRect.x - exWidth/2, spr->mouseRect.y - exHeight/2, realWidth*spr->mouseRect.width, realHeight*spr->mouseRect.height);
		else rect.setTo(spr->mouseRect.x, spr->mouseRect.y, realWidth*spr->mouseRect.width, realHeight*spr->mouseRect.height);

		Point point;
		point.setTo(engine->mouseX, engine->mouseY);
		if (!spr->zooms) {
			point.x *= engine->width/engine->camera.width;
			point.y *= engine->height/engine->camera.height;
		}

		if (spr->centerPivot) point.rotate(spr->mouseRect.x + exWidth/2, spr->mouseRect.y + exHeight/2, spr->rotation);
		else point.rotate(spr->mouseRect.x, spr->mouseRect.y, spr->rotation);

		if (spr->parent) {
			Point tl, tr, bl, br;
			tl.setTo(rect.x, rect.y);
			tr.setTo(rect.x+rect.width, rect.y);
			bl.setTo(rect.x, rect.y+rect.height);
			bl.setTo(rect.x+rect.width, rect.y+rect.height);

			matrixMultiplyPoint(&spr->parentTransform, &tl);
			matrixMultiplyPoint(&spr->parentTransform, &tr);
			matrixMultiplyPoint(&spr->parentTransform, &bl);
			matrixMultiplyPoint(&spr->parentTransform, &br);

			rect.x = tl.x;
			rect.y = tl.y;
			rect.width = tr.x - tl.x;
			rect.height = bl.y - tl.y;
		}

		bool nowHovering = rectContainsPoint(&rect, &point);
#else
		//@incomplete: Fully remove mouseRect?
		Matrix realTrans;
		matrixIdentity(&realTrans);
		matrixMultiply(&realTrans, &spr->transform);
		matrixMultiply(&realTrans, &spr->offsetTransform);
		matrixInvert(&realTrans);

		Point realMouse = {(float)engine->mouseX, (float)engine->mouseY};
		matrixMultiplyPoint(&realTrans, &realMouse);

		Rect rect = {0, 0, realWidth, realHeight};
		bool nowHovering = rectContainsPoint(&rect, &realMouse);

		// // if (spr->width == 65 && spr->height == 65) {
		// if (spr->assetId && strstr(spr->assetId, "choiceArrow") && spr->scaleX < 0) {
		// 	printf("%0.1f %0.1f in %0.1f %0.1f %0.1f %0.1f (%d)\n", realMouse.x, realMouse.y, rect.x, rect.y, rect.width, rect.height, nowHovering);
		// }
#endif
		bool nowPressing = spr->hovering && engine->leftMousePressed;
		spr->justPressed = false;
		spr->justReleased = false;
		spr->justHovered = false;
		spr->justUnHovered = false;
		spr->justReleasedAnywhere = false;

		if (nowHovering && engine->leftMouseJustPressed) {
			spr->lastPressedTime = engine->time;
			spr->justPressed = true;
		}
		if (nowHovering && engine->leftMouseJustReleased) {
			spr->justReleased = true;
			spr->lastReleasedTime = engine->time;
		}
		if (engine->leftMouseJustReleased && spr->holding) spr->justReleasedAnywhere = true;

		if (nowHovering && !spr->hovering) spr->justHovered = true;
		if (!nowHovering && spr->hovering) spr->justUnHovered = true;

		if (spr->justPressed) {
			spr->holdPivot.setTo(engine->mouseX, engine->mouseY);
			spr->holdTime = 0;
			spr->holding = true;

			spr->holdPivot.x -= spr->x;
			spr->holdPivot.y -= spr->y;
		}

		spr->hovering = nowHovering;
		spr->pressing = nowPressing;

		if (spr->hovering && !spr->hoveredTime) spr->hoveredTime = engine->time;
		if (!spr->hovering && spr->hoveredTime) spr->hoveredTime = 0;

		if (!engine->leftMousePressed) spr->holding = false;

		if (spr->holding) {
			spr->holdTime += engine->elapsed;
		}

		if (!spr->holding && spr->holdTime != 0) {
			spr->prevHoldTime = spr->holdTime;
			spr->holdTime = 0;
		}

		if (!spr->active && !spr->exists) return; // If the result of a button event was it no longer being active @Cleanup This can be removed I think

		if (spr->mouseRect.width == 0 || (!spr->hovering && !spr->pressing) || spr->ignoreMouseRect) {
			spr->mouseRect.x = spr->x;
			spr->mouseRect.y = spr->y;
			spr->mouseRect.width = spr->scaleX; //@cleanup This is really unintuitive
			spr->mouseRect.height = spr->scaleY;
			// printf("Rect: %0.2f, %0.2f, %0.2f, %0.2f\n", spr->mouseRect.x, spr->mouseRect.y, spr->mouseRect.width, spr->mouseRect.height);
		}
	}

	/// Animation
	if (spr->animated && spr->playing) {
		spr->timeTillNextFrame -= engine->elapsed;
		int newFrame = spr->currentFrame;

		while (spr->timeTillNextFrame <= 0) {
			if (!spr->reversed) {

				if (spr->currentAnim) {
					if (spr->currentAnimFrame+1 >= spr->currentAnim->framesNum) {
						if (spr->looping) spr->currentAnimFrame = 0;
						else spr->playing = false;
					} else {
						spr->currentAnimFrame++;
					}

					newFrame = spr->currentAnim->frames[spr->currentAnimFrame]->absFrame;
				} else {
					if (newFrame+1 >= spr->framesNum) {
						if (spr->looping) newFrame = 0;
						else spr->playing = false;
					} else {
						newFrame++;
					}
				}

			} else {

				if (spr->currentAnim) {
					if (spr->currentAnimFrame == -1) spr->currentAnimFrame = 0;
					if (spr->currentAnimFrame-1 <= -1) {
						if (spr->looping) spr->currentAnimFrame = spr->currentAnim->framesNum-1;
						else spr->playing = false;
					} else {
						spr->currentAnimFrame--;
					}

					newFrame = spr->currentAnim->frames[spr->currentAnimFrame]->absFrame;
				} else {
					if (newFrame-1 <= -1) {
						if (spr->looping) newFrame = spr->framesNum-1;
						else spr->playing = false;
					} else {
						newFrame--;
					}
				}

			}

			spr->timeTillNextFrame += 1.0/spr->animFrameRate;
		}

		spr->gotoFrame(newFrame);
	}

	spr->updateTransform();

	if (spr->deferredText) {
		spr->setText(deferredText);
		Free(spr->deferredText);
		spr->deferredText = NULL;
	}

	if (spr->fadeDestroyStartTime) {
		float timeIn = engine->time - spr->fadeDestroyStartTime;
		if (timeIn > spr->fadeDestroyDelay) {
			float timePastDelay = timeIn - spr->fadeDestroyDelay;
			if (timePastDelay < spr->fadeDestroyFadeTime) {
				float percIntoFade = 1.0 - timePastDelay / spr->fadeDestroyFadeTime;
				spr->alpha = percIntoFade;
			} else {
				spr->destroy();
			}
		}
	}
}

bool MintSprite::updateTransform() {
	MintSprite *spr = this;

	if (!spr->disableAlphaClamp) spr->alpha = mathClamp(spr->alpha);
	spr->skewX = Clamp(spr->skewX, -1, 1);
	spr->skewY = Clamp(spr->skewY, -1, 1);
	bool dirty = false;

	if (spr->x != spr->old_x) dirty = true;
	else if (spr->y != spr->old_y) dirty = true;
	else if (spr->scaleX != spr->old_scaleX) dirty = true;
	else if (spr->scaleY != spr->old_scaleY) dirty = true;
	else if (spr->skewX != spr->old_skewX) dirty = true;
	else if (spr->skewY != spr->old_skewY) dirty = true;
	else if (spr->rotation != spr->old_rotation) dirty = true;
	else if (spr->pivotX != spr->old_pivotX) dirty = true;
	else if (spr->pivotY != spr->old_pivotY) dirty = true;
	else if (spr->centerPivot != spr->old_centerPivot) dirty = true;
	else if (spr->trueCenterPivot != spr->old_trueCenterPivot) dirty = true;

	if (spr->parent) {
		spr->parent->updateTransform();
		if (!matrixEqual(&spr->parentTransform, &spr->parent->transform)) {
			spr->parentTransform = spr->parent->transform;
			dirty = true;
		}

		if (!spr->parent->disableAlphaPropagation) {
			spr->multipliedAlpha = spr->alpha * spr->parent->multipliedAlpha;
		} else {
			spr->multipliedAlpha = spr->alpha;
		}

		spr->scrolls = parent->scrolls;
		spr->zooms = parent->zooms;
	} else {
		spr->multipliedAlpha = spr->alpha;
	}


	if (dirty) {
		if (trueCenterPivot) centerPivot = false;

		Matrix *m = &spr->transform;
		matrixIdentity(m);
		matrixMultiply(m, &spr->parentTransform);

		matrixTranslate(m, x, y);

		if (spr->centerPivot) matrixTranslate(m, spr->width/2.0, spr->height/2.0);
		matrixRotate(m, -rotation); //@cleanup Should all matrices be neg rot?
		matrixScale(m, scaleX, scaleY);
		matrixSkew(m, mathMap(skewX, -1, 1, -M_PI/2.0, M_PI/2.0), mathMap(skewY, -1, 1, -M_PI/2.0, M_PI/2.0));
		if (spr->centerPivot) matrixTranslate(m, -spr->width/2.0, -spr->height/2.0);

		matrixTranslate(m, spr->pivotX, spr->pivotY);

		if (spr->trueCenterPivot) {
			if (spr->framesNum)	matrixTranslate(m, -spr->getFrameWidth()/2, -spr->getFrameHeight()/2);
			// else matrixTranslate(m, -spr->width/2, -spr->height/2);
		}

		spr->old_x = spr->x;
		spr->old_y = spr->y;
		spr->old_scaleX = spr->scaleX;
		spr->old_scaleY = spr->scaleY;
		spr->old_rotation = spr->rotation;
		spr->old_pivotX = spr->pivotX;
		spr->old_pivotY = spr->pivotY;
		spr->old_centerPivot = spr->centerPivot;
		spr->old_trueCenterPivot = spr->trueCenterPivot;
	}

	return dirty;
}

void MintSprite::fillPixels(int x, int y, int width, int height, int argb) {
	fillSpritePixels(this, x, y, width, height, argb);
}

void MintSprite::drawTiles(const char *assetId, int tileWidth, int tileHeight, int tilesWide, int tilesHigh, int *tiles) {
	drawTilesOnSprite(this, assetId, tileWidth, tileHeight, tilesWide, tilesHigh, tiles);
}

bool MintSprite::playWithoutReset(const char *animName, bool loops) {
	MintSprite *spr = this;

	Animation *newAnim = NULL;
	for (int i = 0; i < spr->animationsNum; i++)
		if (strcmp(spr->animations[i].name, animName) == 0)
			newAnim = &spr->animations[i];

	Assert(newAnim);

	spr->looping = loops;
	if (spr->currentAnim == newAnim && spr->playing) return false;

	spr->currentAnim = newAnim;
	spr->playing = true;

	return true;
}

void MintSprite::forcePlay(const char *animName, bool loops) {
	this->playing = false;
	this->play(animName);
}

void MintSprite::play(const char *animName, bool loops) {
	MintSprite *spr = this;

	if (!spr->playWithoutReset(animName, loops)) return;

	if (spr->reversed) spr->gotoAnimFrame(spr->currentAnim->framesNum-1);
	else spr->gotoAnimFrame(0);
}

void MintSprite::move(Point *p) {
	this->move(p->x, p->y);
}

void MintSprite::move(float x, float y) {
	this->x = x;
	this->y = y;
}

void MintSprite::centerOn(Point *p) {
	this->centerOn(p->x, p->y);
}

void MintSprite::centerOn(Rect *r) {
	this->centerOn(r->x + r->width/2, r->y + r->height/2);
}

void MintSprite::centerOn(MintSprite *spr) {
	this->centerOn(spr->x + spr->getWidth()/2, spr->y + spr->getHeight()/2);
}

void MintSprite::centerOnParent() {
	this->centerOn(this->parent->width/2, this->parent->height/2);
}

void MintSprite::alignOutside(Dir8 dir, int xpad, int ypad) {
	Assert(this->parent);
	this->alignOutside(this->parent, dir, xpad, ypad);
}

void MintSprite::alignOutside(MintSprite *other, Dir8 dir, int xpad, int ypad) {
	MintSprite *spr = this;
	// spr->alignInside(other, dir, 0, 0);
	spr->alignInside(other, dir, -spr->getWidth() - xpad, -spr->getHeight() - ypad);
}

void MintSprite::alignInside(Dir8 dir, int xpad, int ypad) {
	Assert(this->parent);
	this->alignInside(this->parent, dir, xpad, ypad);
}

void MintSprite::alignInside(MintSprite *other, Dir8 dir, int xpad, int ypad) {
	MintSprite *spr = this;
	Assert(spr->parent == other || spr->parent == other->parent);

	Point dirVec = {};
	if (dir == DIR8_CENTER) dirVec.setTo(0.5, 0.5);
	else if (dir == DIR8_UP) dirVec.setTo(0.5, 0);
	else if (dir == DIR8_DOWN) dirVec.setTo(0.5, 1);
	else if (dir == DIR8_LEFT) dirVec.setTo(0, 0.5);
	else if (dir == DIR8_RIGHT) dirVec.setTo(1, 0.5);
	else if (dir == DIR8_UP_LEFT) dirVec.setTo(0, 0);
	else if (dir == DIR8_UP_RIGHT) dirVec.setTo(1, 0);
	else if (dir == DIR8_DOWN_LEFT) dirVec.setTo(0, 1);
	else if (dir == DIR8_DOWN_RIGHT) dirVec.setTo(1, 1);

	Rect otherRect;
	if (other == spr->parent) {
		otherRect.x = 0;
		otherRect.y = 0;
		otherRect.width = other->getFrameWidth();
		otherRect.height = other->getFrameHeight();
	} else {
		otherRect.x = other->x;
		otherRect.y = other->y;
		otherRect.width = other->getWidth();
		otherRect.height = other->getHeight();
	}

	float minX = otherRect.x;
	float minY = otherRect.y;
	float maxX = otherRect.x + otherRect.width - spr->getWidth();
	float maxY = otherRect.y + otherRect.height - spr->getHeight();

	Point endPoint;
	endPoint.x = mathLerp(dirVec.x, minX, maxX);
	endPoint.y = mathLerp(dirVec.y, minY, maxY);

	if (dirVec.x == 0) endPoint.x += xpad;
	if (dirVec.x == 1) endPoint.x -= xpad;
	if (dirVec.y == 0) endPoint.y += ypad;
	if (dirVec.y == 1) endPoint.y -= ypad;

	spr->x = endPoint.x;
	spr->y = endPoint.y;
}

void MintSprite::centerOn(float x, float y) {
	this->x = x - this->getWidth()/2.0;
	this->y = y - this->getHeight()/2.0;
}

void MintSprite::scale(float factor) { this->scale(factor, factor); }
void MintSprite::scale(float x, float y) {
	this->scaleX = x;
	this->scaleY = y;
}

void MintSprite::scaleToPixelRatio(MintSprite *other) {
	if (other == this->parent) scaleToPixelRatio(other->getFrameWidth(), other->getFrameHeight());
	else scaleToPixelRatio(other->getWidth(), other->getHeight());
}

void MintSprite::scaleToPixelRatio(float w, float h) {
	MintSprite *spr = this;

	float r = getScaleRatio(spr->width, spr->height, w, h);
	spr->scale(r, r);
}

void MintSprite::copyProps(MintSprite *other) {
	this->x = other->x;
	this->y = other->y;
	this->scaleX = other->scaleX;
	this->scaleY = other->scaleY;
	this->rotation = other->rotation;

	this->alpha = other->alpha;
	this->tint = other->tint;
	this->visible = other->visible;
	this->layer = other->layer;

	this->pivotX = other->pivotX;
	this->pivotY = other->pivotY;
	this->centerPivot = other->centerPivot;
	this->scrolls = other->scrolls;
	this->zooms = other->zooms;
	this->smoothing = other->smoothing;
}

float MintSprite::getFrameWidth() {
	if (this->hasText) return this->textWidth;
	if (this->framesNum > 0) return this->frames[this->currentFrame].offsetData.width;
	return this->width;
}

float MintSprite::getFrameHeight() {
	if (this->hasText) return this->textHeight;
	if (this->framesNum > 0) return this->frames[this->currentFrame].offsetData.height;
	return this->height;
}

float MintSprite::getCombinedWidth() {
	MintSprite *spr = this;

	float total = getWidth();
	for (int i = 0; i < CHILDREN_LIMIT; i++) {
		MintSprite *child = spr->children[i];
		if (child) {
			total = Max(total, child->x + child->getCombinedWidth());
		}
	}

	return total;
}

float MintSprite::getCombinedHeight() {
	MintSprite *spr = this;

	float total = getHeight();
	for (int i = 0; i < CHILDREN_LIMIT; i++) {
		MintSprite *child = spr->children[i];
		if (child) {
			total = Max(total, child->y + child->getCombinedHeight());
		}
	}

	return total;
}

float MintSprite::getWidth() {
	MintSprite *spr = this;

	if (spr->framesNum > 0) return fabs(spr->getFrameWidth() * spr->scaleX);
	if (spr->textWidth != 0) return fabs(spr->textWidth * spr->scaleX);
	return fabs(spr->width * spr->scaleX);
}

float MintSprite::getHeight() {
	MintSprite *spr = this;

	if (spr->framesNum > 0) return fabs(spr->getFrameHeight() * spr->scaleY);
	if (spr->textHeight != 0) return fabs(spr->textHeight * spr->scaleX);
	return fabs(spr->height * spr->scaleY);
}

Rect MintSprite::getRect() {
	Rect rect;
	rect.x = this->x;
	rect.y = this->y;
	rect.width = this->getWidth();
	rect.height = this->getHeight();
	return rect;
}

Point MintSprite::getPoint() {
	Point p;
	p.x = this->x;
	p.y = this->y;
	return p;
}

Point MintSprite::getCenterPoint() {
	Point p;
	p.x = this->x + this->getWidth()/2;
	p.y = this->y + this->getHeight()/2;
	return p;
}

void MintSprite::copyTileNum(int tileNum, int dx, int dy, int tint) {
	MintSprite *spr = this;

	Rect *r = &spr->frames[tileNum].data;
	Rect *off = &spr->frames[tileNum].offsetData;
	Assert(r);

	spr->copyPixels(r->x, r->y, r->width, r->height, dx+off->x, dy+off->y, tint);
}

void MintSprite::copyTile(const char *tileName, int dx, int dy, int tint) {
	MintSprite *spr = this;

	int index = -1;
	for (int i = 0; i < spr->framesNum; i++) {
		if (strcmp(tileName, spr->frames[i].name) == 0) {
			index = i;
			break;
		}
	}

	Assert(index != -1);
	spr->copyTileNum(index, dx, dy, tint);
}

void MintSprite::append(const char *text, ...) {
	MintSprite *spr = this;
	MintSpriteData *man = &engine->spriteData;

	char buffer[HUGE_STR];

	va_list argptr;
	va_start(argptr, text);
	vsprintf(buffer, text, argptr);
	va_end(argptr);

	strcpy(man->textBuffer, spr->rawText);
	strcat(man->textBuffer, buffer);
	spr->setText(man->textBuffer);
}

void MintSprite::deferAppend(const char *text) {
	MintSprite *spr = this;

	char *newText;
	if (spr->deferredText) {
		newText = (char *)Malloc(strlen(spr->deferredText) + strlen(text) + 1);
		strcpy(newText, spr->deferredText);
		strcat(newText, text);
		Free(spr->deferredText);
		spr->deferredText = newText;
	} else {
		spr->deferredText = stringClone(text);
	}
}

void MintSprite::deferSetText(const char *text) {
	MintSprite *spr = this;

	if (spr->deferredText) Free(spr->deferredText);
	spr->deferredText = stringClone(text);
}

void MintSprite::appendChar(char c) {
	MintSprite *spr = this;
	MintSpriteData *man = &engine->spriteData;

	int len = strlen(spr->rawText);
	strcpy(man->textBuffer, spr->rawText);
	man->textBuffer[len] = c;
	man->textBuffer[len+1] = '\0';

	spr->setText(man->textBuffer);
}

void MintSprite::unAppend(int chars) {
	MintSprite *spr = this;
	MintSpriteData *man = &engine->spriteData;

	if (spr->deferredText) {
		int len = strlen(spr->deferredText);
		if (len <= 0) return;

		for (int i = 0; i < chars; i++) {
			spr->deferredText[len-1] = '\0';
			len--;
		}
	} else {
		int len = strlen(spr->rawText);
		if (len <= 0) return;

	strcpy(man->textBuffer, spr->rawText);
		for (int i = 0; i < chars; i++) {
			man->textBuffer[len-1] = '\0';
			len--;
		}
		spr->setText(man->textBuffer);
	}
}

void MintSprite::setText(const char *newText, ...) {
	MintSprite *spr = this;

	if (!spr->hasText) {
		spr->assetId = textAssetId;
		spr->hasText = true;
		spr->rawText = (char *)Malloc(HUGE_STR);
		spr->rawText[0] = '\0';
		spr->text = (char *)Malloc(HUGE_STR);
	}

	if (streq(newText, spr->rawText)) return;

	char buffer[HUGE_STR];

	va_list argptr;
	va_start(argptr, newText);
	vsprintf(buffer, newText, argptr);
	va_end(argptr);

	strcpy(spr->rawText, buffer);
	char *rawText = spr->rawText;

	memset(spr->text, 0, HUGE_STR);
	char *text = spr->text;

	FormatRegion regions[REGION_LIMIT];
	int regionsNum;

	spr->clear();
	parseText(rawText, text, regions, &regionsNum);
	drawText(spr, regions, regionsNum);
}

void MintSprite::fadeDestroy(float fadeTime, float delay) {
	this->fadeDestroyStartTime = engine->time;
	this->fadeDestroyFadeTime = fadeTime;
	this->fadeDestroyDelay = delay;
}

void MintSprite::destroy() {
	MintSprite *spr = this;
	if (!spr->exists) return;

	for (int i = 0; i < CHILDREN_LIMIT; i++) {
		if (spr->children[i]) {
			spr->children[i]->destroy();
		}
	}

	if (spr->parent) spr->parent->removeChild(spr);

	if (spr->rawText) Free(spr->rawText);
	if (spr->text) Free(spr->text);

	if (spr->frames) Free(spr->frames);
	if (spr->animations) Free(spr->animations);
	spr->exists = false;
	deinitSprite(spr);
}

void MintSprite::resetMouse() {
	MintSprite *spr = this;

	spr->hovering = spr->justHovered = spr->justUnHovered = spr->pressing = spr->holding = spr->justPressed = spr->justReleased = false;
	spr->mouseRect.setTo();
}

void MintSprite::clear() {
	clearSprite(this);
}

bool MintSprite::collideWith(MintSprite *spr, Rect *offsetRect) {
	return this->collideWith(spr->x, spr->y, spr->getWidth(), spr->getHeight(), offsetRect);
}

bool MintSprite::collideWith(Rect *rect, Rect *offsetRect) {
	return this->collideWith(rect->x, rect->y, rect->width, rect->height, offsetRect);
}

bool MintSprite::collideWith(double rectX, double rectY, double rectWidth, double rectHeight, Rect *offsetRect) {
	MintSprite *spr = this;
#if 1
	Point offsetPoint = {};
	Rect sprRect;
	if (offsetRect) {
		offsetPoint.x = offsetRect->x;
		offsetPoint.y = offsetRect->y;
		sprRect = *offsetRect;
	} else {
		sprRect.setTo(0, 0, spr->getWidth(), spr->getHeight());
	}

	sprRect.x += x;
	sprRect.y += y;

	while (rectContainsPoint(rectX, rectY, rectWidth, rectHeight, sprRect.x, sprRect.y + sprRect.height/2.0)) sprRect.x++;
	while (rectContainsPoint(rectX, rectY, rectWidth, rectHeight, sprRect.x + sprRect.width, sprRect.y + sprRect.height/2.0)) sprRect.x--;
	while (rectContainsPoint(rectX, rectY, rectWidth, rectHeight, sprRect.x + sprRect.width/2.0, sprRect.y)) sprRect.y++;
	while (rectContainsPoint(rectX, rectY, rectWidth, rectHeight, sprRect.x + sprRect.width/2.0, sprRect.y + sprRect.height)) sprRect.y--;

	spr->x = sprRect.x;
	spr->y = sprRect.y;
	if (offsetRect) {
		spr->x -= offsetPoint.x;
		spr->y -= offsetPoint.y;
	}
	return false;
#else
	bool inWall = true;
	while (inWall) {
		inWall = false;

		float step = 1;
		if (rectIntersectRect(rect, wall)) {
			inWall = true;
			rect->x += 1;
			if (rect->x + rect->width/2 <= wall->x + wall->width/2) rect->x -= step;
			if (rect->x + rect->width/2 >= wall->x + wall->width/2) rect->x += step;
			if (rect->y + rect->height/2 <= wall->y + wall->height/2) rect->y -= step;
			if (rect->y + rect->height/2 >= wall->y + wall->height/2) rect->y += step;
		}
	}
#endif
}


void MintSprite::addChild(MintSprite *spr) {
	spr->parent = this;

	for (int i = 0; i < CHILDREN_LIMIT; i++) {
		if (this->children[i] == NULL) {
			this->children[i] = spr;
			break;
		}
	}
}

void MintSprite::removeChild(MintSprite *spr) {
	Assert(spr->parent == this);

	for (int i = 0; i < CHILDREN_LIMIT; i++) {
		if (this->children[i] == spr) {
			this->children[i] = NULL;
			break;
		}
	}

	spr->parent = NULL;
}

void MintSprite::globalToLocal(Point *p) {
	if (!this->parent) return;

	// Point transPoint = {};
	this->updateTransform();
#if 1
	matrixMultiplyPoint(&this->parentTransform, p);
#else
	Matrix inv = this->transform;
	matrixInvert(&inv);
	matrixMultiplyPoint(&inv, &transPoint);
#endif
	// p->x -= transPoint.x;
	// p->y -= transPoint.y;
}

void MintSprite::globalToLocal(Rect *rect) {
	Point tl, tr, bl, br;
	tl.setTo(rect->x, rect->y);
	tr.setTo(rect->x+rect->width, rect->y);
	bl.setTo(rect->x, rect->y+rect->height);
	br.setTo(rect->x+rect->width, rect->y+rect->height);

	globalToLocal(&tl);
	globalToLocal(&tr);
	globalToLocal(&bl);
	globalToLocal(&br);

	rect->x = tl.x;
	rect->y = tl.y;
	rect->width = tr.x - tl.x;
	rect->height = bl.y - tl.y;
}

void MintSprite::localToGlobal(Point *p) {
	// if (!this->parent) return;

	Point invPoint = {};
	this->updateTransform();
	Matrix inv = this->transform;
	matrixInvert(&inv);
	matrixMultiplyPoint(&inv, &invPoint);
	// matrixMultiplyPoint(&inv, p);
	p->x -= invPoint.x;
	p->y -= invPoint.y;
}

void MintSprite::localToGlobal(Rect *rect) {
	Point tl, tr, bl, br;
	tl.setTo(rect->x, rect->y);
	tr.setTo(rect->x+rect->width, rect->y);
	bl.setTo(rect->x, rect->y+rect->height);
	br.setTo(rect->x+rect->width, rect->y+rect->height);

	localToGlobal(&tl);
	localToGlobal(&tr);
	localToGlobal(&bl);
	localToGlobal(&br);

	rect->x = tl.x;
	rect->y = tl.y;
	rect->width = tr.x - tl.x;
	rect->height = bl.y - tl.y;
}

void MintSprite::getAllChildren(MintSprite **arr, int *arrNum) {
	MintSprite *spr = this;

	for (int i = 0; i < CHILDREN_LIMIT; i++) {
		MintSprite *child = spr->children[i];
		if (!child) continue;

		arr[*arrNum] = child;
		*arrNum = *arrNum + 1;
		child->getAllChildren(arr, arrNum);
	}
}
