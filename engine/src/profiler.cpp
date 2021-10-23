#define PROFILES_MAX 128
#define PROFILER_AVERAGE_FRAMES 30

struct Profile {
	bool exists;
	NanoTime startTime;
	NanoTime endTime;

	float pastMs[PROFILER_AVERAGE_FRAMES];
};

struct Profiler {
	Profile profiles[PROFILES_MAX];
	int profilesNum;

	void startProfile(int id);
	void endProfile(int id);
	float getMsResult(int id);
	float getAverage(int id);
	void getResult(int id, NanoTime *time);
	void printAll();
};

Profiler *profiler;

void initProfiler(Profiler *profilerInstance) {
	profiler = profilerInstance;
	memset(profiler, 0, sizeof(Profiler));
}

void Profiler::startProfile(int id) {
	NanoTime time;

	Profile *prof = &profiler->profiles[id];
	if (prof->exists) {
		for (int j = PROFILER_AVERAGE_FRAMES-1; j > 0; j--) prof->pastMs[j] = prof->pastMs[j-1];
		prof->pastMs[0] = profiler->getMsResult(id);
	} else {
		prof->exists = true;
	}

	getNanoTime(&prof->startTime);
}

void Profiler::endProfile(int id) {
	Profile *prof = &profiler->profiles[id];
	getNanoTime(&prof->endTime);
}

float Profiler::getAverage(int id) {
	Profile *prof = &profiler->profiles[id];
	Assert(prof->exists);

	float sum = 0;
	for (int i = 0; i < PROFILER_AVERAGE_FRAMES; i++) sum += prof->pastMs[i];
	sum /= PROFILER_AVERAGE_FRAMES;
	return sum;
}

float Profiler::getMsResult(int id) {
	NanoTime time;
	profiler->getResult(id, &time);

	float ms = time.seconds*1000 + time.nanos/1000000.0;
	return ms;
}

void Profiler::getResult(int id, NanoTime *time) {
	Profile *prof = &profiler->profiles[id];
	Assert(prof->exists);

	if (prof->endTime.nanos < prof->startTime.nanos) {
		time->seconds = prof->endTime.seconds - prof->startTime.seconds - 1;
		time->nanos = 1000000000 + prof->endTime.nanos - prof->startTime.nanos;
	} else {
		time->seconds = prof->endTime.seconds - prof->startTime.seconds;
		time->nanos = prof->endTime.nanos - prof->startTime.nanos;
	}
}

void Profiler::printAll() {
	for (int i = 0; i < PROFILES_MAX; i++) {
		Profile *prof = &profiler->profiles[i];
		if (!prof->exists) continue;

		printf("Profile: %d\nStart: %d, %d\n", i, prof->startTime.seconds, prof->startTime.nanos);
		if (prof->endTime.seconds != 0 || prof->endTime.nanos != 0) {
			printf("End: %d, %d\n", prof->endTime.seconds, prof->endTime.nanos);
		} else {
			printf("Not finished\n");
		}
	}
}
