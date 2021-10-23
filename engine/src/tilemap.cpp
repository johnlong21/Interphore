#include "tilemap.h"

Tilemap *tilemapData;

void initTilemap(void *tilemapMemory) {
	tilemapData = (Tilemap *)tilemapMemory;
	memset(tilemapData, 0, sizeof(Tilemap));
}

void Tilemap::load(const char *tmxPath) {
	Tilemap *tilemap = tilemapData;
	if (tilemap->exists) tilemap->destroy();

	memset(tilemap, 0, sizeof(Tilemap));
	tilemap->exists = true;

	Asset *tmxAsset = getAsset(tmxPath, TMX_ASSET);
	Assert(tmxAsset);

	tinyxml2::XMLDocument doc;
	doc.Parse((char *)tmxAsset->data, tmxAsset->dataLen);

	tinyxml2::XMLElement *rootEle = doc.RootElement();

	tilemap->tilesWide = rootEle->IntAttribute("width");
	tilemap->tilesHigh = rootEle->IntAttribute("height");
	tilemap->tileWidth = rootEle->IntAttribute("tilewidth");
	tilemap->tileHeight = rootEle->IntAttribute("tileheight");
	tilemap->width = tilemap->tilesWide*tilemap->tileWidth;
	tilemap->height = tilemap->tilesHigh*tilemap->tileHeight;

	tinyxml2::XMLElement *curEle = rootEle->FirstChildElement("tileset");
	/// Tile sets
	const char *tsxPath = curEle->Attribute("source");
	Asset *setAsset = getAsset(tsxPath, TSX_ASSET);
	Assert(setAsset);
	tinyxml2::XMLDocument setDoc;
	setDoc.Parse((char *)setAsset->data, setAsset->dataLen);
	tinyxml2::XMLElement *setRoot = setDoc.RootElement();

	tilemap->tilesetTileWidth = setRoot->IntAttribute("tilewidth");
	tilemap->tilesetTileHeight = setRoot->IntAttribute("tileheight");
	tilemap->tilesetTileCount = setRoot->IntAttribute("tilecount");
	tilemap->tilesetColumns = setRoot->IntAttribute("columns");

	const char *imgPath = setRoot->FirstChildElement("image")->Attribute("source");
	imgPath = strchr(imgPath, '/')+1;

	char *_dataPart = (char *)strstr(imgPath, "_data");
	if (_dataPart) *_dataPart = '\0';

	Asset *tilesetPngAsset = getAsset(imgPath, PNG_ASSET);
	Assert(tilesetPngAsset);


	tilemap->tilesetAssetId = tilesetPngAsset->name;
	curEle = curEle->NextSiblingElement();

	/// Tiled layers
	while(curEle != NULL && strcmp(curEle->Name(), "layer") == 0) {
		TiledLayer *tiledLayer = &tilemap->tiledLayers[tilemap->tiledLayersNum++];
		strcpy(tiledLayer->name, curEle->Attribute("name"));

		/// Get prefix
		char *endIndex = strchr(tiledLayer->name, '_');
		int prefixLen;
		if (endIndex) prefixLen =  endIndex - tiledLayer->name;
		else prefixLen = strlen(tiledLayer->name);
		strncpy(tiledLayer->prefix, tiledLayer->name, prefixLen);
		tiledLayer->prefix[prefixLen] = '\0';

		char *data = (char *)curEle->FirstChildElement("data")->FirstChild()->Value();
		data = data+1; // Skip opening newline

		char curEntry[SHORT_STR];
		int curEntryNum = 0;

		tiledLayer->data = (int *)Malloc(tilemap->tilesWide * tilemap->tilesHigh * sizeof(int));

		int dataAdded = 0;
		while (dataAdded < tilemap->tilesWide * tilemap->tilesHigh) {
			if (data[0] == ',' || data[0] == '\n' || data[0] == '\0') {
				curEntry[curEntryNum] = '\0';
				if (curEntryNum > 0) tiledLayer->data[dataAdded++] = atoi(curEntry);
				memset(curEntry, 0, SHORT_STR);
				curEntryNum = 0;
			} else {
				curEntry[curEntryNum++] = data[0];
			}

			data++;
		}

		curEle = curEle->NextSiblingElement();
	}

	curEle = rootEle->FirstChildElement("objectgroup");

	/// Meta objects
	tilemap->metaObjectsNum = 0; // This may not be needed
	while(curEle != NULL && strcmp(curEle->Name(), "objectgroup") == 0) {
		tinyxml2::XMLElement *objElement = curEle->FirstChildElement("object");
		while (objElement != NULL) {
			MetaObject *meta = &tilemap->metaObjects[tilemap->metaObjectsNum++];
			meta->rect.x = objElement->FloatAttribute("x");
			meta->rect.y = objElement->FloatAttribute("y");
			meta->rect.width = objElement->FloatAttribute("width");
			meta->rect.height = objElement->FloatAttribute("height");
			meta->rotation = objElement->FloatAttribute("rotation");

			if (objElement->Attribute("name") != NULL) strcpy(meta->name, objElement->Attribute("name"));

			tinyxml2::XMLElement *propsElement = objElement->FirstChildElement("properties");

			if (propsElement != NULL) {
				tinyxml2::XMLElement *propElement = propsElement->FirstChildElement("property");
				while (propElement != NULL) {
					// printf("%s = %s\n", propElement->Attribute("name"), propElement->Attribute("value"));
					MetaProperty *metaProp = &meta->properties[meta->propertiesNum++];
					strcpy(metaProp->name, propElement->Attribute("name"));
					strcpy(metaProp->value, propElement->Attribute("value"));
					propElement = propElement->NextSiblingElement();
				}
			}

			objElement = objElement->NextSiblingElement();
		}

		curEle = curEle->NextSiblingElement();
	}

	/// Tilelayers
	for (int i = 0; i < tilemap->tiledLayersNum; i++) {
		TiledLayer *tiledLayer = &tilemap->tiledLayers[i];

		bool toAdd = true;
		for (int j = 0; j < tilemap->spriteLayersNum; j++)
			if (strcmp(tilemap->spriteLayers[j].prefix, tiledLayer->prefix) == 0)
				toAdd = false;

		if (streq(tiledLayer->name, "collision")) toAdd = false; //@hack

		if (toAdd) {
			SpriteTileLayer *layer = &tilemap->spriteLayers[tilemap->spriteLayersNum++];
			layer->exists = true;
			strcpy(layer->prefix, tiledLayer->prefix);

			layer->sprite = createMintSprite();
			layer->sprite->setupEmpty(tilemap->width, tilemap->height);
			layer->sprite->assetId = getTextureAsset(tilesetPngAsset->name)->name;
		}
	}

	for (int layerNum = 0; layerNum < tilemap->tiledLayersNum; layerNum++) {
		TiledLayer *tiledLayer = &tilemap->tiledLayers[layerNum];

		for (int i = 0; i < tilemap->spriteLayersNum; i++) {
			if (streq(tilemap->spriteLayers[i].prefix, tiledLayer->prefix)) {
				tilemap->spriteLayers[i].sprite->drawTiles(tilemap->tilesetAssetId, tilemap->tileWidth, tilemap->tileHeight, tilemap->tilesWide, tilemap->tilesHigh, tiledLayer->data);
			}
		}
	}
}

void Tilemap::destroy() {
	Tilemap *tilemap = this;
	tilemap->exists = false;

	for (int i = 0; i < tilemap->tiledLayersNum; i++) {
		TiledLayer *tiledLayer = &tilemap->tiledLayers[i];
		Free(tiledLayer->data);
	}

	for (int i = 0; i < tilemap->spriteLayersNum; i++) {
		if (!tilemap->spriteLayers[i].exists) continue;
		tilemap->spriteLayers[i].sprite->destroy();
		tilemap->spriteLayers[i].exists = false;
	}
}

MetaObject *Tilemap::getMeta(const char *name) {
	Tilemap *tilemap = this;

	for (int i = 0; i < tilemap->metaObjectsNum; i++)
		if (streq(tilemap->metaObjects[i].name, name))
			return &tilemap->metaObjects[i];

	return NULL;
}

char *MetaObject::getProperty(const char *name) {
	MetaObject *meta = this;

	for (int i = 0; i < meta->propertiesNum; i++)
		if (strcmp(meta->properties[i].name, name) == 0)
			return meta->properties[i].value;

	return NULL;
}
