#pragma once

#ifdef SEMI_AL
# include <AL/al.h>
# include <AL/alc.h>
#endif

#ifdef SEMI_SL
# include <SLES/OpenSLES.h>
# include <SLES/OpenSLES_Android.h>
#endif

#define STB_VORBIS_MAX_CHANNELS 2
#include "stb_vorbis.c"

struct Sound {
	bool exists;
	char *assetId;

	bool streaming;
	float length;

	float *samples;
	int samplesStreamed;
	int sampleRate;
	int samplesTotal;

	stb_vorbis_alloc vorbisTempMem;
	stb_vorbis *vorbis;
};

struct Channel {
	bool exists;
	Sound *sound;
	int samplePosition;

	char *name;
	float userVolume;
	float tweakVolume;
	float lastVolume;
	bool playing;
	bool looping;

	void destroy();
};

struct SoundTweak {
	char *assetId;
	float volume;
};

struct SoundData {
	float sampleBuffer[SAMPLE_BUFFER_LIMIT];
	Channel channels[CHANNEL_LIMIT];
	Sound sounds[SOUND_LIMIT];

	float masterVolume;

	SoundTweak tweaks[ASSET_LIMIT];

	int memoryUsed;
#ifdef SEMI_AL
ALCdevice *device;
ALCcontext *context;

ALuint buffers[2];
ALuint source;
#endif

#ifdef SEMI_SL
SLEngineItf engine;
SLObjectItf mixObj;
SLObjectItf engineObj;

SLObjectItf bqPlayerObject = NULL;
SLPlayItf bqPlayerPlay;
SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
SLMuteSoloItf bqPlayerMuteSolo;
SLVolumeItf bqPlayerVolume;

short bufferA[SAMPLE_BUFFER_LIMIT];
short bufferB[SAMPLE_BUFFER_LIMIT];
bool bufferDead = false;
int curBuffer;
#endif
};

void initSound();
void disableSound();
void enableSound();
void updateSound();
void stopChannel(const char *channelName);
void cleanupSound();

Channel *playEffect(const char *assetId, const char *channelName=NULL);
Channel *playMusic(const char *assetId, const char *channelName=NULL);
Channel *playSound(const char *assetId, const char *channelName=NULL, bool stream=true, bool looping=false);
