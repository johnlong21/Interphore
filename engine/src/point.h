#pragma once

struct Point;

void pointSetTo(Point *point, float x, float y);
void pointRotate(Point *p, float cx, float cy, float degrees);
float pointDistance(Point *p1, Point *p2);
float pointDistance(float x1, float x2, float y1, float y2);
float pointDistanceRaw(float x1, float x2, float y1, float y2);

struct Point {
	float x;
	float y;

	void setTo(float x=0, float y=0) { pointSetTo(this, x, y); }
	void rotate(float cx, float cy, float degrees) { pointRotate(this, cx, cy, degrees); }

	bool isZero();
	void zero();
	bool equals(Point *other);

	void multiply(double value);
};

