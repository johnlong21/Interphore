#pragma once

namespace Writer {
	void initWriter(MintSprite *bgSpr);
	void deinitWriter();
	void updateWriter();

	CTinyJS *jsInterp;
}
