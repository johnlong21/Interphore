#include "js.h"

duk_ret_t js_print(duk_context *ctx);
duk_ret_t js_clampMap(duk_context *ctx);
duk_ret_t js_map(duk_context *ctx);
duk_ret_t js_norm(duk_context *ctx);
duk_ret_t js_lerp(duk_context *ctx);
void jsFatalHandler(void *udata, const char *msg);

void *jsUdata = NULL;

void initJs() {
	jsContext = duk_create_heap(NULL, NULL, NULL, jsUdata, jsFatalHandler);
	Assert(jsContext);

	addJsFunction("print", js_print, 1);

	addJsFunction("lerp_internal", js_lerp, 3);
	addJsFunction("norm_internal", js_norm, 3);
	addJsFunction("map_internal", js_map, 6);
	addJsFunction("clampMap_internal", js_clampMap, 6);

#ifdef SEMI_INTERNAL
	bool internalBool = true;
#else
	bool internalBool = false;
#endif

#ifdef SEMI_DEBUG
	bool debugBool = true;
#else
	bool debugBool = false;
#endif

#ifdef SEMI_EARLY
	bool earlyBool = true;
#else
	bool earlyBool = false;
#endif

	char buf[2048];
	sprintf(
		buf,
		"var gameWidth = %d;"
		"var gameHeight = %d;"
		"var isFlash = %d;"
		"var isAndroid = %d;"
		"var internal = %d;"
		"var debug = %d;"
		"var early = %d;"
		"var KEY_JUST_PRESSED = %d;"
		"var KEY_PRESSED = %d;"
		"var KEY_RELEASED = %d;"
		"var KEY_JUST_RELEASED = %d;"
		"var KEY_SHIFT = %d;"
		"var KEY_BACKSPACE = %d;"
		"var LINEAR = %d;"
		"var QUAD_IN = %d;"
		"var QUAD_OUT = %d;"
		"var QUAD_IN_OUT = %d;"
		"var CUBIC_IN = %d;"
		"var CUBIC_OUT = %d;"
		"var CUBIC_IN_OUT = %d;"
		"var QUART_IN = %d;"
		"var QUART_OUT = %d;"
		"var QUART_IN_OUT = %d;"
		"var QUINT_IN = %d;"
		"var QUINT_OUT = %d;"
		"var QUINT_IN_OUT = %d;"
		"var SINE_IN = %d;"
		"var SINE_OUT = %d;"
		"var SINE_IN_OUT = %d;"
		"var CIRC_IN = %d;"
		"var CIRC_OUT = %d;"
		"var CIRC_IN_OUT = %d;"
		"var EXP_IN = %d;"
		"var EXP_OUT = %d;"
		"var EXP_IN_OUT = %d;"
		"var ELASTIC_IN = %d;"
		"var ELASTIC_OUT = %d;"
		"var ELASTIC_IN_OUT = %d;"
		"var BACK_IN = %d;"
		"var BACK_OUT = %d;"
		"var BACK_IN_OUT = %d;"
		"var BOUNCE_IN = %d;"
		"var BOUNCE_OUT = %d;"
		"var BOUNCE_IN_OUT = %d;",
	engine->width,
	engine->height,
	engine->platform == PLAT_FLASH,
	engine->platform == PLAT_ANDROID,
	internalBool,
	debugBool,
	earlyBool,
	KEY_JUST_PRESSED,
	KEY_PRESSED,
	KEY_RELEASED,
	KEY_JUST_RELEASED,
	KEY_SHIFT,
	KEY_BACKSPACE,
	LINEAR,
	QUAD_IN,
	QUAD_OUT,
	QUAD_IN_OUT,
	CUBIC_IN,
	CUBIC_OUT,
	CUBIC_IN_OUT,
	QUART_IN,
	QUART_OUT,
	QUART_IN_OUT,
	QUINT_IN,
	QUINT_OUT,
	QUINT_IN_OUT,
	SINE_IN,
	SINE_OUT,
	SINE_IN_OUT,
	CIRC_IN,
	CIRC_OUT,
	CIRC_IN_OUT,
	EXP_IN,
	EXP_OUT,
	EXP_IN_OUT,
	ELASTIC_IN,
	ELASTIC_OUT,
	ELASTIC_IN_OUT,
	BACK_IN,
	BACK_OUT,
	BACK_IN_OUT,
	BOUNCE_IN,
	BOUNCE_OUT,
	BOUNCE_IN_OUT
		);
	runJs(buf);
}

void deinitJs() {
	duk_destroy_heap(jsContext);
}

void jsFatalHandler(void *udata, const char *msg) {
	(void)udata;

	printf("JS FATAL ERROR: %s\n", msg);
}

void addJsFunction(const char *name, duk_c_function fn, int args) {
	duk_push_c_function(jsContext, fn, args);
	duk_put_global_string(jsContext, name);
}

void runJs(const char *js) {
	// printf("Running:\n%s\n", js);
	// duk_eval_string(jsContext, js);

	duk_push_string(jsContext, js);
	if (duk_peval(jsContext) != 0) {
		if (jsErrorFn) {
			printf("Code was:\n%s\n", js);
			jsErrorFn(duk_safe_to_string(jsContext, -1));
		} else {
			printf("eval failed: %s\n", duk_safe_to_string(jsContext, -1));
		}
	}
	duk_pop(jsContext);
}

void setJsKeyStatus(int key, int state) {
	char buf[128];
	sprintf(buf, "keys[%d] = %d;", key, state);
	runJs(buf);
}

duk_ret_t js_print(duk_context *ctx) {
	printf("%s\n", duk_safe_to_string(ctx, -1));
	return 0;
}

duk_ret_t js_lerp(duk_context *ctx) {
	float max = duk_get_number(ctx, -1);
	float min = duk_get_number(ctx, -2);
	float perc = duk_get_number(ctx, -3);

	duk_push_number(ctx, mathLerp(perc, min, max));
	return 1;
}

duk_ret_t js_norm(duk_context *ctx) {
	float max = duk_get_number(ctx, -1);
	float min = duk_get_number(ctx, -2);
	float value = duk_get_number(ctx, -3);

	duk_push_number(ctx, mathNorm(value, min, max));
	return 1;
}

duk_ret_t js_map(duk_context *ctx) {
	Ease ease = (Ease)duk_get_int(ctx, -1);
	float destMax = duk_get_number(ctx, -2);
	float destMin = duk_get_number(ctx, -3);
	float sourceMax = duk_get_number(ctx, -4);
	float sourceMin = duk_get_number(ctx, -5);
	float value = duk_get_number(ctx, -6);

	duk_push_number(ctx, mathMap(value, sourceMin, sourceMax, destMin, destMax, ease));
	return 1;
}

duk_ret_t js_clampMap(duk_context *ctx) {
	Ease ease = (Ease)duk_get_int(ctx, -1);
	float destMax = duk_get_number(ctx, -2);
	float destMin = duk_get_number(ctx, -3);
	float sourceMax = duk_get_number(ctx, -4);
	float sourceMin = duk_get_number(ctx, -5);
	float value = duk_get_number(ctx, -6);

	duk_push_number(ctx, mathClampMap(value, sourceMin, sourceMax, destMin, destMax, ease));
	return 1;
}
