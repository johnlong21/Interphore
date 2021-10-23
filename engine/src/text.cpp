#include "text.h"

BitmapCharDef *getCharDef(BitmapFont *font, int ch);
int getKern(BitmapFont *font, int first, int second);

BitmapCharDef *getCharDef(BitmapFont *font, int ch) {
	if (ch == '\'' && strstr(font->name, "Espresso-Dolce")) ch = '"';
	else if (ch == '"' && strstr(font->name, "Espresso-Dolce")) ch = '\'';

	for (int i = 0; i < font->charDefsNum; i++) {
		if (font->charDefs[i].id == ch)
			return &font->charDefs[i];
	}

	return NULL;
}

int getKern(BitmapFont *font, int first, int second) {
	if (first == '\'' && strstr(font->name, "Espresso-Dolce")) first = '"';
	else if (first == '"' && strstr(font->name, "Espresso-Dolce")) first = '\'';

	if (second == '\'' && strstr(font->name, "Espresso-Dolce")) second = '"';
	else if (second == '"' && strstr(font->name, "Espresso-Dolce")) second = '\'';

	for (int i = 0; i < font->kernDefsNum; i++)
		if (font->kernDefs[i].first == first && font->kernDefs[i].second == second)
			return font->kernDefs[i].amount;

	return 0;
}

void parseText(const char *rawText, char *outText, FormatRegion *outRegions, int *outRegionsNum) {
	FormatRegion *regions = outRegions;
	int regionsNum = 0;
	char *text = outText;

	int curChar = 0;
	bool inRegion = false;
	for (;;) {
		const char *angleBracket = strchr(&rawText[curChar], '<');
		if (angleBracket == NULL) {
			strncat(text, &rawText[curChar], strlen(rawText) - curChar);
			break;
		}

		int angleIndex = angleBracket - rawText;

		const char *endAngleBracket = strchr(angleBracket, '>');
		const char *nextAngleBracket = strchr(angleBracket+1, '<');
		bool nextIsCloser = nextAngleBracket && nextAngleBracket < endAngleBracket;
		if (!endAngleBracket || nextIsCloser) {
			strncat(text, &rawText[curChar], angleIndex - curChar);
			strcat(text, "<");
			curChar = angleIndex+1;
			continue;
		}
		Assert(endAngleBracket);

		int endAngleIndex = endAngleBracket - rawText;
		strncat(text, &rawText[curChar], angleIndex - curChar);

		// printf("Found angle brackets at %d %d (%s) (%c)\n", angleIndex, endAngleIndex, text, rawText[angleIndex]);
		if (rawText[angleIndex+1] != '/') {
			char tag[SHORT_STR];
			int tagLen = endAngleIndex - angleIndex - 1;
			strncpy(tag, &rawText[angleIndex+1], tagLen);
			tag[tagLen] = '\0';

			if (!engine->spriteData.tagMap->exists(tag)) {
				strcat(text, "<");
				curChar = angleIndex+1;
				continue;
			}

			regions[regionsNum].fontName = engine->spriteData.tagMap->getString(tag);
			regions[regionsNum].start = strlen(text);
			inRegion = true;
		} else {
			if (!inRegion) {
				strcat(text, "<");
				curChar = angleIndex+1;
				continue;
			}
			regions[regionsNum].end = strlen(text);
			regionsNum++;
			inRegion = false;
		}
		curChar = endAngleIndex+1;
	}

	*outRegionsNum = regionsNum;
}

void getTextRects(const char *text, int wordWrapWidth, const char *defaultFontName, FormatRegion *regions, int regionsNum, CharRenderDef *outCharDefs, int *outCharDefsNum, int *outTextWidth, int *outTextHeight) {
	CharRenderDef *defs = outCharDefs;
	int defsNum = 0;
	int textWidth = 0;
	int textHeight = 0;

	Asset *defaultFontAsset;
	if (defaultFontName) defaultFontAsset = getBitmapFontAsset(defaultFontName);
	else defaultFontAsset = getBitmapFontAsset(engine->spriteData.defaultFont);

	BitmapFont *defaultFont = (BitmapFont *)defaultFontAsset->data;
	BitmapFont *curFont = defaultFont;

	for (int i = 0; i < regionsNum; i++) {
		regions[i].fontName = getBitmapFontAsset(regions[i].fontName)->name;
	}

	// int lineBreaks[HUGE_STR];
	// int lineBreaksNum = 0;

	Point cursor;
	cursor.setTo(0, 0);
	char prevChar = -1;

	int textLen = strlen(text);

	Asset *curFontTexture = NULL;
	const char *lastFontName = NULL;
	for (int i = 0; i < textLen; i++) {
		// printf("Looking at char #%d %c\n", i, text[i]);
		if (text[i] == '\n') {
			cursor.x = 0;
			cursor.y += curFont->lineHeight;
			textHeight += curFont->lineHeight;
			// lineBreaks[lineBreaksNum++] = i;
			continue;
		}

		if (prevChar == 32) {
			const char *nextBreakPtr = strpbrk(&text[i], " \n");
			int nextBreakIndex;
			if (nextBreakPtr != NULL) nextBreakIndex = nextBreakPtr - text;
			else nextBreakIndex = textLen;

			int prevWordChar = -1;
			float wordWidth = 0;
			// printf("Going from %d to %d\n", i, nextBreakIndex);
			for (int j = i; j < nextBreakIndex; j++) {
				FormatRegion *curRegion = NULL;

				for (int k = 0; k < regionsNum; k++)
					if (j >= regions[k].start && j < regions[k].end)
						curRegion = &regions[k];

				if (curRegion == NULL) curFont = defaultFont;
				else curFont = (BitmapFont *)getBitmapFontAsset(curRegion->fontName)->data;

				char curWordChar = text[j];
				BitmapCharDef *def = getCharDef(curFont, curWordChar);
				if (!def) continue;

				wordWidth += def->xadvance + getKern(curFont, prevWordChar, curWordChar) + 1;
				prevWordChar = curWordChar;
			}

			if (cursor.x + wordWidth > wordWrapWidth) {
				// char word[256] = {};
				// strncpy(word, &text[i], nextBreakIndex - i);
				// printf("Word |%s| is %f long, line is %f long, that would be %f, which is more than %d\n", word, wordWidth, cursor.x, cursor.x + wordWidth, wordWrapWidth);

				cursor.x = 0;
				cursor.y += curFont->lineHeight;
				textHeight += curFont->lineHeight;
			}

			// lineBreaks[lineBreaksNum++] = i;
		}

		FormatRegion *curRegion = NULL;
		for (int j = 0; j < regionsNum; j++)
			if (i >= regions[j].start && i < regions[j].end)
				curRegion = &regions[j];

		const char *curFontName;
		if (curRegion == NULL) {
			curFontName = defaultFontAsset->name;
		} else {
			curFontName = curRegion->fontName;
		}

		bool wouldSwitch = !lastFontName || lastFontName != curFontName;
		if (wouldSwitch) {
			curFont = (BitmapFont *)getBitmapFontAsset(curFontName)->data;
			curFontTexture = getTextureAsset(curFont->pngPath);
			lastFontName = curFontName;
		}

		if (text[i] == 0) continue; // New line removal makes this happen. //@cleanup Really?

		cursor.x += round(getKern(curFont, prevChar, text[i]));

		BitmapCharDef *charDef = NULL;

		if (text[i] == (char)-30 && i < textLen+3) {
			if (text[i+1] == (char)-128 && text[i+2] == (char)-108) {
				charDef = getCharDef(curFont, 8212);
			}
		} else {
			charDef = getCharDef(curFont, text[i]);
		}

		if (!charDef) continue;
		int charX = cursor.x + charDef->xoff;
		int charY = cursor.y + charDef->yoff;

		CharRenderDef def;
		def.glyph = text[i];
		def.sourceTextureAsset = curFontTexture;
		def.sourceRect.setTo(charDef->x, charDef->y, charDef->width, charDef->height);
		def.destPoint.setTo(charX, charY);
		defs[defsNum++] = def;

		textWidth = fmax((charX+charDef->width), textWidth);
		cursor.x += charDef->xadvance;
		prevChar = text[i];
	}

	textHeight += curFont->lineHeight;

	*outTextWidth = textWidth;
	*outTextHeight = textHeight;
	*outCharDefsNum = defsNum;
}

