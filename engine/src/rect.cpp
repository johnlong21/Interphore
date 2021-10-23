#include "rect.h"

void Rect::setTo(float x, float y, float width, float height) {
	this->x = x;
	this->y = y;
	this->width = width;
	this->height = height;
}

void Rect::inflateFromCenter(float x, float y) {
	this->x -= x;
	this->y -= y;
	this->width += x*2;
	this->height += y*2;
}

bool rectContainsPoint(Rect *rect, Point *point) { return rectContainsPoint(rect->x, rect->y, rect->width, rect->height, point->x, point->y); }
bool rectContainsPoint(Rect *rect, float px, float py) { return rectContainsPoint(rect->x, rect->y, rect->width, rect->height, px, py); }
bool rectContainsPoint(float rx, float ry, float rw, float rh, float px, float py) {
	return px >= rx && px <= rx+(rw-1) && py >= ry && py <= ry+(rh-1);
}

bool rectIntersectRect(Rect *r1, Rect *r2) { return rectIntersectRectRaw(r1->x, r1->y, r1->width, r1->height, r2->x, r2->y, r2->width, r2->height); }
bool rectIntersectRectRaw(float r1x, float r1y, float r1w, float r1h, float r2x, float r2y, float r2w, float r2h) {
	return r1x < r2x + r2w &&
		r1x + r1w > r2x &&
		r1y < r2y + r2h &&
		r1h + r1y > r2y;

	bool intercects = !(
		r2x > r1x+r1w ||
		r2x+r2w < r1x ||
		r2y > r1y+r1h ||
		r2y+r2h < r1y);

	bool contains = 
		(r1x+r1w) < (r1x+r1w) &&
		(r2x) > (r1x) &&
		(r2y) > (r1y) &&
		(r2y+r2h) < (r1y+r1h); //@cleanup dunno about this contains thing

	return intercects || contains;


	// return r1x < r2x + r2w && r1x + r1w > r2x && r1y < r2y + r2y && r1y + r1h > r2y;
}

Point Rect::getCenterPoint() {
	Rect *rect = this;

	Point p;
	p.x = rect->x + rect->width/2.0;
	p.y = rect->y + rect->height/2.0;
	return p;
}

bool Rect::equals(Rect *other) {
	return other->x == this->x && other->y == this->y && other->width == this->width && other->height == this->height;
}

bool Rect::isZero() {
	return this->x == 0 && this->y == 0 && this->width == 0 && this->height == 0;
}

void Rect::add(Rect *other) {
	this->x += other->x;
	this->y += other->y;
	this->width += other->width;
	this->height += other->height;
}

void Rect::subtract(Rect *other) {
	this->x -= other->x;
	this->y -= other->y;
	this->width -= other->width;
	this->height -= other->height;
}

void Rect::multiply(Rect *other) {
	this->x *= other->x;
	this->y *= other->y;
	this->width *= other->width;
	this->height *= other->height;
}

void Rect::divide(Rect *other) {
	this->x /= other->x;
	this->y /= other->y;
	this->width /= other->width;
	this->height /= other->height;
}

void Rect::add(float n) {
	this->x += n;
	this->y += n;
	this->width += n;
	this->height += n;
}

void Rect::subtract(float n) {
	this->x -= n;
	this->y -= n;
	this->width -= n;
	this->height -= n;
}

void Rect::multiply(float n) {
	this->x *= n;
	this->y *= n;
	this->width *= n;
	this->height *= n;
}

void Rect::divide(float n) {
	this->x /= n;
	this->y /= n;
	this->width /= n;
	this->height /= n;
}

void Rect::collideWith(Rect *wall) {
	Rect *rect = this;

#if 1
	while (rectContainsPoint(rect, wall->x, wall->y + wall->height/2.0)) rect->x--;
	while (rectContainsPoint(rect, wall->x + wall->width, wall->y + wall->height/2.0)) rect->x++;
	while (rectContainsPoint(rect, wall->x + wall->width/2.0, wall->y)) rect->y--;
	while (rectContainsPoint(rect, wall->x + wall->width/2.0, wall->y + wall->height)) rect->y++;
#else
	bool inWall = true;
	while (inWall) {
		inWall = false;

		float step = 1;
		if (rectIntersectRect(rect, wall)) {
			inWall = true;
			rect->x += 1;
			if (rect->x + rect->width/2 <= wall->x + wall->width/2) rect->x -= step;
			if (rect->x + rect->width/2 >= wall->x + wall->width/2) rect->x += step;
			if (rect->y + rect->height/2 <= wall->y + wall->height/2) rect->y -= step;
			if (rect->y + rect->height/2 >= wall->y + wall->height/2) rect->y += step;
		}
	}
#endif
}
