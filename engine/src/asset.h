#pragma once

enum AssetType { DONTCARE_ASSET = 0, BINARY_ASSET, PNG_ASSET, SPR_FILE_ASSET, TMX_ASSET, TSX_ASSET, TEXTURE_ASSET, OGG_ASSET, BITMAP_FONT_ASSET, ANGEL_CODE_ASSET, SHADER_ASSET };

struct Frame;

struct Asset {
	bool exists;
	AssetType type;

	char name[PATH_LIMIT];
	void *data;
	size_t dataLen;

	int width;
	int height;

	int level;

#ifdef SEMI_GL
	GLuint glTexture;
#endif
};

struct AssetData {
	Asset assets[ASSET_LIMIT];
	int assetsNum;

	int memoryStreamingAssetsNum;
	int nextStreamCacheLevel;
};

void initAssets(void *assetsMemory);
Asset *getAsset(const char *assetId, AssetType type=DONTCARE_ASSET);
Asset *summonAsset(const char *assetIdPtr, AssetType type=DONTCARE_ASSET);
void getMatchingAssets(const char *assetId, AssetType type, Asset **assetList, int *assetListNum);
void addAsset(const char *assetName, const char *data, unsigned int dataLen);
unsigned char *getPngBitmapData(const char *assetId, int *returnWidth, int *returnHeight);
Asset *getTextureAsset(const char *assetId);
Asset *getSprFileAsset(const char *assetId);
Asset *getNewAsset();
void clearAssetLevel(int level, AssetType type=DONTCARE_ASSET);
void destroyAsset(Asset *asset);
void streamTextures(const char **assetIds, int assetIdsNum, int cacheLevel=1);
Asset *getTextureFromData(const char *name, void *data, int width, int height, bool argb=false);
void cleanupAssets();

void bulkGetTextures(const char **assetIds, int assetIdsNum);
void parseSprAsset(Asset *sprFileAsset, Frame **framesRet, int *framesNumRet);

struct BitmapCharDef {
	int id, x, y, width, height, xoff, yoff, xadvance;
};
struct BitmapKernDef {
	int first, second, amount;
};

struct BitmapFont {
	char name[SHORT_STR];
	int size;
	int lineHeight;
	char pngPath[PATH_LIMIT];
	char *fntPath;
	BitmapCharDef *charDefs;
	BitmapKernDef *kernDefs;
	int charDefsNum;
	int kernDefsNum;
};

struct FormatRegion {
	int start;
	int end;
	char *fontName;
};
