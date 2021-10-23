#pragma once

#define STB_VORBIS_MAX_CHANNELS 2
#include "stb_vorbis.c"

struct Channel;

void initSound();
void initSoundPlat();
void addSoundTweak(const char *assetId, float volume);
void disableSound();
void enableSound();

Channel *playSound(const char *assetId, const char *channelName=NULL, bool looping=false);
Channel *getChannel(const char *channelName);

void updateSound();
void cleanupSound();
void cleanupSoundInternal();
void mixSound();

struct Channel {
	int id;
	const char *assetId;
	float length; // In seconds
	bool exists;
	char name[SHORT_STR]; // can set
	bool looping; // can set
	bool playing; // can set
	float speed; // can set
	float perc;
	stb_vorbis_alloc vorbisTempMem;
	stb_vorbis *vorbis;

	float tweakVolume;
	float userVolume; // can set
	float lastVol;

	void destroy();

	size_t totalSamplesLeft;
};

struct SoundTweak {
	char *assetId;
	float volume;
};

struct SoundData {
	float sampleBuffer[SAMPLE_BUFFER_LIMIT];
	Channel channelList[CHANNEL_LIMIT];
	int channelListNum;

	float masterVolume;

	SoundTweak tweaks[ASSET_LIMIT];
};
