#pragma once

#define REPLAY_FRAMES_LIMIT (5*60*60)
#define REPLAY_VERSION 1
#define REPLAY_DATA_LINE_LEN 128

struct ReplayFrame {
	int mouseX;
	int mouseY;
	bool mousePressed;
};

struct ReplayData {
	bool loadedReplays;
	Asset *replayFiles[ASSET_LIMIT];
	int replayFilesNum;
	int selectedFileIndex;

	long seed;
	ReplayFrame frames[REPLAY_FRAMES_LIMIT];
	int framesNum;

	bool recording;
	bool playingBack;
	int currentPlaybackFrame;

	MintSprite *cursor;
	int selectedSlot;
	bool playbackCurrentReplay;
};

void startRecording();
void endRecording();
void startPlayback();
void endPlayback();
void updateReplay();
void loadReplay(const char *filename);
