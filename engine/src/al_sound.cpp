#include "sound.h"
#include "engine.h"

#include <AL/al.h>
#include <AL/alc.h>

ALCdevice *soundDevice;
ALCcontext *soundContext;

ALuint alBuffers[2];
ALuint alSource;

void checkAlError(int lineNum);
void soundAppendAudio(int buffer);

void initSoundPlat() {
	printf("Openal initing\n");

	soundDevice = alcOpenDevice(NULL);
	Assert(soundDevice);

	ALboolean enumeration;

	enumeration = alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT");
	if (enumeration == AL_TRUE) {
		const ALCchar *devices = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
		const ALCchar *device = devices;
		const ALCchar *next = devices + 1;

		while (device && *device != '\0' && next && *next != '\0') {
			printf("Openal device: %s\n", device);
			size_t len = strlen(device);
			device += len + 1;
			next += len + 2;
		}
	} else {
		printf("Openal device enum isn't supported\n");
	}

	soundContext = alcCreateContext(soundDevice, NULL);
	Assert(alcMakeContextCurrent(soundContext));

	alGenSources(1, &alSource);
	alGenBuffers(2, alBuffers);

	ALfloat listenerOri[] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f };

	alListener3f(AL_POSITION, 0, 0, 1.0f);
	alListener3f(AL_VELOCITY, 0, 0, 0);
	alListenerfv(AL_ORIENTATION, listenerOri);
	alSourcef(alSource, AL_PITCH, 1);
	alSourcef(alSource, AL_GAIN, 1);
	alSourcei(alSource, AL_LOOPING, AL_FALSE);

	soundAppendAudio(alBuffers[0]);
	soundAppendAudio(alBuffers[1]);

	alSourceQueueBuffers(alSource, 2, alBuffers);

	alSourcePlay(alSource);

	int isPlaying;
	alGetSourcei(alSource, AL_SOURCE_STATE, &isPlaying);

	checkAlError(__LINE__);
}

void updateSound() {
	int toProcess;
	alGetSourcei(alSource, AL_BUFFERS_PROCESSED, &toProcess);

	bool requeued = false;
	while (toProcess > 0) {
		requeued = true;
		toProcess--;

		ALuint buffer;
		alSourceUnqueueBuffers(alSource, 1, &buffer);
		soundAppendAudio(buffer);
		alSourceQueueBuffers(alSource, 1, &buffer);
	}

	if (requeued) {
		int isPlaying;
		alGetSourcei(alSource, AL_SOURCE_STATE, &isPlaying);
		if (isPlaying == AL_STOPPED) alSourcePlay(alSource);
	}

	checkAlError(__LINE__);
}

void disableSound() {
}

void enableSound() {
}

void soundAppendAudio(int buffer) {
	mixSound();

	short shortBuffer[SAMPLE_BUFFER_LIMIT];
	for (int i = 0; i < SAMPLE_BUFFER_LIMIT; i++) {
		shortBuffer[i] = engine->soundData.sampleBuffer[i] * 32768;
	}

	alBufferData(buffer, AL_FORMAT_STEREO16, shortBuffer, SAMPLE_BUFFER_LIMIT * sizeof(short), 44100);
	checkAlError(__LINE__);
}

void checkAlError(int lineNum) {
	ALCenum error;

	error = alGetError();
	if (error != AL_NO_ERROR) {
		printf("Openal is in error state %d (line: %d)\n", error, lineNum);
		exit(1);
	}
}

void cleanupSoundInternal() {
	alDeleteSources(1, &alSource);
	alDeleteBuffers(2, alBuffers);
	alcMakeContextCurrent(NULL);
	alcDestroyContext(soundContext);
	alcCloseDevice(soundDevice);
}
