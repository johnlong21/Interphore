#pragma once

#include "defines.hpp"
#include "point.hpp"

extern int pathfinderMap[TILES_WIDE_LIMIT * TILES_HIGH_LIMIT];
extern int pathfinderMapWidth;
extern int pathfinderMapHeight;

void pathfinderFind(int startX, int startY, int endX, int endY, Point *result, int *resultLen);
bool pathfinderGetLos(int x1, int y1, int x2, int y2);
void pathfinderRasterizeLine(int x1, int y1, int x2, int y2, Point *result, int *resultLen);

