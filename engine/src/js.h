#pragma once

#include "duktape.h"
#include "defines.h"

void initJs();
void deinitJs();
void runJs(const char *js);
void addJsFunction(const char *name, duk_c_function fn, int args);
void setJsKeyStatus(int key, int state);

extern void (*jsErrorFn)(const char *);

extern duk_context *jsContext;
extern int jsKeys[KEY_LIMIT];

void addJsVariable(const char *name, bool boolean);
