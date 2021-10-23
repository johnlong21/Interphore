// #define MMX_IMPLEMENTATION
// #include "../lib/mm_vec.h"

#include "matrix.h"
#include <stdlib.h>

/*
a, b, c
d, e, f
g, h, i
*/

void inline matrixMultiplyByArray(Matrix *mat, float *array);

Matrix *matrixCreate() {
	Matrix *mat = (Matrix *)Malloc(sizeof(Matrix));
	matrixIdentity(mat);
	return mat;
}

void inline matrixIdentity(Matrix *mat) {
	matrixSetTo(mat,
		1, 0, 0,
		0, 1, 0,
		0, 0, 1
	);
}

void inline matrixSetToArray(Matrix *mat, float *array) {
	matrixSetTo(mat, array[0], array[1], array[2], array[3], array[4], array[5], array[6], array[7], array[8]);
}

void inline matrixSetTo(Matrix *mat, float a, float b, float c, float d, float e, float f, float g, float h, float i) {
	mat->data[0] = a;
	mat->data[1] = b;
	mat->data[2] = c;
	mat->data[3] = d;
	mat->data[4] = e;
	mat->data[5] = f;
	mat->data[6] = g;
	mat->data[7] = h;
	mat->data[8] = i;
}

void inline matrixProject(Matrix *mat, float width, float height) {
	float array[9] = {
		2/width, 0, 0,
		0, -2/height, 0,
		-1, 1, 1
	};
	matrixMultiplyByArray(mat, array);
}


void inline matrixTranslate(Matrix *mat, float x, float y) {
	float array[9] = {
		1, 0, 0,
		0, 1, 0,
		x, y, 1
	};
	matrixMultiplyByArray(mat, array);
}

void inline matrixRotate(Matrix *mat, float degrees) {
	float s = sin(degrees*M_PI/180);
	float c = cos(degrees*M_PI/180);
	float array[9] = {
		c, -s, 0,
		s,  c, 0,
		0,  0, 1
	};
	matrixMultiplyByArray(mat, array);
}

void inline matrixScale(Matrix *mat, float x, float y) {
	float array[9] = {
		x, 0, 0,
		0, y, 0,
		0, 0, 1
	};
	matrixMultiplyByArray(mat, array);
}

void inline matrixSkew(Matrix *mat, float x, float y) {
	float array[9] = {
		1             , (float)tan(y) , 0 ,
		(float)tan(x) , 1             , 0 ,
		0             , 0             , 1
	};
	matrixMultiplyByArray(mat, array);
}

void inline matrixInvert(Matrix *mat) {
	double det =
		mat->data[0] * (mat->data[4] * mat->data[8] - mat->data[7] * mat->data[5]) -
		mat->data[1] * (mat->data[3] * mat->data[8] - mat->data[5] * mat->data[6]) +
		mat->data[2] * (mat->data[3] * mat->data[7] - mat->data[4] * mat->data[6]);

	double invdet = 1 / det;

	float temp[9] = {};
	temp[0] = (mat->data[4] * mat->data[8] - mat->data[7] * mat->data[5]) * invdet;
	temp[1] = (mat->data[2] * mat->data[7] - mat->data[1] * mat->data[8]) * invdet;
	temp[2] = (mat->data[1] * mat->data[5] - mat->data[2] * mat->data[4]) * invdet;
	temp[3] = (mat->data[5] * mat->data[6] - mat->data[3] * mat->data[8]) * invdet;
	temp[4] = (mat->data[0] * mat->data[8] - mat->data[2] * mat->data[6]) * invdet;
	temp[5] = (mat->data[3] * mat->data[2] - mat->data[0] * mat->data[5]) * invdet;
	temp[6] = (mat->data[3] * mat->data[7] - mat->data[6] * mat->data[4]) * invdet;
	temp[7] = (mat->data[6] * mat->data[1] - mat->data[0] * mat->data[7]) * invdet;
	temp[8] = (mat->data[0] * mat->data[4] - mat->data[3] * mat->data[1]) * invdet;

	matrixSetToArray(mat, temp);
}

bool inline matrixEqual(Matrix *mat1, Matrix *mat2) {
	if (mat1->data[0] != mat2->data[0]) return false;
	if (mat1->data[1] != mat2->data[1]) return false;
	if (mat1->data[2] != mat2->data[2]) return false;
	if (mat1->data[3] != mat2->data[3]) return false;
	if (mat1->data[4] != mat2->data[4]) return false;
	if (mat1->data[5] != mat2->data[5]) return false;
	if (mat1->data[6] != mat2->data[6]) return false;
	if (mat1->data[7] != mat2->data[7]) return false;
	if (mat1->data[8] != mat2->data[8]) return false;
	return true;
}

void inline matrixMultiplyPoint(Matrix *mat, Point *point) {
	Point newPoint = {};
	newPoint.x = mat->data[0]*point->x + mat->data[3]*point->y + mat->data[6];
	newPoint.y = mat->data[1]*point->x + mat->data[4]*point->y + mat->data[7];
	float w =    mat->data[2]*point->x + mat->data[5]*point->y + mat->data[8];

	point->x = newPoint.x/w;
	point->y = newPoint.y/w;
}

void inline matrixMultiplyByArray(Matrix *mat, float *array) {
	float temp[9] = {};

	temp[0] += mat->data[0] * array[0] + mat->data[3] * array[1] + mat->data[6] * array[2];
	temp[1] += mat->data[1] * array[0] + mat->data[4] * array[1] + mat->data[7] * array[2];
	temp[2] += mat->data[2] * array[0] + mat->data[5] * array[1] + mat->data[8] * array[2];
	temp[3] += mat->data[0] * array[3] + mat->data[3] * array[4] + mat->data[6] * array[5];
	temp[4] += mat->data[1] * array[3] + mat->data[4] * array[4] + mat->data[7] * array[5];
	temp[5] += mat->data[2] * array[3] + mat->data[5] * array[4] + mat->data[8] * array[5];
	temp[6] += mat->data[0] * array[6] + mat->data[3] * array[7] + mat->data[6] * array[8];
	temp[7] += mat->data[1] * array[6] + mat->data[4] * array[7] + mat->data[7] * array[8];
	temp[8] += mat->data[2] * array[6] + mat->data[5] * array[7] + mat->data[8] * array[8];

	matrixSetToArray(mat, temp);
}

void inline matrixMultiply(Matrix *a, Matrix *b) {
	matrixMultiplyByArray(a, b->data);
}

void inline matrixPrint(Matrix *mat) {
	printf(
		"%0.1f %0.1f %0.1f\n%0.1f %0.1f %0.1f\n%0.1f %0.1f %0.1f\n",
		mat->data[0],
		mat->data[1],
		mat->data[2],
		mat->data[3],
		mat->data[4],
		mat->data[5],
		mat->data[6],
		mat->data[7],
		mat->data[8]
	);
}
