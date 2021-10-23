#pragma once
#include "point.h"

struct Rect {
	float x;
	float y;
	float width;
	float height;

	void setTo(float x=0, float y=0, float width=0, float height=0);
	void inflateFromCenter(float x, float y);

	Point getCenterPoint();
	bool equals(Rect *other);
	bool isZero();

	void add(Rect *other);
	void subtract(Rect *other);
	void multiply(Rect *other);
	void divide(Rect *other);

	void add(float n);
	void subtract(float n);
	void multiply(float n);
	void divide(float n);

	void collideWith(Rect *wall);
};

bool rectContainsPoint(Rect *rect, Point *point);
bool rectContainsPoint(Rect *rect, float px, float py);
bool rectContainsPoint(float rx, float ry, float rw, float rh, float px, float py);

bool rectIntersectRectRaw(float r1x, float r1y, float r1w, float r1h, float r2x, float r2y, float r2w, float r2h);

bool rectIntersectRect(Rect *r1, Rect *r2);
float rectOverlapPercent(Rect *r1, Rect *r2);
