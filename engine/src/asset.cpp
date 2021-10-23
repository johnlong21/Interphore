#include "asset.h"
#include "renderer.h"
#include "platform.h"

#ifdef SEMI_SOUND_NEW
# include "newSound.h"
#else
# include "sound.h"
#endif

AssetData *assetData;

void *threadedGetTexture(void *asset);

int assetFileMem = 0;
int textureMem = 0;

void initAssets(void *assetsMemory) {
	assetData = (AssetData *)assetsMemory;

	char *names[ASSET_LIMIT];
	int namesNum = 0;
	getDirList("assets", (char **)&names, &namesNum);
	printf("Found %d assets\n", namesNum);
	for (int i = 0; i < namesNum; i++) {
		void *data;
		int dataLen = readFile(names[i], &data);
		if (names[i][0] == '/') {
			addAsset(&names[i][1], (const char *)data, dataLen);
		} else {
			addAsset(names[i], (const char *)data, dataLen);
		}

		Free(names[i]);
	}
}

void assetAdd(const char *assetName, const char *data, unsigned int dataLen) {addAsset(assetName, data, dataLen);} //@cleanup Remove this name
void addAsset(const char *assetName, const char *data, unsigned int dataLen) {
#ifdef LOG_ASSET_ADD
	printf("Adding asset %s\n", assetName);
#endif
	Asset *asset = &assetData->assets[assetData->assetsNum++];
	asset->exists = true;
	asset->level = -1;
	strcpy((char *)asset->name, assetName);

#if 1
	asset->data = (void *)data;
#else
	asset->data = Malloc(dataLen);
	memcpy(asset->data, data, dataLen);
#endif

	asset->dataLen = dataLen;
	assetFileMem += dataLen;
	// printf("Asset %s taking %.2f kb (%d bytes) Total mem: %.2f mb\n", asset->name, (float)asset->dataLen / Kilobytes(1), asset->dataLen, (float)assetFileMem / Megabytes(1));

	asset->type = BINARY_ASSET;

	// if (
	// 	strstr(assetName, ".spr") ||
	// 	strstr(assetName, ".fnt") ||
	// 	strstr(assetName, ".vert") ||
	// 	strstr(assetName, ".frag") ||
	// 	strstr(assetName, ".tmx") ||
	// 	strstr(assetName, ".js") ||
	// 	strstr(assetName, ".phore") ||
	// 	strstr(assetName, ".tsx")
	// ) {
	// 	((char *)asset->data)[asset->dataLen] = '\0'; //@cleanup Why not just all assets?  //@hack This is starting to look pretty bad
	// }

	if (strstr(assetName, ".png")) asset->type = PNG_ASSET;
	if (strstr(assetName, ".spr")) asset->type = SPR_FILE_ASSET;
	if (strstr(assetName, ".tmx")) asset->type = TMX_ASSET;
	if (strstr(assetName, ".tsx")) asset->type = TSX_ASSET;
	if (strstr(assetName, ".ogg")) asset->type = OGG_ASSET;
	if (strstr(assetName, ".fnt")) asset->type = ANGEL_CODE_ASSET;
	if (strstr(assetName, ".vert") || strstr(assetName, ".frag")) asset->type = SHADER_ASSET;

	// printf("Total asset mem is now %0.1fkb\n", assetFileMem/1024.0);
}

Asset *getAsset(const char *assetId, AssetType type) {
#ifdef LOG_ASSET_GET
	printf("Getting asset %s of type %d\n", assetId, type);
#endif

	Assert(assetId);
	// if (stringStartsWith(assetId, "assets/")) stringPrepend((char *)assetId, "bin/"); //@incomplete Actually test this sometime
	bool isAbsPath = stringStartsWith(assetId, "assets/") && assetId[strlen(assetId)-4] == '.';
	//(strncmp(assetId, "assets/", strlen("assets/")) == 0 || strncmp(assetId, "bin/assets", strlen("bin/assets")) == 0) &&
	// @incomplete Does this mean fonts are really slow again?

	if (!isAbsPath) {
		char *options[ASSET_LIMIT];
		int optionsNum = 0;

		for (int i = 0; i < assetData->assetsNum; i++) {
			Asset *asset = &assetData->assets[i];
			if (asset->exists && (type == DONTCARE_ASSET || asset->type == type)) {
				if (strstr(asset->name, assetId)) {
					// printf("Comparing %s %s (%d)\n", asset->name, assetId, strstr(asset->name, assetId));
					options[optionsNum++] = (char *)asset->name;
				}
			}
		}

		if (optionsNum == 0) return NULL;

		char *shortest = NULL;
		for (int i = 0; i < optionsNum; i++) {
			char *curString = options[i];
			if (shortest == NULL) {
				shortest = curString;
				continue;
			}

			if (strlen(curString) < strlen(shortest)) shortest = curString;
		}

		assetId = shortest; //@cleanup This returns the shortest string, not the shortest after the last slash
	}

	for (int i = 0; i < assetData->assetsNum; i++) {
		Asset *asset = &assetData->assets[i];
		if (strcmp(asset->name, assetId) == 0)
			if (asset->exists && (type == DONTCARE_ASSET || asset->type == type))
				return asset;
	}

	return NULL;
}

Asset *summonAsset(const char *assetIdPtr, AssetType type) {
	for (int i = 0; i < assetData->assetsNum; i++) {
		Asset *asset = &assetData->assets[i];
		if (asset->name == assetIdPtr)
			if (asset->exists && (type == DONTCARE_ASSET || asset->type == type))
				return asset;
	}

	return NULL;
}

void getMatchingAssets(const char *assetId, AssetType type, Asset **assetList, int *assetListNum) {
	int assetNum = 0;
	for (int i = 0; i < assetData->assetsNum; i++) {
		Asset *asset = &assetData->assets[i];
		if (!asset->exists) continue;
		if (type != DONTCARE_ASSET && asset->type != type) continue;
		if (!strstr(asset->name, assetId)) continue;

		assetList[assetNum++] = asset;
	}

	*assetListNum = assetNum;
}

Asset *getNewAsset() {
	Asset *asset = NULL;
	for (int i = 0; i < assetData->assetsNum; i++) {
		if (!assetData->assets[i].exists) {
			asset = &assetData->assets[i];
			break;
		}
	}

	if (!asset) asset = &assetData->assets[assetData->assetsNum++];
	memset(asset, 0, sizeof(Asset));

	asset->exists = true;
	return asset;
}

unsigned char *getPngBitmapData(const char *assetId, int *returnWidth, int *returnHeight) {
	Asset *pngAsset = getAsset(assetId, PNG_ASSET);
	if (!pngAsset) {
		printf("Failed to find png asset %s\n", assetId);
		Assert(0);
	}

	int channels;
	stbi_uc *img = stbi_load_from_memory((unsigned char *)pngAsset->data, pngAsset->dataLen, returnWidth, returnHeight, &channels, 4);
	textureMem += (*returnWidth) * (*returnHeight) * 4;
	// printf("Textures using %0.1fmb (if stored)\n", textureMem/1024.0/1024.0);
	if (!img) {
		printf("Failed to load %s(%d)\n", assetId, pngAsset->dataLen);
		// printf("Png data is: %p (%d)\n", pngAsset->data, pngAsset->dataLen);
		// dumpHex(pngAsset->data, pngAsset->dataLen);
		// for (int i = 0; i < assetData->assetsNum; i++) {
		// 	printf("Asset name: %s\n", assetData->assets[i].name);
		// }
		Assert(0);
	}

	return img;
}

Asset *getTextureAsset(const char *assetId) {
	Asset *textureAsset = getAsset(assetId, TEXTURE_ASSET);
	if (textureAsset) {
		return textureAsset;
	}

#ifdef LOG_TEXTURE_GEN
	printf("Generating texture %s\n", assetId);
#endif

	int width, height;
	unsigned char *img = getPngBitmapData(assetId, &width, &height);

	textureAsset = getNewAsset();
	strcpy(textureAsset->name, getAsset(assetId, PNG_ASSET)->name);
	textureAsset->type = TEXTURE_ASSET;
	textureAsset->width = width;
	textureAsset->height = height;
	unsigned long startTime = platformGetTime();
	generateTexture(img, textureAsset);
	unsigned long endTime = platformGetTime();
	if (endTime - startTime >= 5) printf("Texture %s took %dms to load!\n", assetId, endTime - startTime);

	textureAsset->level = 1;
	return textureAsset;
}

Asset *getSprFileAsset(const char *assetId) {
	char sprFileAssetId[PATH_LIMIT];
	strcpy(sprFileAssetId, assetId);

	char *ext = &sprFileAssetId[strlen(sprFileAssetId)-4];
	if (strcmp(ext, ".png") == 0) strcpy(&sprFileAssetId[strlen(sprFileAssetId)-4], ".spr");

	Asset *sprFileAsset = getAsset(sprFileAssetId, SPR_FILE_ASSET);
	return sprFileAsset;
}

Asset *getBitmapFontAsset(const char *assetId) {
	Asset *fontAsset = getAsset(assetId, BITMAP_FONT_ASSET);
	if (fontAsset) {
		return fontAsset;
	}

	fontAsset = getNewAsset();

	Asset *angelCodeAsset = getAsset(assetId, ANGEL_CODE_ASSET);
	if (!angelCodeAsset) {
		printf("Failed to load fnt files %s\n", assetId);
		Assert(0);
	}
	char *fontData = (char *)angelCodeAsset->data;

	fontAsset->type = BITMAP_FONT_ASSET;
	strcpy(fontAsset->name, angelCodeAsset->name);

	BitmapFont *font = (BitmapFont *)Malloc(sizeof(BitmapFont));
	memset(font, 0, sizeof(BitmapFont));
	font->fntPath = angelCodeAsset->name;

	const char *delim;
	if (strstr(fontData, "\r\n")) delim = "\r\n";
	else delim = "\n";
	int delimNum = strlen(delim);

	char *lineStart = fontData;

	bool moreLines = true;
	for (int i = 0; moreLines; i++) {
		char *lineEnd = strstr(lineStart, delim);

		if (!lineEnd) {
			moreLines = false;
			lineEnd = &fontData[strlen(fontData)-1];
		}

		int lineLen = lineEnd-lineStart;
		char line[LARGE_STR];
		if (lineLen <= 0) break;
		strncpy(line, lineStart, lineLen);
		line[lineLen] = '\0';

		if (i == 0) {
			sscanf(
				line,
				"info face=\"%[^\"]\" size=%d bold=%*d italic=%*d charset="" unicode=%*d stretchH=%*d smooth=%*d aa=%*d padding=%*d,%*d,%*d,%*d spacing=%*d,%*d outline=%*d",
				font->name, &font->size
			);
		}

		if (i == 1) {
			sscanf(line, "common lineHeight=%d base=%*d scaleW=%*d scaleH=%*d pages=%*d packed=%*d alphaChnl=%*d redChnl=%*d greenChnl=%*d blueChnl=%*d", &font->lineHeight);
		}

		if (i == 2) {
			sscanf(line, "page id=0 file=\"%[^\"]\"", font->pngPath);
		}

		if (strstr(line, "chars count=")) {
			int chars;
			sscanf(line, "chars count=%d", &chars);
			font->charDefs = (BitmapCharDef *)Malloc(chars * sizeof(BitmapCharDef));
		}

		if (strstr(line, "kernings count=")) {
			int kerns;
			sscanf(line, "kernings count=%d", &kerns);
			font->kernDefs = (BitmapKernDef *)Malloc(kerns * sizeof(BitmapKernDef));
		}

		if (strstr(line, "char id")) {
			BitmapCharDef *charDef = &font->charDefs[font->charDefsNum++];

			sscanf(line, "char id=%d x=%d y=%d width=%d height=%d xoffset=%d yoffset=%d xadvance=%d page=%*d  chnl=%*d", 
				&charDef->id, &charDef->x, &charDef->y, &charDef->width, &charDef->height, &charDef->xoff, &charDef->yoff, &charDef->xadvance
			);
		}

		if (strstr(line, "kerning first")) {
			BitmapKernDef *kernDef = &font->kernDefs[font->kernDefsNum++];
			sscanf(line, "kerning first=%d  second=%d  amount=%d", &kernDef->first, &kernDef->second, &kernDef->amount);
		}

		lineStart = lineEnd + delimNum;
	}

	fontAsset->data = (void *)font;
	fontAsset->type = BITMAP_FONT_ASSET;
	fontAsset->level = -1;
	strcpy(fontAsset->name, angelCodeAsset->name);
	return fontAsset;
}

void clearAssetLevel(int level, AssetType type) {
	for (int i = 0; i < assetData->assetsNum; i++) {
		Asset *asset = &assetData->assets[i];
		if (!asset->exists) continue;
		if (asset->level != level) continue;
		if (type != DONTCARE_ASSET && asset->type != type) continue;

		destroyAsset(asset);
	}
}

void destroyAsset(Asset *asset) {
	asset->exists = false;

#ifdef LOG_ASSET_EVICT
	printf("Evicting asset %s of type %d\n", asset->name, asset->type);
#endif

	if (asset->type == BITMAP_FONT_ASSET) {
		BitmapFont *font = (BitmapFont *)asset->data;
		Free(font->charDefs);
		if (font->kernDefs) Free(font->kernDefs);
		Free(font);
	} else if (asset->type == TEXTURE_ASSET) {
		// if (!textureInUse(asset))
		destroyTexture(asset);
	} else {
		Free(asset->data);
	}
}

void streamTextures(const char **assetIds, int assetIdsNum, int cacheLevel) {
	assetData->nextStreamCacheLevel = cacheLevel;
	//@incomplete Set cache level even if not flash

#ifdef SEMI_FLASH
	for (int i = 0; i < assetIdsNum; i++) {
		assetData->memoryStreamingAssetsNum++;
		Asset *pngAsset = getAsset(assetIds[i], PNG_ASSET);

#ifdef LOG_TEXTURE_STREAM
		printf("%s Stream started.\n", pngAsset->name);
#endif

		inline_as3(
			"Console.streamTexture(%0, %1, %2, %3);"
			:: "r"(pngAsset->name), "r"(strlen(pngAsset->name)), "r"(pngAsset->data), "r"(pngAsset->dataLen)
		);
	}
#endif
}

void bulkGetTextures(const char **assetIds, int assetIdsNum) {
	Assert(assetIdsNum < ASSET_BULK_LOAD_LIMIT);

#ifdef SEMI_FLASH
	disableSound();
	for (int i = 0; i < assetIdsNum; i++) getTextureAsset(assetIds[i]);
	enableSound();
#elif SEMI_MULTITHREAD
	int MAX_THREADS = 4;
	pthread_t threads[ASSET_BULK_LOAD_LIMIT] = {};
	int threadsNum = 0;
	Asset *assetReturns[ASSET_BULK_LOAD_LIMIT] = {};

	for (int i = 0; i < assetIdsNum; i++) {
		Asset *textureAsset = getAsset(assetIds[i], TEXTURE_ASSET);
		if (textureAsset) continue;

		Asset *pngAsset = getAsset(assetIds[i], PNG_ASSET);
		Assert(pngAsset);

		assetReturns[i] = getNewAsset();
		strcpy(assetReturns[i]->name, pngAsset->name);
		assetReturns[i]->exists = false;

		Assert(pthread_create(&threads[threadsNum], NULL, threadedGetTexture, assetReturns[threadsNum]) == 0);
		threadsNum++;
		if (threadsNum >= MAX_THREADS) {
			for (int i = 0; i < threadsNum; i++)
				Assert(pthread_join(threads[i], NULL) == 0);
		}
	}

#else
	disableSound();
	for (int i = 0; i < assetIdsNum; i++) {
		getTextureAsset(assetIds[i]);
	}
	enableSound();
#endif
}

void *threadedGetTexture(void *asset) {
	Asset *textureAsset = (Asset *)asset;
	Asset *pngAsset = getAsset(textureAsset->name, PNG_ASSET);
	Assert(pngAsset);

	int width, height, channels;
	stbi_uc *img = stbi_load_from_memory((unsigned char *)pngAsset->data, pngAsset->dataLen, &width, &height, &channels, 4);
	Assert(img);

	strcpy(textureAsset->name, pngAsset->name);
	textureAsset->type = TEXTURE_ASSET;
	textureAsset->width = width;
	textureAsset->height = height;
	generateTexture(img, textureAsset);

	textureAsset->level = 1;
	textureAsset->exists = true;
	return textureAsset;
}

Asset *getTextureFromData(const char *name, void *data, int width, int height, bool argb) {
	Asset *textureAsset = getAsset(name, TEXTURE_ASSET);
	if (textureAsset) return textureAsset;

	textureAsset = getNewAsset();

#ifdef LOG_TEXTURE_STREAM
	printf("%s Stream done.\n", name);
#endif

	strcpy(textureAsset->name, name);
	textureAsset->type = TEXTURE_ASSET;
	textureAsset->width = width;
	textureAsset->height = height;
	generateTexture(data, textureAsset, argb);

	textureAsset->level = 1;
	textureAsset->exists = true;

	return textureAsset;
}

void cleanupAssets() {
	for (int i = 0; i < assetData->assetsNum; i++) {
		Asset *asset = &assetData->assets[i];
		if (!asset->exists) continue;
		destroyAsset(asset);
	}
}

void parseSprAsset(Asset *sprFileAsset, Frame **framesRet, int *framesNumRet) {
	Assert(sprFileAsset);
	char *sprData = (char *)sprFileAsset->data;

	int newLineCount = 1;
	for (int i = 0; i < sprFileAsset->dataLen; i++)
		if (sprData[i] == '\n') newLineCount++;

	int totalFrames = newLineCount/3;

	Frame *frames = (Frame *)Malloc(totalFrames*sizeof(Frame));
	int framesNum = totalFrames;

	char delim[3];
	if (strstr(sprData, "\r\n")) strcpy(delim, "\r\n");
	else strcpy(delim, "\n\0");
	int delimNum = strlen(delim);

	char *lineStart = sprData;

	bool moreLines = true;
	for (int i = 0; moreLines; i++) {
		char *lineEnd = strstr(lineStart, delim);
		if (!lineEnd) {
			moreLines = false;
			lineEnd = &sprData[strlen(sprData)-1];
		}

		int lineLen = lineEnd-lineStart;
		char line[MED_STR];
		if (lineLen <= 0) break;
		strncpy(line, lineStart, lineLen);
		line[lineLen] = '\0';

		Frame *frame = &frames[i/3];
		frame->absFrame = i/3;
		if (i % 3 == 0) {
			strcpy(frame->name, line);
		} else if (i % 3 == 1) {
			sscanf(line, "%f %f %f %f", &frame->data.x, &frame->data.y, &frame->data.width, &frame->data.height);
		} else if (i % 3 == 2) {
			sscanf(line, "%f %f %f %f", &frame->offsetData.x, &frame->offsetData.y, &frame->offsetData.width, &frame->offsetData.height);
		}

		lineStart = lineEnd + delimNum;
	}

	*framesRet = frames;
	*framesNumRet = framesNum;
}

