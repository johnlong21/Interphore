#pragma once

struct TiledLayer {
	char prefix[SHORT_STR];
	char name[SHORT_STR];
	int *data;
};

struct MetaProperty {
	char name[SHORT_STR];
	char value[PATH_LIMIT];
};

struct MetaObject {
	char name[SHORT_STR];
	Rect rect;
	float rotation;

	MetaProperty properties[TILEMAP_PROPERTY_LIMIT];
	int propertiesNum;

	char *getProperty(const char *name);
};

struct SpriteTileLayer {
	bool exists;
	char prefix[SHORT_STR];
	MintSprite *sprite;
};

struct Tilemap {
	bool exists;

	TiledLayer tiledLayers[TILED_LAYERS_LIMIT];
	int tiledLayersNum;

	SpriteTileLayer spriteLayers[TILEMAP_SPRITE_LAYER_LIMIT];
	int spriteLayersNum;

	MetaObject metaObjects[TILEMAP_META_LIMIT];
	int metaObjectsNum;

	int tilesWide;
	int tilesHigh;
	int tileWidth;
	int tileHeight;
	int width;
	int height;

	int tilesetTileWidth;
	int tilesetTileHeight;
	int tilesetTileCount;
	int tilesetColumns;
	char *tilesetAssetId;

	void load(const char *tmxPath);
	MetaObject *getMeta(const char *name);
	void destroy();
};
