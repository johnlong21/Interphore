#pragma once

int pathfinderMap[TILES_WIDE_LIMIT * TILES_HIGH_LIMIT];
int pathfinderMapWidth = 0;
int pathfinderMapHeight = 0;

void pathfinderFind(int startX, int startY, int endX, int endY, Point *result, int *resultLen);
bool pathfinderGetLos(int x1, int y1, int x2, int y2);
void pathfinderRasterizeLine(int x1, int y1, int x2, int y2, Point *result, int *resultLen);

