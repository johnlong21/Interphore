#include "defines.h"
#include "platform.h"

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

extern Profiler *profiler;

void initProfiler(Profiler *profilerInstance);
