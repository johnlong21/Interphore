#include "sound.h"
#include "engine.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
void slSendSoundBuffer();
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context);

SLEngineItf slEngine;
SLObjectItf slMixObj;
SLObjectItf slEngineObj;

SLObjectItf bqPlayerObject = NULL;
SLPlayItf bqPlayerPlay;
SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
SLMuteSoloItf bqPlayerMuteSolo;
SLVolumeItf bqPlayerVolume;

short slBufferA[SAMPLE_BUFFER_LIMIT];
short slBufferB[SAMPLE_BUFFER_LIMIT];
bool slBufferDead = false;
int slCurBuffer;

void initSoundPlat() {
	const SLuint32 lEngineMixIIDCount = 1;
	const SLInterfaceID lEngineMixIIDs[] = {SL_IID_ENGINE};
	const SLboolean lEngineMixReqs[] = {SL_BOOLEAN_TRUE};
	Assert(slCreateEngine(&slEngineObj, 0, NULL, lEngineMixIIDCount, lEngineMixIIDs, lEngineMixReqs) == SL_RESULT_SUCCESS);

	Assert((*slEngineObj)->Realize(slEngineObj, SL_BOOLEAN_FALSE) == SL_RESULT_SUCCESS);
	Assert((*slEngineObj)->GetInterface(slEngineObj, SL_IID_ENGINE, &slEngine) == SL_RESULT_SUCCESS);

	const SLuint32 lOutputMixIIDCount = 0;
	const SLInterfaceID lOutputMixIIDs[] = {};
	const SLboolean lOutputMixReqs[] = {};
	Assert((*slEngine)->CreateOutputMix(slEngine, &slMixObj, lOutputMixIIDCount, lOutputMixIIDs, lOutputMixReqs) == SL_RESULT_SUCCESS);

	Assert((*slMixObj)->Realize(slMixObj, SL_BOOLEAN_FALSE) == SL_RESULT_SUCCESS);

	SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
	SLDataFormat_PCM format_pcm = {};
	format_pcm.formatType = SL_DATAFORMAT_PCM;
	format_pcm.numChannels = 2;
	format_pcm.samplesPerSec = SL_SAMPLINGRATE_44_1;
	format_pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
	format_pcm.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
	format_pcm.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
	format_pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;

	SLDataSource audioSrc;
	audioSrc.pLocator = &loc_bufq;
	audioSrc.pFormat = &format_pcm;

	// configure audio sink
	SLDataLocator_OutputMix loc_outmix = {};
	loc_outmix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
	loc_outmix.outputMix = slMixObj;
	SLDataSink audioSnk = {&loc_outmix, NULL};

	// create audio player
	const SLInterfaceID ids[] = {SL_IID_PLAY, SL_IID_BUFFERQUEUE};
	const SLboolean req[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
	Assert((*slEngine)->CreateAudioPlayer(slEngine, &bqPlayerObject, &audioSrc, &audioSnk, ArrayLength(ids), ids, req) == SL_RESULT_SUCCESS);

	Assert((*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE) == SL_RESULT_SUCCESS);

	Assert((*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay) == SL_RESULT_SUCCESS);
	Assert((*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE, &bqPlayerBufferQueue) == SL_RESULT_SUCCESS);

	Assert((*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, NULL) == SL_RESULT_SUCCESS);

	// memset(slBuffer, 0, 2*SAMPLE_BUFFER_LIMIT*sizeof(short));

	slCurBuffer = 0;
	memset(slBufferA, 0, SAMPLE_BUFFER_LIMIT*sizeof(short));
	memset(slBufferB, 0, SAMPLE_BUFFER_LIMIT*sizeof(short));
	slSendSoundBuffer();

	// slAudioCallback(slBuffers[slCurBuffer], SAMPLE_BUFFER_LIMIT/2);

	// slCurBuffer ^= 1;
	Assert((*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING) == SL_RESULT_SUCCESS);
}

void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
	slSendSoundBuffer();
}

void slSendSoundBuffer() {
	Assert((*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, (slCurBuffer ? slBufferA : slBufferB), SAMPLE_BUFFER_LIMIT*sizeof(short)) == SL_RESULT_SUCCESS);
	slBufferDead = true;
	slCurBuffer = !slCurBuffer;
}

void updateSound() {
	if (slBufferDead) {
		slBufferDead = false;
		mixSound();
		short *deadBuffer = slCurBuffer ? slBufferA : slBufferB;
		for (int i = 0; i < SAMPLE_BUFFER_LIMIT; i++) deadBuffer[i] = engine->soundData.sampleBuffer[i] * 32768;
	}
}

void disableSound() {
}

void enableSound() {
}

void cleanupSoundInternal() {
	(*slMixObj)->Destroy(slMixObj);
	slMixObj = NULL;

	(*slEngineObj)->Destroy(slEngineObj);
	slEngineObj = NULL;
	slEngine = NULL;
}
