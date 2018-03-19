namespace Writer {
	void setBackground(int bgNum, const char *assetId);
	void updateBackgrounds();

	void setScrollingBackground(int bgNum, float scrollX, float scrollY);
	void resetBackgroundMode(int bgNum);

	void setBackground(int bgNum, const char *assetId) {
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
					writer->bgs[i] = createMintSprite(writer->nextBgs[i]);
					writer->bgs[i]->alpha = 0;
					writer->bgs[i]->layer = lowestLayer + BG1_LAYER;
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

				bg->x += mode->scrollX;
				bg->y += mode->scrollY;
			}
		}
	}

	void setScrollingBackground(int bgNum, float scrollX, float scrollY) {
		writer->bgModes[bgNum].scrollX = scrollX;
		writer->bgModes[bgNum].scrollY = scrollX;
	}

	void resetBackgroundMode(int bgNum) {
		memset(&writer->bgModes[bgNum], 0, sizeof(BackgroundMode));
	}
}
