#pragma once
#include "tilemap.h"
#include "platform.h"
#include "replay.h"
#include "mintParticleSystem.h"

enum KeyState { KEY_JUST_PRESSED=1, KEY_PRESSED=2, KEY_RELEASED=3, KEY_JUST_RELEASED=4 };

struct EngineData {
	float time;
	unsigned long lastTime;
	float elapsed;
	float realElapsed;
	int frameCount;
	bool portraitMode;
	Platform platform;
	int activeSprites;
	unsigned long long buildDate;

	KeyState keys[KEY_LIMIT];
	int keysUpInFrames[KEY_LIMIT];

	int width;
	int height;
	bool disablePremultipliedAlpha;

	Rect camera;

	int mouseX;
	int mouseY;
	bool leftMousePressed;
	bool leftMouseJustPressed;
	bool leftMouseJustReleased;

	void (*initCallback)();
	void (*updateCallback)();
	void (*exitCallback)();

	RendererData renderer;
	AssetData assets;
	MintSpriteData spriteData;
	Tilemap tilemap;
	SoundData soundData;
	ReplayData replay;
	MintParticleSystemData particleSystemData;
};

void initEngine(void (*initCallbackFn)(), void (*updateCallbackFn)());
void updateEngine();
bool keyIsPressed(int key);
bool keyIsJustPressed(int key);
bool keyIsJustReleased(int key);
void pressKey(int key);
void releaseKey(int key);
void drawText(MintSprite *sprite, FormatRegion *regions, int regionsNum);
void getBuildDate(char *retDate32Char);

EngineData *engine;
