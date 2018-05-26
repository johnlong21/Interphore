// THIS FILE IS A TEST RE-WRITE OF THE GAME, NOTHING IN HERE IS IN USE CURRENTLY

namespace Writer {
	void initWriter(MintSprite *bgSpr);
	void deinitWriter();
	void updateWriter();

	void initWriter(MintSprite *bgSpr) {
		printf("Inited\n");
	}

	void deinitWriter() {
		printf("deinited\n");
	}

	void updateWriter() {
		printf("Updated\n");
	}
}
