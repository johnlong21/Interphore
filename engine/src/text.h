#pragma once
struct CharRenderDef {
	char glyph;
	Asset *sourceTextureAsset;
	Rect sourceRect;
	Point destPoint;
};

void getTextRects(const char *text, int wordWrapWidth, const char *defaultFontName, FormatRegion *regions, int regionsNum, CharRenderDef *outCharDefs, int *outCharDefsNum, int *outTextWidth, int *outTextHeight);
void parseText(const char *rawText, char *outText, FormatRegion *outRegions, int *outRegionsNum);
