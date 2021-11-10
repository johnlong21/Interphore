#include "js.hpp"
#include "engine.hpp"

duk_ret_t js_print(duk_context *ctx);
duk_ret_t js_clampMap(duk_context *ctx);
duk_ret_t js_map(duk_context *ctx);
duk_ret_t js_norm(duk_context *ctx);
duk_ret_t js_lerp(duk_context *ctx);
void jsFatalHandler(void *udata, const char *msg);

void (*jsErrorFn)(const char *) = NULL;

void *jsUdata = nullptr;
duk_context *jsContext = nullptr;
int jsKeys[KEY_LIMIT];

void initJs() {
	jsContext = duk_create_heap(NULL, NULL, NULL, jsUdata, jsFatalHandler);
	Assert(jsContext);

	addJsFunction("print", js_print, 1);

	addJsFunction("lerp_internal", js_lerp, 3);
	addJsFunction("norm_internal", js_norm, 3);
	addJsFunction("map_internal", js_map, 6);
	addJsFunction("clampMap_internal", js_clampMap, 6);

#ifdef SEMI_INTERNAL
	const bool internalBool = true;
#else
	const bool internalBool = false;
#endif

#ifdef SEMI_DEBUG
	const bool debugBool = true;
#else
	const bool debugBool = false;
#endif

#ifdef SEMI_EARLY
	const bool earlyBool = true;
#else
	const bool earlyBool = false;
#endif

    const duk_number_list_entry globalValues[] = {
        { "gameWidth", (double)engine->width },
        { "gameHeight", (double)engine->height },
        { "KEY_JUST_PRESSED", KEY_JUST_PRESSED },
        { "KEY_PRESSED", KEY_PRESSED },
        { "KEY_RELEASED", KEY_RELEASED },
        { "KEY_JUST_RELEASED", KEY_JUST_RELEASED },
        { "KEY_SHIFT", KEY_SHIFT },
        { "KEY_BACKSPACE", KEY_BACKSPACE },
        { "LINEAR", LINEAR },
        { "QUAD_IN", QUAD_IN },
        { "QUAD_OUT", QUAD_OUT },
        { "QUAD_IN_OUT", QUAD_IN_OUT },
        { "CUBIC_IN", CUBIC_IN },
        { "CUBIC_OUT", CUBIC_OUT },
        { "CUBIC_IN_OUT", CUBIC_IN_OUT },
        { "QUART_IN", QUART_IN },
        { "QUART_OUT", QUART_OUT },
        { "QUART_IN_OUT", QUART_IN_OUT },
        { "QUINT_IN", QUINT_IN },
        { "QUINT_OUT", QUINT_OUT },
        { "QUINT_IN_OUT", QUINT_IN_OUT },
        { "SINE_IN", SINE_IN },
        { "SINE_OUT", SINE_OUT },
        { "SINE_IN_OUT", SINE_IN_OUT },
        { "CIRC_IN", CIRC_IN },
        { "CIRC_OUT", CIRC_OUT },
        { "CIRC_IN_OUT", CIRC_IN_OUT },
        { "EXP_IN", EXP_IN },
        { "EXP_OUT", EXP_OUT },
        { "EXP_IN_OUT", EXP_IN_OUT },
        { "ELASTIC_IN", ELASTIC_IN },
        { "ELASTIC_OUT", ELASTIC_OUT },
        { "ELASTIC_IN_OUT", ELASTIC_IN_OUT },
        { "BACK_IN", BACK_IN },
        { "BACK_OUT", BACK_OUT },
        { "BACK_IN_OUT", BACK_IN_OUT },
        { "BOUNCE_IN", BOUNCE_IN },
        { "BOUNCE_OUT", BOUNCE_OUT },
        { "BOUNCE_IN_OUT", BOUNCE_IN_OUT },
        { nullptr, 0.0 }
    };
    duk_push_global_object(jsContext);
    duk_put_number_list(jsContext, -1, globalValues);

    addJsVariable("internal", internalBool);
    addJsVariable("debug", debugBool);
    addJsVariable("early", earlyBool);
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

void addJsVariable(const char *name, bool boolean) {
    duk_push_boolean(jsContext, boolean);
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
