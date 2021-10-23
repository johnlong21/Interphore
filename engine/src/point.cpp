#include "point.h"

void pointSetTo(Point *point, float x, float y) {
	point->x = x;
	point->y = y;
}

void pointRotate(Point *p, float cx, float cy, float degrees) {
	float rads = degrees * (M_PI/180);
	float s = sin(rads);
	float c = cos(rads);

	// translate point back to origin:
	p->x -= cx;
	p->y -= cy;

	// rotate point
	float xnew = p->x*c - p->y*s;
	float ynew = p->x*s + p->y*c;

	// translate point back:
	p->x = xnew + cx;
	p->y = ynew + cy;
}

float pointDistance(Point *p1, Point *p2) { return pointDistance(p1->x, p1->y, p2->x, p2->y); }
float pointDistance(float x1, float y1, float x2, float y2) { return sqrt((pow(x2-x1, 2))+(pow(y2-y1, 2))); }
float pointDistanceRaw(float x1, float y1, float x2, float y2) { return pointDistance(x1, y1, x2, y2); }

bool Point::isZero() {
	if (this->x == 0 && this->y == 0) return false;
	return true;
}

void Point::zero() {
	this->x = this->y = 0;
}

bool Point::equals(Point *other) {
	return this->x == other->x && this->y == other->y;
}

void Point::multiply(double value) {
	this->x *= value;
	this->y *= value;
}
