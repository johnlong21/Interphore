#include "platform.h"
#include "engine.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <unistd.h>

#include <jni.h>
#include <android/looper.h>
#include <android/sensor.h>
#include <android/window.h>

android_app *androidApp = NULL;
ANativeWindow *androidWindow;
EGLDisplay androidDisplay;
EGLSurface androidSurface;
EGLContext androidContext;
bool platformMouseLeftBuffer = false;

const char androidPackageName[] = "com.paraphore.semiphore";
char tempSavePath[PATH_LIMIT];
void androidProcessEvents();
static int32_t androidHandleInput(struct android_app* app, AInputEvent* event);

void *initPlatform(int mem) {
	LOGI("Plat initing\n");

	assetManager = androidApp->activity->assetManager;
	ANativeActivity_setWindowFlags(androidApp->activity, AWINDOW_FLAG_FULLSCREEN, 0);

	androidDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	eglInitialize(androidDisplay, 0, 0);

	const EGLint attribs[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_BLUE_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_RED_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_NONE
	};

	EGLConfig config;
	EGLint numConfigs;
	EGLint format;

	LOGI("Initializing context");

	eglChooseConfig(androidDisplay, attribs, &config, 1, &numConfigs);
	eglGetConfigAttrib(androidDisplay, config, EGL_NATIVE_VISUAL_ID, &format);

	ANativeWindow_setBuffersGeometry(androidWindow, 0, 0, format);

	androidSurface = eglCreateWindowSurface(androidDisplay, config, androidWindow, 0);

	const EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
	androidContext = eglCreateContext(androidDisplay, config, 0, contextAttribs);

	eglMakeCurrent(androidDisplay, androidSurface, androidSurface, androidContext);
	eglQuerySurface(androidDisplay, androidSurface, EGL_WIDTH, &platWidth);
	eglQuerySurface(androidDisplay, androidSurface, EGL_HEIGHT, &platHeight);

	glDepthMask(GL_FALSE);
	platPlatform = PLAT_ANDROID;

	ASensorManager *sensor_manager = ASensorManager_getInstance();
	Assert(sensor_manager);

#if 0
	ASensorList sensor_list;
	int sensor_count = ASensorManager_getSensorList(sensor_manager, &sensor_list);
	// printf("Found %d sensors\n", sensor_count);
	// for (int i = 0; i < sensor_count; i++) printf("Found %s\n", ASensor_getName(sensor_list[i]));

	ASensorEventQueue *queue = ASensorManager_createEventQueue(sensor_manager, ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS), 1, NULL, NULL);
	Assert(queue);

		// Find the first sensor of the specified type that can be opened
		// const int kTimeoutMicroSecs = 1000000;
		// const int kTimeoutMilliSecs = 1000;
		// ASensorRef sensor = NULL;
		// bool sensor_found = false;

		// for (int i = 0; i < sensor_count; i++) {
		// 	sensor = sensor_list[i];
		// 	if (ASensor_getType(sensor) != ASENSOR_TYPE_ACCELEROMETER) continue;
		// 	if (ASensorEventQueue_enableSensor(queue, sensor) < 0) continue;
		// 	if (ASensorEventQueue_setEventRate(queue, sensor, kTimeoutMicroSecs) < 0) continue;

		// 	// Found an equipped sensor of the specified type.
		// 	sensor_found = true;
		// 	break;
		// }

		// Assert(sensor_found);
		// printf("\nSensor %s activated\n", ASensor_getName(sensor));

		// const int kNumEvents = 1;
		// const int kNumSamples = 10;
		// const int kWaitTimeSecs = 1;
		// for (int i = 0; i < kNumSamples; i++) {
		// 	ASensorEvent data[kNumEvents];
		// 	memset(data, 0, sizeof(data));
		// 	int ident = ALooper_pollAll(kTimeoutMilliSecs, NULL, NULL, NULL);

		// 	if (ident != 1) {
		// 		fprintf(stderr, "Incorrect Looper ident read from poll.\n");
		// 		continue;
		// 	}

		// 	if (ASensorEventQueue_getEvents(queue, data, kNumEvents) <= 0) {
		// 		fprintf(stderr, "Failed to read data from the sensor.\n");
		// 		continue;
		// 	}

		// 	sleep(kWaitTimeSecs);
		// }

		// int ret = ASensorEventQueue_disableSensor(queue, sensor);
		// if (ret < 0) fprintf(stderr, "Failed to disable %s: %s\n", ASensor_getName(sensor), strerror(-ret));

		// ret = ASensorManager_destroyEventQueue(sensor_manager, queue);
		// if (ret < 0) fprintf(stderr, "Failed to destroy event queue: %s\n", strerror(-ret));
#endif

	void *memory = Malloc(mem);
	memset(memory, 0, mem);
	return memory;
}

void platformStartFrame() {
	platformMouseLeftDown = platformMouseLeftBuffer;
	androidProcessEvents();
}

void platformEndFrame() {
	eglSwapBuffers(androidDisplay, androidSurface);
}

unsigned long platformGetTime() {
	struct timespec res;
	clock_gettime(CLOCK_MONOTONIC, &res);

	time_t s = res.tv_sec;
	unsigned long ms = round(res.tv_nsec / 1.0e6);
	ms += s * 1000;

	// printf("sec %ld ns %ld\n", res.tv_sec, res.tv_nsec);
	// printf("Time is %d\n", ms);
	return ms;
}

void platformSleepMs(int millis) {
	sleep(millis/1000.0);
}

void platformSaveToTemp(const char *str) {
	printf("savePath: %s\n", tempSavePath);

	FILE *f = fopen(tempSavePath, "w");
	Assert(f);

	fprintf(f, "%s", str);
	fclose(f);
}

char *platformLoadFromTemp() {
	FILE *f = fopen(tempSavePath, "r");

	if (f == NULL) {
		char *str = (char *)"none";
		return stringClone(str);
	}

	fseek(f, 0, SEEK_END);
	long fileLen = ftell(f);
	rewind(f);

	char *str = (char *)Malloc((fileLen + 1) * sizeof(char));
	fread(str, fileLen, 1, f);
	fclose(f);

	str[fileLen] = '\0';
	return str;
}

void platformSaveToDisk(const char *str) {
	platformSaveToTemp(str);
}

void platformLoadFromDisk(void (*loadCallback)(char *, int)) {
	char *str = platformLoadFromTemp();
	loadCallback(str, strlen(str));
}

void platformLoadFromUrl(const char *url, void (*loadCallback)(char *, int)) {
	loadFromUrl(url, loadCallback);
}

void platformClearTemp() {
	remove(tempSavePath);
}

void androidProcessEvents() {
	int ident;
	int events;
	struct android_poll_source* source;

	while ((ident=ALooper_pollAll(0, NULL, &events, (void**)&source)) >= 0) {
		if (source != NULL) {
			source->process(androidApp, source);
		}
	}
}

static int32_t androidHandleInput(struct android_app* app, AInputEvent* event) {
	// struct engine* engine = (struct engine*)app->userData;
	if (!assetManager) {
		printf("Got asset manager\n");
		assetManager = app->activity->assetManager;
	}

	if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
		platformMouseX = AMotionEvent_getX(event, 0);
		platformMouseY = AMotionEvent_getY(event, 0);

		if (AKeyEvent_getAction(event) == AMOTION_EVENT_ACTION_UP) platformMouseLeftBuffer = false;
		if (AKeyEvent_getAction(event) == AMOTION_EVENT_ACTION_DOWN) platformMouseLeftBuffer = true;
		return 1;
	}

	if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY) {
		int keyValue = AKeyEvent_getKeyCode(event);
		int engineValue = -1;

		if (keyValue == AKEYCODE_0) engineValue = '0';
		if (keyValue == AKEYCODE_1) engineValue = '1';
		if (keyValue == AKEYCODE_2) engineValue = '2';
		if (keyValue == AKEYCODE_3) engineValue = '3';
		if (keyValue == AKEYCODE_4) engineValue = '4';
		if (keyValue == AKEYCODE_5) engineValue = '5';
		if (keyValue == AKEYCODE_6) engineValue = '6';
		if (keyValue == AKEYCODE_7) engineValue = '7';
		if (keyValue == AKEYCODE_8) engineValue = '8';
		if (keyValue == AKEYCODE_9) engineValue = '9';
		if (keyValue == AKEYCODE_DPAD_UP) engineValue = KEY_UP;
		if (keyValue == AKEYCODE_DPAD_DOWN) engineValue = KEY_DOWN;
		if (keyValue == AKEYCODE_DPAD_LEFT) engineValue = KEY_LEFT;
		if (keyValue == AKEYCODE_DPAD_RIGHT) engineValue = KEY_RIGHT;
		// if (keyValue == AKEYCODE_VOLUME_UP) engineValue = '';
		// if (keyValue == AKEYCODE_VOLUME_DOWN) engineValue = '';
		if (keyValue == AKEYCODE_A) engineValue = 'A';
		if (keyValue == AKEYCODE_B) engineValue = 'B';
		if (keyValue == AKEYCODE_C) engineValue = 'C';
		if (keyValue == AKEYCODE_D) engineValue = 'D';
		if (keyValue == AKEYCODE_E) engineValue = 'E';
		if (keyValue == AKEYCODE_F) engineValue = 'F';
		if (keyValue == AKEYCODE_G) engineValue = 'G';
		if (keyValue == AKEYCODE_H) engineValue = 'H';
		if (keyValue == AKEYCODE_I) engineValue = 'I';
		if (keyValue == AKEYCODE_J) engineValue = 'J';
		if (keyValue == AKEYCODE_K) engineValue = 'K';
		if (keyValue == AKEYCODE_L) engineValue = 'L';
		if (keyValue == AKEYCODE_M) engineValue = 'M';
		if (keyValue == AKEYCODE_N) engineValue = 'N';
		if (keyValue == AKEYCODE_O) engineValue = 'O';
		if (keyValue == AKEYCODE_P) engineValue = 'P';
		if (keyValue == AKEYCODE_Q) engineValue = 'Q';
		if (keyValue == AKEYCODE_R) engineValue = 'R';
		if (keyValue == AKEYCODE_S) engineValue = 'S';
		if (keyValue == AKEYCODE_T) engineValue = 'T';
		if (keyValue == AKEYCODE_U) engineValue = 'U';
		if (keyValue == AKEYCODE_V) engineValue = 'V';
		if (keyValue == AKEYCODE_W) engineValue = 'W';
		if (keyValue == AKEYCODE_X) engineValue = 'X';
		if (keyValue == AKEYCODE_Y) engineValue = 'Y';
		if (keyValue == AKEYCODE_Z) engineValue = 'Z';
		if (keyValue == AKEYCODE_COMMA) engineValue = ',';
		if (keyValue == AKEYCODE_PERIOD) engineValue = '.';
		// if (keyValue == AKEYCODE_ALT_LEFT) engineValue = '';
		// if (keyValue == AKEYCODE_ALT_RIGHT) engineValue = '';
		if (keyValue == AKEYCODE_SHIFT_LEFT) engineValue = KEY_SHIFT;
		if (keyValue == AKEYCODE_SHIFT_RIGHT) engineValue = KEY_SHIFT;
		// if (keyValue == AKEYCODE_TAB) engineValue = '';
		if (keyValue == AKEYCODE_SPACE) engineValue = ' ';
		// if (keyValue == AKEYCODE_ENTER) engineValue = '';
		if (keyValue == AKEYCODE_DEL) engineValue = KEY_BACKSPACE;
		if (keyValue == AKEYCODE_GRAVE) engineValue = '`';
		if (keyValue == AKEYCODE_MINUS) engineValue = '-';
		if (keyValue == AKEYCODE_EQUALS) engineValue = '=';
		if (keyValue == AKEYCODE_LEFT_BRACKET) engineValue = '[';
		if (keyValue == AKEYCODE_RIGHT_BRACKET) engineValue = ']';
		if (keyValue == AKEYCODE_BACKSLASH) engineValue = '\\';
		if (keyValue == AKEYCODE_SEMICOLON) engineValue = ';';
		if (keyValue == AKEYCODE_APOSTROPHE) engineValue = '\'';
		if (keyValue == AKEYCODE_SLASH) engineValue = '/';
		if (keyValue == AKEYCODE_AT) engineValue = '@';
		// if (keyValue == AKEYCODE_PAGE_UP) engineValue = '';
		// if (keyValue == AKEYCODE_PAGE_DOWN) engineValue = '';
		// if (keyValue == AKEYCODE_ESCAPE) engineValue = '';
		// if (keyValue == AKEYCODE_FORWARD_DEL) engineValue = '';
		// if (keyValue == AKEYCODE_CTRL_LEFT) engineValue = '';
		// if (keyValue == AKEYCODE_CTRL_RIGHT) engineValue = '';
		// if (keyValue == AKEYCODE_INSERT) engineValue = '';
		// if (keyValue == AKEYCODE_F1) engineValue = '';
		// if (keyValue == AKEYCODE_F2) engineValue = '';
		// if (keyValue == AKEYCODE_F3) engineValue = '';
		// if (keyValue == AKEYCODE_F4) engineValue = '';
		// if (keyValue == AKEYCODE_F5) engineValue = '';
		// if (keyValue == AKEYCODE_F6) engineValue = '';
		// if (keyValue == AKEYCODE_F7) engineValue = '';
		// if (keyValue == AKEYCODE_F8) engineValue = '';
		// if (keyValue == AKEYCODE_F9) engineValue = '';
		// if (keyValue == AKEYCODE_F10) engineValue = '';
		// if (keyValue == AKEYCODE_F11) engineValue = '';
		// if (keyValue == AKEYCODE_F12) engineValue = '';
		// if (keyValue == AKEYCODE_NUM_LOCK) engineValue = '';
		if (keyValue == AKEYCODE_NUMPAD_0) engineValue = '0';
		if (keyValue == AKEYCODE_NUMPAD_1) engineValue = '1';
		if (keyValue == AKEYCODE_NUMPAD_2) engineValue = '2';
		if (keyValue == AKEYCODE_NUMPAD_3) engineValue = '3';
		if (keyValue == AKEYCODE_NUMPAD_4) engineValue = '4';
		if (keyValue == AKEYCODE_NUMPAD_5) engineValue = '5';
		if (keyValue == AKEYCODE_NUMPAD_6) engineValue = '6';
		if (keyValue == AKEYCODE_NUMPAD_7) engineValue = '7';
		if (keyValue == AKEYCODE_NUMPAD_8) engineValue = '8';
		if (keyValue == AKEYCODE_NUMPAD_9) engineValue = '9';
		if (keyValue == AKEYCODE_NUMPAD_DIVIDE) engineValue = '/';
		if (keyValue == AKEYCODE_NUMPAD_MULTIPLY) engineValue = '*';
		if (keyValue == AKEYCODE_NUMPAD_SUBTRACT) engineValue = '-';
		if (keyValue == AKEYCODE_NUMPAD_ADD) engineValue = '+';
		if (keyValue == AKEYCODE_NUMPAD_DOT) engineValue = '.';
		if (keyValue == AKEYCODE_NUMPAD_COMMA) engineValue = ',';
		// if (keyValue == AKEYCODE_NUMPAD_ENTER) engineValue = '';
		if (keyValue == AKEYCODE_NUMPAD_EQUALS) engineValue = '=';
		if (keyValue == AKEYCODE_NUMPAD_LEFT_PAREN) engineValue = '(';
		if (keyValue == AKEYCODE_NUMPAD_RIGHT_PAREN) engineValue = ')';

		int action = AKeyEvent_getAction(event);
		if (keyValue != -1 && action == AKEY_EVENT_ACTION_DOWN) pressKey(engineValue);
		if (keyValue != -1 && action == AKEY_EVENT_ACTION_UP) releaseKey(engineValue);

		return 1;
	}

	return 0;
}

void displayKeyboard(bool show) {
	// Attaches the current thread to the JVM.
	jint lResult;
	jint lFlags = 0;

	JavaVM* lJavaVM = androidApp->activity->vm;
	JNIEnv* lJNIEnv = androidApp->activity->env;

	JavaVMAttachArgs lJavaVMAttachArgs;
	lJavaVMAttachArgs.version = JNI_VERSION_1_6;
	lJavaVMAttachArgs.name = "NativeThread";
	lJavaVMAttachArgs.group = NULL;

	lResult = lJavaVM->AttachCurrentThread(&lJNIEnv, &lJavaVMAttachArgs);
	if (lResult == JNI_ERR) {
		return;
	}

	// Retrieves NativeActivity.
	jobject lNativeActivity = androidApp->activity->clazz;
	jclass ClassNativeActivity = lJNIEnv->GetObjectClass(lNativeActivity);

	// Retrieves Context.INPUT_METHOD_SERVICE.
	jclass ClassContext = lJNIEnv->FindClass("android/content/Context");
	jfieldID FieldINPUT_METHOD_SERVICE = lJNIEnv->GetStaticFieldID(ClassContext, "INPUT_METHOD_SERVICE", "Ljava/lang/String;");
	jobject INPUT_METHOD_SERVICE = lJNIEnv->GetStaticObjectField(ClassContext, FieldINPUT_METHOD_SERVICE);
	// jniCheck(INPUT_METHOD_SERVICE);

	// Runs getSystemService(Context.INPUT_METHOD_SERVICE).
	jclass ClassInputMethodManager = lJNIEnv->FindClass("android/view/inputmethod/InputMethodManager");
	jmethodID MethodGetSystemService = lJNIEnv->GetMethodID(ClassNativeActivity, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
	jobject lInputMethodManager = lJNIEnv->CallObjectMethod(lNativeActivity, MethodGetSystemService, INPUT_METHOD_SERVICE);

	// Runs getWindow().getDecorView().
	jmethodID MethodGetWindow = lJNIEnv->GetMethodID(ClassNativeActivity, "getWindow", "()Landroid/view/Window;");
	jobject lWindow = lJNIEnv->CallObjectMethod(lNativeActivity, MethodGetWindow);
	jclass ClassWindow = lJNIEnv->FindClass("android/view/Window");
	jmethodID MethodGetDecorView = lJNIEnv->GetMethodID(ClassWindow, "getDecorView", "()Landroid/view/View;");
	jobject lDecorView = lJNIEnv->CallObjectMethod(lWindow, MethodGetDecorView);

	if (show) {
		// Runs lInputMethodManager.showSoftInput(...).
		jmethodID MethodShowSoftInput = lJNIEnv->GetMethodID(ClassInputMethodManager, "showSoftInput", "(Landroid/view/View;I)Z");
		/*jboolean lResult = */lJNIEnv->CallBooleanMethod(lInputMethodManager, MethodShowSoftInput, lDecorView, lFlags);
	} else {
		// Runs lWindow.getViewToken()
		jclass ClassView = lJNIEnv->FindClass("android/view/View");
		jmethodID MethodGetWindowToken = lJNIEnv->GetMethodID(ClassView, "getWindowToken", "()Landroid/os/IBinder;");
		jobject lBinder = lJNIEnv->CallObjectMethod(lDecorView, MethodGetWindowToken);

		// lInputMethodManager.hideSoftInput(...).
		jmethodID MethodHideSoftInput = lJNIEnv->GetMethodID(ClassInputMethodManager, "hideSoftInputFromWindow", "(Landroid/os/IBinder;I)Z");
		/*jboolean lRes = */lJNIEnv->CallBooleanMethod(lInputMethodManager, MethodHideSoftInput, lBinder, lFlags);
	}

	// Finished with the JVM.
	lJavaVM->DetachCurrentThread();
}

void gotoUrl(const char *url) {
}
