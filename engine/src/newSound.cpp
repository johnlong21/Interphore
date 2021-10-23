#include "newSound.h"

#ifdef SEMI_FLASH
extern "C" int getFlashSoundBuffer();
#endif

#ifdef SEMI_SL
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context);
void slSendSoundBuffer();
#endif

void checkSoundError(int lineNum);
void mixSound();
float computeChannelVolume(Channel *channel);
float *getSampleMemory(int memNeeded);

void initSound() {
	SoundData *sm = &engine->soundData;
	sm->masterVolume = 1;

#ifdef SEMI_AL
	printf("Openal initing\n");

	sm->device = alcOpenDevice(NULL);
	Assert(sm->device);

	ALboolean enumeration;

	enumeration = alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT");
	if (enumeration == AL_TRUE) {
		const ALCchar *devices = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
		const ALCchar *device = devices;
		const ALCchar *next = devices + 1;

		// while (device && *device != '\0' && next && *next != '\0') {
		// 	printf("Openal device: %s\n", device);
		// 	size_t len = strlen(device);
		// 	device += len + 1;
		// 	next += len + 2;
		// }
	} else {
		printf("Openal device enum isn't supported\n");
	}

	sm->context = alcCreateContext(sm->device, NULL);
	Assert(alcMakeContextCurrent(sm->context));

	alGenSources(1, &sm->source);
	alGenBuffers(2, sm->buffers);

	ALfloat listenerOri[] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f };

	alListener3f(AL_POSITION, 0, 0, 1.0f);
	alListener3f(AL_VELOCITY, 0, 0, 0);
	alListenerfv(AL_ORIENTATION, listenerOri);
	alSourcef(sm->source, AL_PITCH, 1);
	alSourcef(sm->source, AL_GAIN, 1);
	alSourcei(sm->source, AL_LOOPING, AL_FALSE);

	short startingBuffer[SAMPLE_BUFFER_LIMIT] = {};
	alBufferData(sm->buffers[0], AL_FORMAT_STEREO16, startingBuffer, SAMPLE_BUFFER_LIMIT * sizeof(short), 44100);
	alBufferData(sm->buffers[1], AL_FORMAT_STEREO16, startingBuffer, SAMPLE_BUFFER_LIMIT * sizeof(short), 44100);
	checkSoundError(__LINE__);

	alSourceQueueBuffers(sm->source, 2, sm->buffers);

	alSourcePlay(sm->source);

	int isPlaying;
	alGetSourcei(sm->source, AL_SOURCE_STATE, &isPlaying);

	checkSoundError(__LINE__);
#endif

#ifdef SEMI_SL
	const SLuint32 lEngineMixIIDCount = 1;
	const SLInterfaceID lEngineMixIIDs[] = {SL_IID_ENGINE};
	const SLboolean lEngineMixReqs[] = {SL_BOOLEAN_TRUE};
	Assert(slCreateEngine(&sm->engineObj, 0, NULL, lEngineMixIIDCount, lEngineMixIIDs, lEngineMixReqs) == SL_RESULT_SUCCESS);

	Assert((*sm->engineObj)->Realize(sm->engineObj, SL_BOOLEAN_FALSE) == SL_RESULT_SUCCESS);
	Assert((*sm->engineObj)->GetInterface(sm->engineObj, SL_IID_ENGINE, &sm->engine) == SL_RESULT_SUCCESS);

	const SLuint32 lOutputMixIIDCount = 0;
	const SLInterfaceID lOutputMixIIDs[] = {};
	const SLboolean lOutputMixReqs[] = {};
	Assert((*sm->engine)->CreateOutputMix(sm->engine, &sm->mixObj, lOutputMixIIDCount, lOutputMixIIDs, lOutputMixReqs) == SL_RESULT_SUCCESS);

	Assert((*sm->mixObj)->Realize(sm->mixObj, SL_BOOLEAN_FALSE) == SL_RESULT_SUCCESS);

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
	loc_outmix.outputMix = sm->mixObj;
	SLDataSink audioSnk = {&loc_outmix, NULL};

	// create audio player
	const SLInterfaceID ids[] = {SL_IID_PLAY, SL_IID_BUFFERQUEUE};
	const SLboolean req[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
	Assert((*sm->engine)->CreateAudioPlayer(sm->engine, &sm->bqPlayerObject, &audioSrc, &audioSnk, ArrayLength(ids), ids, req) == SL_RESULT_SUCCESS);

	Assert((*sm->bqPlayerObject)->Realize(sm->bqPlayerObject, SL_BOOLEAN_FALSE) == SL_RESULT_SUCCESS);

	Assert((*sm->bqPlayerObject)->GetInterface(sm->bqPlayerObject, SL_IID_PLAY, &sm->bqPlayerPlay) == SL_RESULT_SUCCESS);
	Assert((*sm->bqPlayerObject)->GetInterface(sm->bqPlayerObject, SL_IID_BUFFERQUEUE, &sm->bqPlayerBufferQueue) == SL_RESULT_SUCCESS);

	Assert((*sm->bqPlayerBufferQueue)->RegisterCallback(sm->bqPlayerBufferQueue, bqPlayerCallback, NULL) == SL_RESULT_SUCCESS);

	// memset(sm->buffer, 0, 2*SAMPLE_BUFFER_LIMIT*sizeof(short));

	sm->curBuffer = 0;
	memset(sm->bufferA, 0, SAMPLE_BUFFER_LIMIT*sizeof(short));
	memset(sm->bufferB, 0, SAMPLE_BUFFER_LIMIT*sizeof(short));
	slSendSoundBuffer();

	// slAudioCallback(sm->buffers[sm->curBuffer], SAMPLE_BUFFER_LIMIT/2);

	// sm->curBuffer ^= 1;
	Assert((*sm->bqPlayerPlay)->SetPlayState(sm->bqPlayerPlay, SL_PLAYSTATE_PLAYING) == SL_RESULT_SUCCESS);
#endif

#ifdef SEMI_FLASH
	inline_as3(
		"import flash.media.*;"
		"import flash.events.*;"
		"Console.globalSound = new Sound();"
		"Console.globalSound.addEventListener(SampleDataEvent.SAMPLE_DATA, Console.sampleGlobal);"
		"Console.globalChannel = Console.globalSound.play();"
		::
	);
#endif
}

void updateSound() {
	SoundData *sm = &engine->soundData;

	// if (engine->frameCount % 60 == 0) printf("Sound mem used: %fmb\n", (float)sm->memoryUsed / Megabytes(1));

	for (int i = 0; i < SOUND_LIMIT; i++) {
		Sound *sound = &sm->sounds[i];
		if (!sound->exists) continue;

		if (sound->streaming) {
			int samplesToGet = SAMPLE_BUFFER_LIMIT*2;
			int samplesLeft = sound->samplesTotal - sound->samplesStreamed;
			if (samplesToGet > samplesLeft) samplesToGet = samplesLeft;

			int samplesGot = stb_vorbis_get_samples_float_interleaved(sound->vorbis, 2, &sound->samples[sound->samplesStreamed], samplesToGet) * 2;
			sound->samplesStreamed += samplesGot;
			// printf("%s streaming %d/%d samples\n", sound->assetId, sound->samplesStreamed, sound->samplesTotal);

			if (samplesGot == 0) {
				sound->streaming = false;
				stb_vorbis_close(sound->vorbis);
				Free(sound->vorbisTempMem.alloc_buffer);
				sound->vorbis = NULL;
			}
		}
	}

#ifdef SEMI_AL
	int toProcess;
	alGetSourcei(sm->source, AL_BUFFERS_PROCESSED, &toProcess);

	bool requeued = false;
	for(int i = 0; i < toProcess; i++) {
		requeued = true;

		ALuint buffer;
		alSourceUnqueueBuffers(sm->source, 1, &buffer);
		{
			mixSound();

			short shortBuffer[SAMPLE_BUFFER_LIMIT];
			for (int i = 0; i < SAMPLE_BUFFER_LIMIT; i++) {
				shortBuffer[i] = sm->sampleBuffer[i] * 32768;
			}

			alBufferData(buffer, AL_FORMAT_STEREO16, shortBuffer, SAMPLE_BUFFER_LIMIT * sizeof(short), 44100);
			checkSoundError(__LINE__);
		}
		alSourceQueueBuffers(sm->source, 1, &buffer);
	}

	if (requeued) {
		int isPlaying;
		alGetSourcei(sm->source, AL_SOURCE_STATE, &isPlaying);
		if (isPlaying == AL_STOPPED) alSourcePlay(sm->source);
	}

	checkSoundError(__LINE__);
#endif

#ifdef SEMI_SL
	if (sm->bufferDead) {
		sm->bufferDead = false;
		mixSound();
		short *deadBuffer = sm->curBuffer ? sm->bufferA : sm->bufferB;
		for (int i = 0; i < SAMPLE_BUFFER_LIMIT; i++) deadBuffer[i] = sm->sampleBuffer[i] * 32768;
	}
#endif
}

void disableSound() {
#ifdef SEMI_FLASH
	inline_as3("Console.globalChannel.stop();"::);
#endif
}

void enableSound() {
#ifdef SEMI_FLASH
	inline_as3("Console.globalChannel = Console.globalSound.play();"::);
#endif
}

void addSoundTweak(const char *assetId, float volume) {
	Asset *oggAsset = getAsset(assetId, OGG_ASSET);

	SoundTweak *tweak = NULL;
	for (int i = 0; i < ASSET_LIMIT; i++) {
		SoundTweak *curTweak = &engine->soundData.tweaks[i];
		if (!tweak && !curTweak->assetId) tweak = curTweak;
		if (curTweak->assetId == oggAsset->name) {
			tweak = curTweak;
			break;
		}
	}

	Assert(tweak);
	tweak->assetId = oggAsset->name;
	tweak->volume = volume;
}

void mixSound() {
	SoundData *sm = &engine->soundData;

	memset(sm->sampleBuffer, 0, SAMPLE_BUFFER_LIMIT*sizeof(float));

	for (int i = 0; i < CHANNEL_LIMIT; i++) {
		Channel *channel = &sm->channels[i];
		if (!channel->exists || !channel->playing) continue;
		Sound *sound = channel->sound;

		if (sound->streaming) {
			int samplesAvail = sound->samplesStreamed - channel->samplePosition;
			// printf("Avail %d of %d needed from pos %d\n", samplesAvail, SAMPLE_BUFFER_LIMIT, channel->samplePosition);
			if (samplesAvail < SAMPLE_BUFFER_LIMIT) continue;
		}

		channel->userVolume = Clamp(channel->userVolume, 0, 1);

		float vol = computeChannelVolume(channel);
		float startVol = vol;
		float volAdd = 0;

		if (vol != channel->lastVolume) {
			float minVol = channel->lastVolume;
			float maxVol = vol;
			startVol = minVol;
			volAdd = (maxVol-minVol) / SAMPLE_BUFFER_LIMIT;
		}

		channel->lastVolume = vol;

		float samples[SAMPLE_BUFFER_LIMIT] = {};
		int samplesNum = 0;
		bool soundOver = false;

		while (samplesNum < SAMPLE_BUFFER_LIMIT && !soundOver) {
			int samplesToGet = SAMPLE_BUFFER_LIMIT - samplesNum;
			int samplesLeft = sound->samplesTotal - channel->samplePosition;
			if (samplesToGet > samplesLeft) samplesToGet = samplesLeft;
			// printf("Playing %d samples from pos %d(%0.1f sec(wrong)), %d left\n", samplesToGet, channel->samplePosition, (float)channel->samplePosition/(float)sound->sampleRate, samplesLeft);

			// printf("Getting %d\n", samplesToGet);
			for (int i = 0; i < samplesToGet; i++) {
				samples[samplesNum++] = sound->samples[channel->samplePosition++];
			}

			if (channel->samplePosition >= sound->samplesTotal-1) {
				if (channel->looping) {
					channel->samplePosition = 0;
				} else {
					soundOver = true;
				}
			}
		}

		for (int j = 0; j < SAMPLE_BUFFER_LIMIT; j+=2) {
			float curVol = startVol + volAdd*j;
			sm->sampleBuffer[j] += samples[j] * curVol;
			sm->sampleBuffer[j+1] += samples[j+1] * curVol;
		}

		if (soundOver) channel->destroy();
	}
	for (int i = 0; i < SAMPLE_BUFFER_LIMIT; i++) sm->sampleBuffer[i] = Clamp(sm->sampleBuffer[i], -1, 1);
}

void cleanupSound() {
	SoundData *sm = &engine->soundData;
	for (int i = 0; i < SOUND_LIMIT; i++) {
		Sound *sound = &sm->sounds[i];
		if (!sound->exists) continue;

		if (sound->samples) Free(sound->samples);
		if (sound->vorbis) {
			stb_vorbis_close(sound->vorbis);
			Free(sound->vorbisTempMem.alloc_buffer);
		}
		sound->exists = false;
	}

#ifdef SEMI_AL
	alDeleteSources(1, &sm->source);
	alDeleteBuffers(2, sm->buffers);
	alcMakeContextCurrent(NULL);
	alcDestroyContext(sm->context);
	alcCloseDevice(sm->device);
#endif
}

Channel *playEffect(const char *assetId, const char *channelName) {
	Channel *channel = playSound(assetId, channelName, false, false);
	return channel;
}

Channel *playMusic(const char *assetId, const char *channelName) {
	Channel *channel = playSound(assetId, channelName, true, true);
	return channel;
}

Channel *playSound(const char *assetId, const char *channelName, bool stream, bool looping) {
	SoundData *sm = &engine->soundData;

	Channel *channel = NULL;
	for (int i = 0; i < CHANNEL_LIMIT; i++) {
		if (!sm->channels[i].exists) {
			channel = &engine->soundData.channels[i];
			break;
		}
	}

	if (!channel) {
		printf("There are no more sound channels\n");
		Assert(0);
	}

	memset(channel, 0, sizeof(Channel));

	channel->userVolume = 1;
	channel->playing = true;
	channel->exists = true;
	channel->looping = looping;
	if (channelName) channel->name = stringClone(channelName);

	Asset *assetList[ASSET_LIMIT];
	int assetListNum;

	getMatchingAssets(assetId, OGG_ASSET, assetList, &assetListNum);

	if (assetListNum == 0) {
		printf("No sounds match name %s\n", assetId);
		Assert(0);
	}

	assetId = assetList[rndInt(0, assetListNum-1)]->name;
	// printf("Playing sound %s\n", assetId);
	Asset *oggAsset = getAsset(assetId, OGG_ASSET);
	Assert(oggAsset);

	channel->tweakVolume = 1;
	for (int i = 0; i < ASSET_LIMIT; i++) {
		SoundTweak *curTweak = &engine->soundData.tweaks[i];
		if (curTweak->assetId == assetId) {
			channel->tweakVolume = curTweak->volume;
			break;
		}
	}

	channel->lastVolume = computeChannelVolume(channel);

	Sound *sound = NULL;
	for (int i = 0; i < SOUND_LIMIT; i++) {
		if (sm->sounds[i].exists && sm->sounds[i].assetId == assetId) {
			sound = &engine->soundData.sounds[i];
			channel->sound = sound;
			return channel;
		}
	}

	if (!sound) {
		for (int i = 0; i < SOUND_LIMIT; i++) {
			if (!sm->sounds[i].exists) {
				sound = &engine->soundData.sounds[i];
				break;
			}
		}
	}

	memset(sound, 0, sizeof(Sound));
	sound->exists = true;

	if (!sound) {
		printf("There are no more sounds\n");
		Assert(0);
	}

	channel->sound = sound;
	sound->streaming = stream;
	sound->assetId = (char *)assetId;

	if (stream) {
		sound->vorbisTempMem.alloc_buffer = (char *)Malloc(Kilobytes(500));
		sound->vorbisTempMem.alloc_buffer_length_in_bytes = Kilobytes(500);
		int err;
		sound->vorbis = stb_vorbis_open_memory((unsigned char *)oggAsset->data, oggAsset->dataLen, &err, &sound->vorbisTempMem);

		if (err != 0) {
			printf("Stb vorbis failed to start stream with error %d\n", err);
			Assert(0);
		}

		sound->length = stb_vorbis_stream_length_in_seconds(sound->vorbis);
		sound->sampleRate = sound->vorbis->sample_rate;
		sound->samplesTotal = sound->vorbis->total_samples * 2;
		sound->samples = getSampleMemory(sound->samplesTotal * sizeof(float));
	} else {
		int channels;
		short *output;
		sound->samplesTotal = stb_vorbis_decode_memory((unsigned char *)oggAsset->data, oggAsset->dataLen, &channels, &sound->sampleRate, &output) * 2;

		if (sound->samplesTotal <= -1) {
			printf("Stb vorbis failed to decode with sample count %d\n", sound->samplesTotal);
			Assert(0);
		}

		sound->samples = (float *)Malloc(sound->samplesTotal * sizeof(float));
		sm->memoryUsed += sound->samplesTotal * sizeof(float);
		for (int i = 0; i < sound->samplesTotal; i++) {
			float sample = ((float)output[i]) / (float)32768;
			sound->samples[i] = Clamp(sample, -1, 1);
		}

		free(output);
	}

	return channel;
}

void stopChannel(const char *channelName) {
	SoundData *sm = &engine->soundData;

	for (int i = 0; i < CHANNEL_LIMIT; i++) {
		Channel *channel = &sm->channels[i];
		if (!channel->exists) continue;
		if (!streq(channel->name, channelName)) continue;

		channel->destroy();
	}
}

void Channel::destroy() {
	this->exists = false;
	if (this->name) Free(this->name);
}

void checkSoundError(int lineNum) {
#ifdef SEMI_AL
	ALCenum error = alGetError();

	if (error != AL_NO_ERROR) {
		printf("Openal is in error state %d (line: %d)\n", error, lineNum);
		Assert(0);
	}
#endif
}

float computeChannelVolume(Channel *channel) {
	SoundData *sm = &engine->soundData;
	return channel->tweakVolume * channel->userVolume * sm->masterVolume;
}

void destroySound(Sound *sound) {
	SoundData *sm = &engine->soundData;
}

float *getSampleMemory(int memNeeded) {
	SoundData *sm = &engine->soundData;

	if (sm->memoryUsed + memNeeded >= SOUND_MEMORY_LIMIT) {
		for (int i = 0; i < SOUND_LIMIT; i++) {
			if (sm->memoryUsed + memNeeded < SOUND_MEMORY_LIMIT) break;
			Sound *sound = &sm->sounds[i];
			if (!sound->exists) continue;

			bool soundUsed = false;
			//@cleanup Maybe try to remove the sound that hasn't been used in the longest amount of time
			for (int j = 0; j < CHANNEL_LIMIT; j++) {
				Channel *channel = &sm->channels[j];
				if (!channel->exists) continue;
				if (channel->sound == sound) {
					soundUsed = true;
					break;
				}
			}

			if (!soundUsed) {
				sound->exists = false;
				Free(sound->samples);
				sm->memoryUsed -= sound->samplesTotal * sizeof(float);
			}
		}
	}

	if (sm->memoryUsed + memNeeded >= SOUND_MEMORY_LIMIT) {
		printf("Not enough sound memory\n");
		Assert(0);
	}

	sm->memoryUsed += memNeeded;
	return (float *)Malloc(memNeeded);
}

#ifdef SEMI_SL
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
	slSendSoundBuffer();
}

void slSendSoundBuffer() {
	SoundData *sm = &engine->soundData;
	Assert((*sm->bqPlayerBufferQueue)->Enqueue(sm->bqPlayerBufferQueue, (sm->curBuffer ? sm->bufferA : sm->bufferB), SAMPLE_BUFFER_LIMIT*sizeof(short)) == SL_RESULT_SUCCESS);
	sm->bufferDead = true;
	sm->curBuffer = !sm->curBuffer;
}
#endif

#ifdef SEMI_FLASH
int getFlashSoundBuffer() {
	SoundData *sm = &engine->soundData;
	mixSound();
	return (int)&sm->sampleBuffer;
	printf("Don't call this\n");
	Assert(0);
	return 0;
}
#endif
