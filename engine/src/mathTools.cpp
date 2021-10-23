#include "mathTools.h"

#define Clamp(x, min, max) ((x) < (min) ? (min) : (x) > (max) ? (max) : (x))
#define Max(x, y) ((x)>(y)?(x):(y))
#define Min(x, y) ((x)<(y)?(x):(y))

#define GetRgbaPixel(data, imgWidth, x, y) argbToHex((data)[((y)*(imgWidth)+(x))*4 + 3], (data)[((y)*(imgWidth)+(x))*4 + 0], (data)[((y)*(imgWidth)+(x))*4 + 1], (data)[((y)*(imgWidth)+(x))*4 + 2])

#ifndef M_PI_2
# define M_PI_2 ((M_PI)*0.5)
#endif

float mathClamp(float num, float min, float max) {
	return
		num < min ? min :
		num > max ? max :
		num;
}

float mathMap(float value, float sourceMin, float sourceMax, float destMin, float destMax, Ease ease) {
	float perc = mathNorm(value, sourceMin, sourceMax);
	perc = tweenEase(perc, ease);
	return mathLerp(perc, destMin, destMax);
}

float mathClampMap(float value, float sourceMin, float sourceMax, float destMin, float destMax, Ease ease) {
	float perc = mathNorm(value, sourceMin, sourceMax);
	perc = Clamp(perc, 0, 1);
	perc = tweenEase(perc, ease);
	return mathLerp(perc, destMin, destMax);
}

float mathNorm(float value, float min, float max) {
	return (value-min)/(max-min);
}

float mathLerp(float perc, float min, float max) {
	return min + (max - min) * perc;
}

float getScaleRatio(float curWidth, float curHeight, float targetWidth, float targetHeight) {
	float rx = targetWidth / curWidth;
	float ry = targetHeight / curHeight;
	return fmin(rx, ry);
}

void mathLerpPoint(float perc, Point *min, Point *max, Point *returnPoint) {
	returnPoint->x = mathLerp(perc, min->x, max->x);
	returnPoint->y = mathLerp(perc, min->y, max->y);
}

void mathLerpRect(float perc, Rect *min, Rect *max, Rect *returnRect) {
	returnRect->x = mathLerp(perc, min->x, max->x);
	returnRect->y = mathLerp(perc, min->y, max->y);
	returnRect->width = mathLerp(perc, min->width, max->width);
	returnRect->height = mathLerp(perc, min->height, max->height);
}

float getBezierPoint(float perc, float n1, float n2) {
	float diff = n2 - n1;
	return n1 + (diff * perc);
}    

float cubicBezier(float t, float x2, float y2, float x3, float y3) {
	t = Clamp(t, 0, 1);

	float x1 = 0;
	float y1 = 0;
	float x4 = 1;
	float y4 = 1;

	// The Green Lines
	float xa = getBezierPoint(t, x1, x2);
	float ya = getBezierPoint(t, y1, y2);
	float xb = getBezierPoint(t, x2, x3);
	float yb = getBezierPoint(t, y2, y3);
	float xc = getBezierPoint(t, x3, x4);
	float yc = getBezierPoint(t, y3, y4);

	// The Blue Line
	float xm = getBezierPoint(t, xa, xb);
	float ym = getBezierPoint(t, ya, yb);
	float xn = getBezierPoint(t, xb, xc);
	float yn = getBezierPoint(t, yb, yc);

	// The Black Dot
	float x = getBezierPoint(t, xm, xn);
	float y = getBezierPoint(t, ym, yn);

	return y;

	// Dunno what this does lol
	// float s = 1.0 - t;
	// float t2 = t*t;
	// float s2 = s*s;
	// float t3 = t2*t;
	// return (3.0 * B * s2 * t) + (3.0 * C * s * t2) + (t3);
}

float bezierTween(float t, float min, float max, float points[4]) {
	return bezierTween(t, min, max, points[0], points[1], points[2], points[3]);
}

float bezierTween(float t, float min, float max, float x2, float y2, float x3, float y3) {
	return mathLerp(cubicBezier(t, x2, y2, x3, y3), min, max);
}

void mathMoveTowards(Point *point, Point *loc, float dist) {
	float followAngle = atan2(point->y - loc->y, point->x - loc->x);
	float moveDistX = -cos(followAngle)*dist;
	float moveDistY = -sin(followAngle)*dist;

	point->x += moveDistX;
	point->y += moveDistY;
}

int mathAtoi(const char *str) {
	int val = 0;
	while(*str)
		val = val*10 + (*str++ - '0');
	return val;
}

void hexToArgb(int argbHex, int *a, int *r, int *g, int *b) {
	*a = (argbHex >> 24) & 0xFF;
	*r = (argbHex >> 16) & 0xFF;
	*g = (argbHex >> 8) & 0xFF;
	*b = (argbHex     ) & 0xFF;
}

int argbToHex(unsigned char a, unsigned char r, unsigned char g, unsigned char b) {
	return ((a & 0xff) << 24) + ((r & 0xff) << 16) + ((g & 0xff) << 8) + (b & 0xff);
}

void hexToRgba(int rgbaHex, int *r, int *g, int *b, int *a) {
	*r = (rgbaHex >> 24) & 0xFF;
	*g = (rgbaHex >> 16) & 0xFF;
	*b = (rgbaHex >> 8) & 0xFF;
	*a = (rgbaHex     ) & 0xFF;
}

int rgbaToHex(char r, char g, char b, char a) {
	return ((r & 0xff) << 24) + ((g & 0xff) << 16) + ((b & 0xff) << 8) + (a & 0xff);
}

float mathWiggle(float baseNumber, float time, float secondsPerIter, float totalIters, float percentDeviate, Ease ease) {
	float totalTime = totalIters*secondsPerIter;

	if (time < totalTime) {
		float wigglePerc = time/totalTime;
		wigglePerc = tweenEase(wigglePerc, ease);
		float wiggleTime = wigglePerc*totalTime;

		float iterPerc = wiggleTime;
		while (iterPerc > secondsPerIter*2) iterPerc -= secondsPerIter*2;
		iterPerc /= secondsPerIter;
		iterPerc = sin(iterPerc*M_PI*2.0);
		return baseNumber + baseNumber*iterPerc*percentDeviate;
	}

	return baseNumber;
}

float mathTween(float baseNumber, float endNumber, float time, float endTime, Ease ease) {
	if (time > endTime) return endNumber;
	float perc = time/endTime;
	perc = tweenEase(perc, ease);
	return mathLerp(perc, baseNumber, endNumber);
}

float tweenEase(float p, Ease ease) {
	if (ease == LINEAR) {
		return p;
	} else if (ease == QUAD_IN) {
		return p * p;
	} else if (ease == QUAD_OUT) {
		return -(p * (p - 2));
	} else if (ease == QUAD_IN_OUT) {
		if (p < 0.5) return 2 * p * p;
		else return (-2 * p * p) + (4 * p) - 1;
	} else if (ease == CUBIC_IN) {
		return p * p * p;
	} else if (ease == CUBIC_OUT) {
		float f = (p - 1);
		return f * f * f + 1;
	} else if (ease == CUBIC_IN_OUT) {
		float f = ((2 * p) - 2);
		if (p < 0.5) return 4 * p * p * p;
		else return 0.5 * f * f * f + 1;
	} else if (ease == QUART_IN) {
		return p * p * p * p;
	} else if (ease == QUART_OUT) {
		float f = (p - 1);
		return f * f * f * (1 - p) + 1;
	} else if (ease == QUART_IN_OUT) {
		float f = (p - 1);
		if (p < 0.5) return 8 * p * p * p * p;
		else return -8 * f * f * f * f + 1;
	} else if (ease == QUINT_IN) {
		return p * p * p * p * p;
	} else if (ease == QUINT_OUT) {
		float f = (p - 1);
		return f * f * f * f * f + 1;
	} else if (ease == QUINT_IN_OUT) {
		float f = ((2 * p) - 2);
		if (p < 0.5) return 16 * p * p * p * p * p;
		else return  0.5 * f * f * f * f * f + 1;
	} else if (ease == SINE_IN) {
		return sin((p - 1) * M_PI_2) + 1;
	} else if (ease == SINE_OUT) {
		return sin(p * M_PI_2);
	} else if (ease == SINE_IN_OUT) {
		return 0.5 * (1 - cos(p * M_PI));
	} else if (ease == CIRC_IN) {
		return 1 - sqrt(1 - (p * p));
	} else if (ease == CIRC_OUT) {
		return sqrt((2 - p) * p);
	} else if (ease == CIRC_IN_OUT) {
		if (p < 0.5) return 0.5 * (1 - sqrt(1 - 4 * (p * p)));
		else return 0.5 * (sqrt(-((2 * p) - 3) * ((2 * p) - 1)) + 1);
	} else if (ease == EXP_IN) {
		return (p == 0.0) ? p : pow(2, 10 * (p - 1));
	} else if (ease == EXP_OUT) {
		return (p == 1.0) ? p : 1 - pow(2, -10 * p);
	} else if (ease == EXP_IN_OUT) {
		if (p == 0.0 || p == 1.0) return p;
		if (p < 0.5) return 0.5 * pow(2, (20 * p) - 10);
		else return -0.5 * pow(2, (-20 * p) + 10) + 1;
	} else if (ease == ELASTIC_IN) {
		return sin(13 * M_PI_2 * p) * pow(2, 10 * (p - 1));
	} else if (ease == ELASTIC_OUT) {
		return sin(-13 * M_PI_2 * (p + 1)) * pow(2, -10 * p) + 1;
	} else if (ease == ELASTIC_IN_OUT) {
		if (p < 0.5) return 0.5 * sin(13 * M_PI_2 * (2 * p)) * pow(2, 10 * ((2 * p) - 1));
		else return 0.5 * (sin(-13 * M_PI_2 * ((2 * p - 1) + 1)) * pow(2, -10 * (2 * p - 1)) + 2);
	} else if (ease == BACK_IN) {
		return p * p * p - p * sin(p * M_PI);
	} else if (ease == BACK_OUT) {
		float f = (1 - p);
		return 1 - (f * f * f - f * sin(f * M_PI));
	} else if (ease == BACK_IN_OUT) {
		if (p < 0.5) {
			float f = 2 * p;
			return 0.5 * (f * f * f - f * sin(f * M_PI));
		} else {
			float f = (1 - (2*p - 1));
			return 0.5 * (1 - (f * f * f - f * sin(f * M_PI))) + 0.5;
		}
	} else if (ease == BOUNCE_IN) {
		return 1 - tweenEase(1 - p, BOUNCE_OUT);
	} else if (ease == BOUNCE_OUT) {
		if (p < 4/11.0) return (121 * p * p)/16.0;
		else if (p < 8/11.0) return (363/40.0 * p * p) - (99/10.0 * p) + 17/5.0;
		else if (p < 9/10.0) return (4356/361.0 * p * p) - (35442/1805.0 * p) + 16061/1805.0;
		else return (54/5.0 * p * p) - (513/25.0 * p) + 268/25.0;
	} else if (ease == BOUNCE_IN_OUT) {
		if (p < 0.5) return 0.5 * tweenEase(p*2, BOUNCE_IN);
		else return 0.5 * tweenEase(p * 2 - 1, BOUNCE_OUT) + 0.5;
	}

	return 0;
}

