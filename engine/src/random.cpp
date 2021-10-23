#include "random.h"
#include <stdlib.h>

inline float rnd() {
	return (float)rand()/(float)RAND_MAX;
}

inline float rndFloat(float min, float max) {
	return min + rnd() * (max - min);
}

inline int rndInt(int min, int max) {
	return round((rndFloat(min, max)));
}

inline bool rndBool() {
	return rndInt(0, 1);
}
