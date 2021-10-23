#include "replay.h"

void startRecording() {
	ReplayData *replay = &engine->replay;
	replay->seed = time(NULL);
	srand(replay->seed);
	printf("Started recording seed: %ld (%0.1fkb)\n", replay->seed, sizeof(ReplayData)/1024.0);

	replay->recording = true;
	replay->framesNum = 0;
}

void endRecording() {
	ReplayData *replay = &engine->replay;
	printf("Ending recording, got %d frames\n", replay->framesNum);

	String *saveStr = newString(1024);
	saveStr->append(SEMI_STRINGIFY(REPLAY_VERSION));
	saveStr->append("\n");

	char buf[128];
	sprintf(buf, "%d\n%ld\n", replay->framesNum, replay->seed);
	saveStr->append(buf);

	for (int i = 0; i < REPLAY_FRAMES_LIMIT; i++) {
		ReplayFrame *frame = &replay->frames[i];

		char buf[REPLAY_DATA_LINE_LEN];
		sprintf(buf, "%d %d %d\n", frame->mouseX, frame->mouseY, frame->mousePressed);
		saveStr->append(buf);
	}

	for (int i = replay->selectedSlot; ; i++) {
		char buf[PATH_LIMIT];
		sprintf(buf, "replay%d.rpl", i);
		if (!fileExists(buf)) {
			printf("Saving to %s\n", buf);
			writeFile(buf, saveStr->cStr);
			break;
		}
	}

	saveStr->destroy();

	replay->recording = false;
	replay->playbackCurrentReplay = true;
}

void startPlayback() {
	ReplayData *replay = &engine->replay;

	if (!replay->playbackCurrentReplay) {
		if (replay->loadedReplays && replay->selectedSlot == 9) {
			loadReplay(replay->replayFiles[replay->selectedFileIndex]->name);
		} else {
			char buf[PATH_LIMIT];
			sprintf(buf, "replay%d.rpl", replay->selectedSlot);
			loadReplay(buf);
		}
	}

	printf("Started playback with seed %ld\n", replay->seed);

	srand(replay->seed);
	replay->cursor = createMintSprite();
	replay->cursor->setupRect(32, 32, 0xFFFFFF);

	replay->playingBack = true;
	replay->currentPlaybackFrame = 0;
}

void loadReplay(const char *filename) {
	ReplayData *replay = &engine->replay;

	char *data;
	int dataLen = readFile(filename, (void **)&data);

	if (!dataLen) {
		printf("File %s doesn't exist\n", filename);
		return;
	}
	replay->framesNum = 0;

	int totalFrames = 0;
	int loadedVersion = 0;
	char *lineStart = data;
	int lineNum = 0;
	for (;;) {
		char *lineEnd = strstr(lineStart, "\n");
		if (!lineEnd) lineEnd = &data[strlen(data)-1];

		int lineLen = lineEnd - lineStart;
		if (lineLen <= 0) break;

		char line[REPLAY_DATA_LINE_LEN];
		strncpy(line, lineStart, lineLen);
		line[lineLen] = '\0';

		if (lineNum == 0) {
			loadedVersion = atoi(line);
		} else if (lineNum == 1) {
			totalFrames = atoi(line);
		} else if (lineNum == 2) {
			replay->seed = atol(line);
		} else {
			ReplayFrame *frame = &replay->frames[replay->framesNum++];
			sscanf(line, "%d %d %d", &frame->mouseX, &frame->mouseY, (int *)&frame->mousePressed);
			if (replay->framesNum >= totalFrames) break;
		}

		lineNum++;
		lineStart = lineEnd+1;
	}

	free(data);
}

void endPlayback() {
	ReplayData *replay = &engine->replay;
	printf("Ending playback\n");

	replay->cursor->destroy();

	replay->playingBack = false;
}

void updateReplay() {
	ReplayData *replay = &engine->replay;

	if (replay->recording) {
		if (replay->framesNum >= REPLAY_FRAMES_LIMIT) {
			printf("Out of frames\n");
			return;
		}

		ReplayFrame *frame = &replay->frames[replay->framesNum++];
		frame->mouseX = platformMouseX;
		frame->mouseY = platformMouseY;
		frame->mousePressed = platformMouseLeftDown;
	}

	if (replay->playingBack) {
		if (replay->currentPlaybackFrame >= replay->framesNum) {
			printf("Replay over.\n");
			endPlayback();
			return;
		}

		ReplayFrame *frame = &replay->frames[replay->currentPlaybackFrame++];
		platformMouseX = frame->mouseX;
		platformMouseY = frame->mouseY;
		platformMouseLeftDown = frame->mousePressed;

		replay->cursor->layer = 9999;
		replay->cursor->x = frame->mouseX - replay->cursor->width/2;
		replay->cursor->y = frame->mouseY - replay->cursor->height/2;
		replay->cursor->tint = frame->mousePressed ? 0xFFFF0000 : 0;
	}

	if (keyIsPressed(KEY_CTRL) && keyIsJustReleased('R')) {
		if (replay->playingBack) return;
		if (!replay->recording) startRecording();
		else endRecording();
	}

	if (keyIsPressed(KEY_CTRL) && keyIsJustReleased('P')) {
		if (replay->recording) return;
		if (!replay->playingBack) startPlayback();
		else endPlayback();
	}

	int newSlot = -1;
	if (keyIsPressed(KEY_CTRL) && keyIsJustReleased('0')) newSlot = 0;
	if (keyIsPressed(KEY_CTRL) && keyIsJustReleased('1')) newSlot = 1;
	if (keyIsPressed(KEY_CTRL) && keyIsJustReleased('2')) newSlot = 2;
	if (keyIsPressed(KEY_CTRL) && keyIsJustReleased('3')) newSlot = 3;
	if (keyIsPressed(KEY_CTRL) && keyIsJustReleased('4')) newSlot = 4;
	if (keyIsPressed(KEY_CTRL) && keyIsJustReleased('5')) newSlot = 5;
	if (keyIsPressed(KEY_CTRL) && keyIsJustReleased('6')) newSlot = 6;
	if (keyIsPressed(KEY_CTRL) && keyIsJustReleased('7')) newSlot = 7;
	if (keyIsPressed(KEY_CTRL) && keyIsJustReleased('8')) newSlot = 8;
	if (keyIsPressed(KEY_CTRL) && keyIsJustReleased('9')) newSlot = 9;
	
	if (newSlot != -1) {
		replay->selectedSlot = newSlot;
		replay->playbackCurrentReplay = false;
		printf("Switched to replay slot %d\n", newSlot);
	}

	int cycleFile = 0;
	if (keyIsPressed(KEY_CTRL) && keyIsJustReleased('Q')) {
		if (replay->recording) return;
		if (replay->playingBack) return;
		cycleFile = -1;
	}

	if (keyIsPressed(KEY_CTRL) && keyIsJustReleased('E')) {
		if (replay->recording) return;
		if (replay->playingBack) return;
		cycleFile = 1;
	}

	if (cycleFile != 0) {
		if (!replay->loadedReplays) {
			replay->loadedReplays = true;
			getMatchingAssets("assets/info/replay/", DONTCARE_ASSET, replay->replayFiles, &replay->replayFilesNum);
		}

		replay->selectedFileIndex += cycleFile;
		if (replay->selectedFileIndex >= replay->replayFilesNum) replay->selectedFileIndex = 0;
		if (replay->selectedFileIndex < 0) replay->selectedFileIndex = replay->replayFilesNum-1;
		printf("Replay %s selected (Ctrl+9 Ctrl+p to play)\n", replay->replayFiles[replay->selectedFileIndex]->name);
	}
}
