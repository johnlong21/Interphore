#pragma once
#include "renderer.h"
#include "mathTools.h"

struct Frame {
	char name[SHORT_STR];
	Rect data;
	Rect offsetData;
	int absFrame;
};

struct Animation {
	char name[SHORT_STR];
	Frame *frames[ANIMATION_FRAME_LIMIT];
	int framesNum;
};

struct MintSprite {
	const char *assetId;
	MintSprite *parent;
	MintSprite *children[CHILDREN_LIMIT];

	int index;
	bool exists;
	bool active;
	bool unrenderable;
	bool is9Slice;
	bool isSetup;

	float x;
	float y;
	float scaleX;
	float scaleY;
	float skewX;
	float skewY;
	float rotation;
	float pivotX;
	float pivotY;
	bool centerPivot;
	bool trueCenterPivot;

	float old_x;
	float old_y;
	float old_scaleX;
	float old_scaleY;
	float old_skewX;
	float old_skewY;
	float old_rotation;
	float old_pivotX;
	float old_pivotY;
	bool old_centerPivot;
	bool old_trueCenterPivot;

	int width;
	int height;

	int tint;
	int old_tint;

	Rect clipRect;

	int layer;
	float alpha;
	float multipliedAlpha;
	bool visible;
	bool scrolls;
	bool zooms;
	Matrix parentTransform;
	Matrix transform;
	Matrix offsetTransform;
	bool smoothing;
	bool alphaClipped;
	bool reversed;
	bool disableAlphaClamp;
	bool disableAlphaPropagation;

	Animation *animations;
	int animationsNum;
	Frame *frames;
	int framesNum;
	Animation *currentAnim;
	Rect curFrameRect;
	Point curOffsetPoint;
	bool animated;
	bool playing;
	bool looping;
	float timeTillNextFrame;
	int currentFrame;
	int currentAnimFrame;
	int animFrameRate;

	Rect mouseRect;
	bool hovering;
	bool justHovered;
	bool justUnHovered;
	bool pressing;
	bool holding;
	bool justPressed;
	bool justReleased;
	bool justReleasedAnywhere;
	Point holdPivot;
	float mouseRejectAlpha;
	bool disableMouse;
	bool ignoreMouseRect;
	float lastReleasedTime;
	float lastPressedTime;
	float holdTime;
	float prevHoldTime;

	char defaultFont[PATH_LIMIT];
	bool hasText;
	char *rawText;
	char *text;
	int wordWrapWidth;
	int fontColour;
	int textWidth;
	int textHeight;
	char *deferredText;

	float fadeDestroyStartTime;
	float fadeDestroyDelay;
	float fadeDestroyFadeTime;

	float creationTime;
	float hoveredTime;

#ifdef SEMI_GL
	GLuint texture;
	int textureWidth;
	int textureHeight;
	bool uniqueTexture;
#endif

#ifdef SEMI_FLASH
	int bitmapIndex;
#endif

	MintSprite *recreate(const char *assetId);
	void setupRect(int width, int height, int rgb=0xFF0000);
	void setupEmpty(int width, int height);
	void setupContainer(int width, int height);
	void setupImage(const char *assetId);
	void setupAnimated(const char *assetId, int width=0, int height=0);
	void setupCanvas(const char *assetId, int width=0, int height=0);
	void setupStretchedImage(const char *assetId, int width=0, int height=0);
	void setup9Slice(const char *assetId, int width, int height, int x1=5, int y1=5, int x2=10, int y2=10);
	void reloadGraphic();

	void fillPixels(int x, int y, int width, int height, int argb=0xFFFF0000);
	void copyPixels(int x, int y, int width, int height, int dx=0, int dy=0, bool bleed=false);
	void copyPixelsFromSprite(MintSprite *other, int x, int y, int width, int height, int dx=0, int dy=0, bool bleed=false);
	void copyPixelsFromAsset(Asset *sourceTextureAsset, int x, int y, int width, int height, int dx=0, int dy=0, bool bleed=false);
	void setPixels(Asset *textureAsset, int dx=0, int dy=0);
	void drawPixels(int x, int y, int width, int height, int dx, int dy, float scaleX=1, float scaleY=1, int tint=0);
	void drawPixelsFromSprite(MintSprite *other, int x, int y, int width, int height, int dx, int dy, float scaleX=1, float scaleY=1, int tint=0);
	void drawPixelsFromAsset(Asset *sourceTextureAsset, int x, int y, int width, int height, int dx, int dy, float scaleX=1, float scaleY=1, int tint=0);
	void drawTiles(const char *assetId, int tileWidth, int tileHeight, int tilesWide, int tilesHigh, int *tiles);

	bool playWithoutReset(const char *animName, bool loops=true);
	void forcePlay(const char *animName, bool loops=true);
	void play(const char *animName, bool loops=true);
	void gotoFrame(const char *frameName);
	void gotoFrame(int frameNum);
	void gotoAnimFrame(int frameNum);

	void move(Point *p);
	void move(float x, float y);
	void centerOn(Rect *r);
	void centerOn(Point *p);
	void centerOn(MintSprite *spr);
	void centerOn(float x, float y);
	void centerOnParent();
	void alignInside(MintSprite *other, Dir8 dir, int xpad=0, int ypad=0);
	void alignInside(Dir8 dir, int xpad=0, int ypad=0);
	void alignOutside(MintSprite *other, Dir8 dir, int xpad=0, int ypad=0);
	void alignOutside(Dir8 dir, int xpad=0, int ypad=0);
	void scale(float factor);
	void scale(float x, float y);
	void scaleToPixelRatio(float w, float h);
	void scaleToPixelRatio(MintSprite *other);
	void copyProps(MintSprite *other);
	float getWidth();
	float getHeight();
	float getCombinedWidth();
	float getCombinedHeight();
	float getFrameWidth();
	float getFrameHeight();
	Rect getRect();
	Point getPoint();
	Point getCenterPoint();

	void copyTileNum(int tileNum, int dx=0, int dy=0, int tint=0);
	void copyTile(const char *tileName, int dx=0, int dy=0, int tint=0);
	void setTile(const char *tileName, int tint=0);
	void setText(const char *newText, ...);
	void append(const char *text, ...);
	void appendChar(char c);
	void unAppend(int chars);

	void deferSetText(const char *text);
	void deferAppend(const char *text);

	void update();
	void addChild(MintSprite *spr);
	void removeChild(MintSprite *spr);
	bool updateTransform();
	void resetMouse();
	void clear();

	void fadeDestroy(float fadeTime, float delay=0);
	void destroy();

	bool collideWith(MintSprite *spr, Rect *offsetRect=NULL);
	bool collideWith(Rect *rect, Rect *offsetRect=NULL);
	bool collideWith(double rectX, double rectY, double rectWidth, double rectHeight, Rect *offsetRect=NULL);

	void localToGlobal(Point *p);
	void localToGlobal(Rect *rect);
	void globalToLocal(Point *p);
	void globalToLocal(Rect *rect);

	void getAllChildren(MintSprite **arr, int *arrNum); // arrNum must start at 0
};

struct MintSpriteData {
	MintSprite sprites[SPRITE_LIMIT];

	int maxIndex;
	int defaultLayer;
	char defaultFont[PATH_LIMIT];

	StringMap *tagMap;

	char textBuffer[HUGE_STR];
};

MintSprite *createMintSprite(const char *assetId=NULL, const char *animName=NULL, int frameNum=0);
bool textureInUse(Asset *asset); //@cleanup Do we need this?
void initMintSprites(void *mintSpriteMemory);
