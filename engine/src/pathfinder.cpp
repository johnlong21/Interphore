#include "pathfinder.h"
#include "micropather.h"

struct PointKV {
	Point key;
	Point value;
};

PointKV cameFrom[TILES_WIDE_LIMIT * TILES_HIGH_LIMIT];
int cameFromNum = 0;

void cameFromSet(Point *key, Point *value);
Point *cameFromGet(Point *key);
void pathfinderGetNeighbors(Point *point, Point *result, int *resultNum);

void pathfinderFind(int startX, int startY, int endX, int endY, Point *result, int *resultLen) {
	*resultLen = 0;

	Point start;
	start.setTo(startX, startY);

	Point end;
	end.setTo(endX, endY);

	if (pathfinderMap[(int)(end.y * pathfinderMapWidth + end.x)] != 0) return; // You can't be there

	{ /// Build cameFrom
		cameFromNum = 0;
		Point frontier[TILES_WIDE_LIMIT * TILES_HIGH_LIMIT];
		int frontierNum = 0;
		frontier[frontierNum++].setTo(startX, startY);

		cameFromSet(&start, NULL);

		bool found = false;
		int depth = 0;
		while (frontierNum != 0 || found) {
			depth++;
			// printf("Frontier: %d\n", frontierNum);
			Point current = frontier[0];
			// for (int i = 1; i < frontierNum; i++) frontier[i-1] = frontier[i];
			memmove(frontier, frontier+1, frontierNum * sizeof(Point));
			frontierNum--;

			if (current.x == end.x && current.y == end.y) break;
			Point neighbors[4];
			int neighborsNum = 0;
			pathfinderGetNeighbors(&current, neighbors, &neighborsNum);
			// printf("Found %d neighbors\n", neighborsNum);
			for (int i = 0; i < neighborsNum; i++) {
				// printf("Doing %d\n", i);
				if (cameFromGet(&neighbors[i]) == NULL) {
					if (pointDistance(&current, &start) < PATHFIND_DISTANCE_LIMIT) {
						frontier[frontierNum++] = neighbors[i];
						cameFromSet(&neighbors[i], &current);
						if (current.equals(&end)) {
							found = true;
							break;
						}
					}
				}
			}
		}
	}

	Point *path = result;
	int pathNum = 0;
	{ /// Find Path
		Point current = end;
		path[pathNum++] = current;
		while (current.x != start.x || current.y != start.y) {
			Point *next = cameFromGet(&current);
			if (pathNum >= PATHFIND_PATH_LIMIT) {
				return;
			}
			if (next == NULL) return;
			path[pathNum++] = *next;
			current = *next;
		}
	}

	{ // Reverse
		int i = pathNum - 1;
		int j = 0;
		while(i > j) {
			Point temp = path[i];
			path[i] = path[j];
			path[j] = temp;
			i--;
			j++;
		}
	}

	*resultLen = pathNum;
}

bool pathfinderGetLos(int x1, int y1, int x2, int y2) {
	Point line[PATHFIND_PATH_LIMIT];
	int lineNum;
	pathfinderRasterizeLine(x1, y1, x2, y2, line, &lineNum);

	bool hasLos = true;
	for (int i = 0; i < lineNum; i++) {
		Point *p = &line[i];
		if (pathfinderMap[(int)(p->y * pathfinderMapWidth + p->x)] == 1) hasLos = false;
	}

	return hasLos;
}

void pathfinderRasterizeLine(int x1, int y1, int x2, int y2, Point *result, int *resultLen) {
	*resultLen = 0;

	Point *line = result;
	int lineNum = 0;

	int dx = abs(x2 - x1);
	int dy = abs(y2 - y1);
	int sx = x1<x2 ? 1 : -1;
	int sy = y1<y2 ? 1 : -1;
	int err = dx - dy;

	line[lineNum++].setTo(x1, y1);

	while (x1 != x2 || y1 != y2) {
		float e2 = err << 1;
		if (e2 > -dy) {
			err -= dy;
			x1 += sx;
		} else if (e2 < dx) {
			err += dx;
			y1 += sy;
		}

		line[lineNum++].setTo(x1, y1);
	}

	*resultLen = lineNum;
}

void pathfinderGetNeighbors(Point *point, Point *result, int *resultNum) {
	Point possible[4];
	possible[0].setTo(point->x-1, point->y);
	possible[1].setTo(point->x+1, point->y);
	possible[2].setTo(point->x, point->y-1);
	possible[3].setTo(point->x, point->y+1);

	for (int i = 0; i < 4; i++) {
		Point *p = &possible[i];
		bool canAdd = true;
		if (p->x < 0 || p->y < 0 || p->x >= pathfinderMapWidth || p->y >= pathfinderMapHeight) canAdd = false;
		// if (canAdd) printf("Map index: %d from point %0.0f %0.0f\n", mapIndex, p->x, p->y);
		if (canAdd && pathfinderMap[(int)(p->y * pathfinderMapWidth + p->x)] == 1) canAdd = false;

		if (canAdd) {
			result[*resultNum].setTo(p->x, p->y);
			*resultNum = *resultNum + 1;
		}
	}
}

void cameFromSet(Point *key, Point *value) {
	Point nullPoint;
	if (value == NULL) {
		nullPoint.setTo(-1, -1);
		value = &nullPoint;
	}

	// printf("Looking for %x\n", key);
	// printf("Looking for %0.0f %0.0f\n", key->x, key->y);

	bool found = false;
	for (int i = 0; i < cameFromNum; i++) {
		if (key->x == cameFrom[i].key.x && key->y == cameFrom[i].key.y) {
			cameFrom[i].value.setTo(value->x, value->y);
			// printf("Found %0.0f %0.0f\n", key->x, key->y);
			return;
		}
	}

	if (!found) {
		cameFrom[cameFromNum].key.setTo(key->x, key->y);
		cameFrom[cameFromNum].value.setTo(value->x, value->y);
		cameFromNum++;
		// printf("Came from at %d\n", cameFromNum);
	}
}

Point *cameFromGet(Point *key) {
	for (int i = 0; i < cameFromNum; i++)
		if (key->x == cameFrom[i].key.x && key->y == cameFrom[i].key.y)
			return &cameFrom[i].value;

	return NULL;
}
