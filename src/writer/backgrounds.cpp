namespace Writer {
	void setBackground(int bgNum, const char *assetId, BackgroundType bgType);
	void updateBackgrounds();

	void resetBackgroundMode(int bgNum);
	void setBackgroundBob(int bgNum, float bobX, float bobY);

	void setBackground(int bgNum, const char *assetId, BackgroundType bgType) {
		if (writer->bgs[bgNum] && strstr(writer->bgs[bgNum]->assetId, assetId)) return;
		writer->bgModes[bgNum].type = bgType;
		writer->nextBgs[bgNum] = stringClone(assetId);
	}

	void updateBackgrounds() {
		for (int i = 0; i < BGS_MAX; i++) {
			if (writer->nextBgs[i] && writer->bgs[i]) {
				writer->bgs[i]->alpha -= 0.05;

				if (writer->bgs[i]->alpha <= 0) {
					writer->bgs[i]->destroy();
					writer->bgs[i] = NULL;
				}
			}

			if (writer->nextBgs[i] && writer->bgs[i] == NULL) {
				if (writer->nextBgs[i][0] != '\0') {
					MintSprite *spr = NULL;
					if (writer->bgModes[i].type == BG_NORMAL) {
						spr = createMintSprite(writer->nextBgs[i]);
					} else if (writer->bgModes[i].type == BG_TILED) {
						spr = createMintSprite();
						spr->setupCanvas(writer->nextBgs[i], engine->width, engine->height);

						Asset *textureAsset = getTextureAsset(writer->nextBgs[i]);
						int imgWidth = textureAsset->width;
						int imgHeight = textureAsset->height;
						int rows = (engine->width / textureAsset->width) + 1;
						int cols = (engine->height / textureAsset->height) + 1;
						// printf("r/c: %d %d\n", rows, cols);
						for (int row = 0; row < rows; row++) {
							for (int col = 0; col < cols; col++) {
								spr->copyPixels(0, 0, imgWidth, imgHeight, row * imgWidth, col * imgHeight);
							}
						}
					} else if (writer->bgModes[i].type == BG_CENTERED) {
						spr = createMintSprite();
						spr->setupCanvas(writer->nextBgs[i], engine->width, engine->height);
						Asset *textureAsset = getTextureAsset(writer->nextBgs[i]);
						int imgWidth = textureAsset->width;
						int imgHeight = textureAsset->height;

						spr->copyPixels(0, 0, imgWidth, imgHeight, (spr->width - imgWidth)/2, (spr->height - imgHeight)/2);
					}
					spr->alpha = 0;
					spr->layer = lowestLayer + BG_LAYER + i;
					writer->bgs[i] = spr;
				}

				Free(writer->nextBgs[i]);
				writer->nextBgs[i] = NULL;
			}

			if (writer->nextBgs[i] == NULL && writer->bgs[i]) {
				writer->bgs[i]->alpha += 0.05;
			}

			if (writer->bgs[i]) {
				MintSprite *bg = writer->bgs[i];
				BackgroundMode *mode = &writer->bgModes[i];

				bg->x = sin(engine->time * mode->bobX) * 30;
				bg->y = sin(engine->time * mode->bobY) * 30;
			}
		}
	}

	void setBackgroundBob(int bgNum, float bobX, float bobY) {
		writer->bgModes[bgNum].bobX = bobX;
		writer->bgModes[bgNum].bobY = bobY;
	}

	void resetBackgroundMode(int bgNum) {
		memset(&writer->bgModes[bgNum], 0, sizeof(BackgroundMode));
	}
}
