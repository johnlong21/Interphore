#pragma once

#include "duktape.h"

void initJs();
void deinitJs();
void runJs(const char *js);
void addJsFunction(const char *name, duk_c_function fn, int args);
void setJsKeyStatus(int key, int state);
void (*jsErrorFn)(const char *) = NULL;

duk_context *jsContext = NULL;
int jsKeys[KEY_LIMIT];
