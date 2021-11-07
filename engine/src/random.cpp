#include <stdlib.h>

#include "defines.h"
#include "random.h"

float rnd() {
	return (float)rand()/(float)RAND_MAX;
}

float rndFloat(float min, float max) {
	return min + rnd() * (max - min);
}

int rndInt(int min, int max) {
	return round((rndFloat(min, max)));
}

bool rndBool() {
	return rndInt(0, 1);
}
