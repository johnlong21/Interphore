#pragma once

enum Dir8 {
	DIR8_CENTER = 0,
	DIR8_LEFT,
	DIR8_RIGHT,
	DIR8_UP,
	DIR8_DOWN,
	DIR8_UP_LEFT,
	DIR8_UP_RIGHT,
	DIR8_DOWN_LEFT,
	DIR8_DOWN_RIGHT
};

enum Ease {
	LINEAR = 0,
	QUAD_IN, QUAD_OUT, QUAD_IN_OUT,
	CUBIC_IN, CUBIC_OUT, CUBIC_IN_OUT,
	QUART_IN, QUART_OUT, QUART_IN_OUT,
	QUINT_IN, QUINT_OUT, QUINT_IN_OUT,
	SINE_IN, SINE_OUT, SINE_IN_OUT,
	CIRC_IN, CIRC_OUT, CIRC_IN_OUT,
	EXP_IN, EXP_OUT, EXP_IN_OUT,
	ELASTIC_IN, ELASTIC_OUT, ELASTIC_IN_OUT,
	BACK_IN, BACK_OUT, BACK_IN_OUT,
	BOUNCE_IN, BOUNCE_OUT, BOUNCE_IN_OUT
};

float mathClamp(float num, float min=0, float max=1);
void mathMoveTowards(Point *point, Point *loc, float dist);

float mathMap(float value, float sourceMin, float sourceMax, float destMin, float destMax, Ease ease=LINEAR);
float mathClampMap(float value, float sourceMin, float sourceMax, float destMin, float destMax, Ease ease=LINEAR);
float mathNorm(float value, float min, float max);

float mathLerp(float perc, float min, float max);
void mathLerpPoint(float perc, Point *min, Point *max, Point *returnPoint);
void mathLerpRect(float perc, Rect *min, Rect *max, Rect *returnRect);
float getScaleRatio(float curWidth, float curHeight, float targetWidth, float targetHeight);

float getBezierPoint(float perc, float n1, float n2);
float cubicBezier(float t, float x2, float y2, float x3, float y3);
float bezierTween(float t, float min, float max, float points[4]);
float bezierTween(float t, float min, float max, float x2, float y2, float x3, float y3);

int mathAtoi(const char *str);

void hexToArgb(int argbHex, int *a, int *r, int *g, int *b); //@todo Make these unsigned char
int argbToHex(unsigned char a, unsigned char r, unsigned char g, unsigned char b);

void hexToRgba(int rgbaHex, int *r, int *g, int *b, int *a);
int rgbaToHex(char r, char g, char b, char a);

float mathWiggle(float baseNumber, float time, float secondsPerIter, float totalIters, float percentDeviate, Ease ease);
float mathTween(float baseNumber, float endNumber, float time, float endTime, Ease ease);
float tweenEase(float p, Ease ease);
