namespace Writer {
	void setBackground(int bgNum, const char *assetId);
	void updateBackgrounds();

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
		}

	}
}
