#include <initializer_list>
#include <memory>
#include <cstdlib>
#include <jni.h>
#include <errno.h>
#include <cassert>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <android/sensor.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#ifndef LOGI
# define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "semiphore", __VA_ARGS__))
# define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "semiphore", __VA_ARGS__))
# define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "semiphore", __VA_ARGS__))
#endif

#include "main.cpp"

struct saved_state {
	float angle;
	int32_t x;
	int32_t y;
};

struct AppEngine {
	struct android_app* app;

	ASensorManager* sensorManager;
	const ASensor* accelerometerSensor;
	ASensorEventQueue* sensorEventQueue;

	int animating;
	bool started;
	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
	int32_t width;
	int32_t height;
	struct saved_state state;
};

/**
	* Process the next input event.
	*/
/**
	* Process the next main command.
	*/
static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
	AppEngine* engine = (AppEngine*)app->userData;
	switch (cmd) {
		case APP_CMD_SAVE_STATE:
			// The system has asked us to save our current state.  Do so.
			// engine->app->savedState = malloc(sizeof(struct saved_state));
			// *((struct saved_state*)engine->app->savedState) = engine->state;
			// engine->app->savedStateSize = sizeof(struct saved_state);
			break;
		case APP_CMD_INIT_WINDOW:
			// The window is being shown, get it ready.
			if (engine->app->window != NULL) {
				LOGI("STARTING");
				androidWindow = engine->app->window;
				androidApp = engine->app;
				entryPoint();
				engine->started = true;
				// engine_init_display(engine);
				// engine_draw_frame(engine);
			}
			break;
		case APP_CMD_TERM_WINDOW:
			// The window is being hidden or closed, clean it up.
			// engine_term_display(engine);
			break;
		case APP_CMD_GAINED_FOCUS:
			// When our app gains focus, we start monitoring the accelerometer.
			// if (engine->accelerometerSensor != NULL) {
			// 	ASensorEventQueue_enableSensor(engine->sensorEventQueue,
			// 		engine->accelerometerSensor);
			// 	// We'd like to get 60 events per second (in us).
			// 	ASensorEventQueue_setEventRate(engine->sensorEventQueue,
			// 		engine->accelerometerSensor,
			// 		(1000L/60)*1000);
			// }
			break;
		case APP_CMD_LOST_FOCUS:
			// When our app loses focus, we stop monitoring the accelerometer.
			// This is to avoid consuming battery while not being used.
			// if (engine->accelerometerSensor != NULL) {
			// 	ASensorEventQueue_disableSensor(engine->sensorEventQueue,
			// 		engine->accelerometerSensor);
			// }
			// Also stop animating.
			// engine->animating = 0;
			// engine_draw_frame(engine);
			break;
	}
}

/*
	* AcquireASensorManagerInstance(void)
	*    Workaround ASensorManager_getInstance() deprecation false alarm
	*    for Android-N and before, when compiling with NDK-r15
	*/
#include <dlfcn.h>


/**
	* This is the main entry point of a native application that is using
	* android_native_app_glue.  It runs in its own thread, with its own
	* event loop for receiving input events and doing other things.
	*/
void android_main(struct android_app *state) {
	strcpy(tempSavePath, state->activity->internalDataPath);
	printf("in dir: %s\n", state->activity->internalDataPath);
	strcat(tempSavePath, "/tempSave");
	// entryPoint();

	AppEngine engine;

	memset(&engine, 0, sizeof(AppEngine));
	state->userData = &engine;
	state->onAppCmd = engine_handle_cmd;
	state->onInputEvent = androidHandleInput;
	engine.app = state;
	androidApp = state;

	if (state->savedState != NULL) {
		// We are starting with a previous saved state; restore from it.
		engine.state = *(struct saved_state*)state->savedState;
	}

	// loop waiting for stuff to do.

	while (1) {
		// Read all pending events.
		int ident;
		int events;
		struct android_poll_source* source;

		// If not animating, we will block forever waiting for events.
		// If animating, we loop until all events are read, then continue
		// to draw the next frame of animation.
		while ((ident=ALooper_pollAll(0, NULL, &events, (void**)&source)) >= 0) {

			// Process this event.
			if (source != NULL) {
				source->process(state, source);
			}

			// Check if we are exiting.
			if (state->destroyRequested != 0) {
				// engine_term_display(&engine);
				return;
			}
		}

		if (engine.started) break;
	}
}
//END_INCLUDE(all)
