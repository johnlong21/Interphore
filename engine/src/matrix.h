#pragma once
#include "point.h"


struct Matrix {
	float data[9];
};

Matrix *matrixCreate();
void inline matrixIdentity(Matrix *mat);
void inline matrixMultiply(Matrix *a, Matrix *b);
void inline matrixSetToArray(Matrix *mat, float *array);
void inline matrixSetTo(Matrix *mat, float a, float b, float c, float d, float e, float f, float g, float h, float i);
void inline matrixProject(Matrix *mat, float width, float height);
void inline matrixTranslate(Matrix *mat, float x, float y);
void inline matrixScale(Matrix *mat, float x, float y);
void inline matrixSkew(Matrix *mat, float x, float y);
void inline matrixRotate(Matrix *mat, float degrees);
bool inline matrixEqual(Matrix *mat1, Matrix *mat2);
void inline matrixPrint(Matrix *mat);
void inline matrixMultiplyPoint(Matrix *mat, Point *point);
void inline matrixInvert(Matrix *mat);
