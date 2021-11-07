#pragma once
#include "point.h"


struct Matrix {
	float data[9];
};

Matrix *matrixCreate();
void matrixIdentity(Matrix *mat);
void matrixMultiply(Matrix *a, Matrix *b);
void matrixSetToArray(Matrix *mat, float *array);
void matrixSetTo(Matrix *mat, float a, float b, float c, float d, float e, float f, float g, float h, float i);
void matrixProject(Matrix *mat, float width, float height);
void matrixTranslate(Matrix *mat, float x, float y);
void matrixScale(Matrix *mat, float x, float y);
void matrixSkew(Matrix *mat, float x, float y);
void matrixRotate(Matrix *mat, float degrees);
bool matrixEqual(Matrix *mat1, Matrix *mat2);
void matrixPrint(Matrix *mat);
void matrixMultiplyPoint(Matrix *mat, Point *point);
void matrixInvert(Matrix *mat);
