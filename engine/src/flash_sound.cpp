#include "sound.h"

extern "C" int getFlashSoundBuffer();

void initSoundPlat() {
inline_as3(
	"import flash.media.*;"
	"import flash.events.*;"
	"Console.globalSound = new Sound();"
	"Console.globalSound.addEventListener(SampleDataEvent.SAMPLE_DATA, Console.sampleGlobal);"
	"Console.globalChannel = Console.globalSound.play();"
	::
	);
}

void updateSound() {
}

void cleanupSoundInternal() {
}

void disableSound() {
	inline_as3("Console.globalChannel.stop();"::);
}

void enableSound() {
	inline_as3("Console.globalChannel = Console.globalSound.play();"::);
}

int getFlashSoundBuffer() {
	mixSound();
	return (int)&engine->soundData.sampleBuffer;
}
