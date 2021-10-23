#include "sound.h"

float computeChannelVolume(Channel *channel);

void initSound() {
	mixSound();
	initSoundPlat();
	engine->soundData.masterVolume = 1;
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

Channel *playSound(const char *assetId, const char *channelName, bool looping) {
	Channel *channel = NULL;

	for (int i = 0; i < engine->soundData.channelListNum; i++) {
		if (!engine->soundData.channelList[i].exists) {
			channel = &engine->soundData.channelList[i];
			channel->id = i;
			break;
		}
	}

	if (!channel) {
		channel = &engine->soundData.channelList[engine->soundData.channelListNum++];
		channel->id = engine->soundData.channelListNum-1;
	}

	channel->looping = looping;
	channel->userVolume = 1;
	channel->speed = 1;
	channel->playing = true;
	channel->exists = true;
	if (channelName) strcpy(channel->name, channelName);

	Asset *assetList[ASSET_LIMIT];
	int assetListNum;

	getMatchingAssets(assetId, OGG_ASSET, assetList, &assetListNum);
	Assert(assetListNum);
	assetId = assetList[rndInt(0, assetListNum-1)]->name;
	channel->assetId = assetId;

	Asset *oggAsset = getAsset(assetId, OGG_ASSET);
	Assert(oggAsset);

	channel->vorbisTempMem.alloc_buffer = (char *)Malloc(Kilobytes(500));
	channel->vorbisTempMem.alloc_buffer_length_in_bytes = Kilobytes(500);
	int err;
	channel->vorbis = stb_vorbis_open_memory((unsigned char *)oggAsset->data, oggAsset->dataLen, &err, &channel->vorbisTempMem);

	if (err != 0) {
		printf("Stb vorbis failed with error %d\n", err);
		exit(1);
	}

	channel->length = stb_vorbis_stream_length_in_seconds(channel->vorbis);

	channel->tweakVolume = 1;
	for (int i = 0; i < ASSET_LIMIT; i++) {
		SoundTweak *curTweak = &engine->soundData.tweaks[i];
		if (curTweak->assetId == assetId) {
			channel->tweakVolume = curTweak->volume;
			break;
		}
	}

	channel->lastVol = computeChannelVolume(channel);

	return channel;
}

float computeChannelVolume(Channel *channel) {
	return channel->tweakVolume * channel->userVolume * engine->soundData.masterVolume;
}

void mixSound() {
	SoundData *soundData = &engine->soundData;

	memset(soundData->sampleBuffer, 0, SAMPLE_BUFFER_LIMIT*sizeof(float));

	for (int i = 0; i < soundData->channelListNum; i++) {
		Channel *c = &soundData->channelList[i];
		if (!c->exists || !c->playing) continue;
		c->userVolume = mathClamp(c->userVolume, 0, 1);

		float vol = computeChannelVolume(c);
		float startVol = vol;
		float volAdd = 0;

		if (vol != c->lastVol) {
			float minVol = c->lastVol;
			float maxVol = vol;
			startVol = minVol;
			volAdd = (maxVol-minVol) / SAMPLE_BUFFER_LIMIT;
		}

		c->lastVol = vol;

		float samples[SAMPLE_BUFFER_LIMIT] = {};
		int samplesNum = 0;
		float tempSamples[SAMPLE_BUFFER_LIMIT] = {};
		int tempSamplesNum = 0;
		bool soundOver = false;

		if (c->speed < 0.1) c->speed = 0.1;
		if (c->speed == 1) {
			while (samplesNum < SAMPLE_BUFFER_LIMIT) {
				tempSamplesNum = stb_vorbis_get_samples_float_interleaved(c->vorbis, 2, tempSamples, SAMPLE_BUFFER_LIMIT-samplesNum)*2;

				for (int i = 0; i < tempSamplesNum; i+=2) {
					samples[samplesNum] = tempSamples[i];
					samples[samplesNum+1] = tempSamples[i+1];
					samplesNum += 2;
				}

				if (tempSamplesNum == 0) {
					if (!c->looping) {
						soundOver = true;
						break;
					}
					stb_vorbis_seek_start(c->vorbis);
				}
			}
		}

		if (c->speed < 1) {
			int multi = 1/c->speed;
			if (multi % 2 != 0 && multi != 1) multi--;
			while (samplesNum < SAMPLE_BUFFER_LIMIT) {
				tempSamplesNum = stb_vorbis_get_samples_float_interleaved(c->vorbis, 2, tempSamples, ceil((SAMPLE_BUFFER_LIMIT-samplesNum)/(float)multi))*2;
				for (int i = 0; i < tempSamplesNum; i+=2) {
					for (int j = 0; j < multi; j++) {
						samples[samplesNum] = tempSamples[i];
						samples[samplesNum+1] = tempSamples[i+1];
						samplesNum += 2;
					}
				}

				if (tempSamplesNum == 0) {
					if (!c->looping) {
						soundOver = true;
						break;
					}
					stb_vorbis_seek_start(c->vorbis);
				}
			}
		}

		if (c->speed > 1) {
			while (samplesNum < SAMPLE_BUFFER_LIMIT) {
				tempSamplesNum = stb_vorbis_get_samples_float_interleaved(c->vorbis, 2, tempSamples, SAMPLE_BUFFER_LIMIT-samplesNum)*2;

				int curSample = 0;
				for (;;) {
					if (curSample > tempSamplesNum-1) break;
					samples[samplesNum] = tempSamples[curSample];
					samples[samplesNum+1] = tempSamples[curSample+1];
					samplesNum += 2;
					curSample += 2*c->speed;
				}

				if (tempSamplesNum == 0) {
					if (!c->looping) {
						soundOver = true;
						break;
					}
					stb_vorbis_seek_start(c->vorbis);
				}
			}
		}

		for (int j = 0; j < SAMPLE_BUFFER_LIMIT; j+=2) {
			float curVol = startVol + volAdd*j;
			soundData->sampleBuffer[j] += samples[j] * curVol;
			soundData->sampleBuffer[j+1] += samples[j+1] * curVol;
		}

		c->perc = (float)c->vorbis->current_loc/c->vorbis->total_samples;
		if (soundOver) c->destroy();
	}
	for (int i = 0; i < SAMPLE_BUFFER_LIMIT; i++) soundData->sampleBuffer[i] = Clamp(soundData->sampleBuffer[i], -1, 1);
}

void Channel::destroy() {
	Channel *channel = this;
	stb_vorbis_close(channel->vorbis);
	Free(channel->vorbisTempMem.alloc_buffer);
	channel->playing = false;
	channel->exists = false;
}

void cleanupSound() {
	for (int i = 0; i < engine->soundData.channelListNum; i++) {
		Channel *c = &engine->soundData.channelList[i];
		if (c->exists) c->destroy();
	}

	cleanupSoundInternal();
}

Channel *getChannel(const char *channelName) {
	for (int i = 0; i < engine->soundData.channelListNum; i++) {
		Channel *c = &engine->soundData.channelList[i];
		if (!c->exists) continue;
		if (streq(c->name, channelName)) return c;
	}

	return NULL;
}
