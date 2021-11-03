#include "platform.h"
#include "engine.h"

#ifdef SEMI_HTML5
#include <emscripten.h>
#endif

#ifdef SEMI_DESKTOP
#include <filesystem>
#include <fstream>
#include <sstream>
#include "nfd.h"
#endif

#include <vector>
#include <functional>

SDL_Window* sdlWindow = NULL;
SDL_GLContext sdlContext = NULL;

void initPlatform() {
	printf("Plat initing\n");

#ifdef SEMI_DESKTOP
	// Change cwd to where the program is
    std::filesystem::current_path(std::filesystem::path(platformArgV[0]).parent_path());
#endif

	Assert(SDL_Init(SDL_INIT_VIDEO) >= 0);

#if defined(SEMI_GL_CORE)
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	// SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
#elif defined(SEMI_GLES) && defined(SEMI_ANDROID)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	// SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
	// SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	// SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	// SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

#ifdef SEMI_HTML5
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");
#endif

    int flags = SDL_WINDOW_OPENGL;
#ifdef SEMI_ANDROID
    flags |= SDL_WINDOW_FULLSCREEN;

    SDL_DisplayMode displayMode;
    SDL_GetCurrentDisplayMode(0, &displayMode);
    platWidth = displayMode.w;
    platHeight = displayMode.h;
#endif

	sdlWindow = SDL_CreateWindow("Interphore", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, platWidth, platHeight, flags);
	// sdlWindow = SDL_CreateWindow("Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, platWidth, platHeight, SDL_WINDOW_SHOWN);
	Assert(sdlWindow);

	sdlContext = SDL_GL_CreateContext(sdlWindow);
	Assert(sdlContext);

	Assert(!SDL_GL_SetSwapInterval(1));

#ifdef GLEW_STATIC
    glewExperimental = GL_TRUE;
	Assert(glewInit() == GLEW_OK);
#endif

	platPlatform = PLAT_SDL2;

	printf("Maybe set\n");
#ifdef SEMI_HTML5
	printf("Setting inf loop\n");
	platformShouldClose = true;
	emscripten_set_main_loop(updateEngine, 0, 0);
	printf("Set\n");
#endif

#ifdef SEMI_DESKTOP
    NFD_Init();
#endif
}

#ifdef SEMI_DESKTOP
void deinitPlatform() {
    NFD_Quit();
}
#endif

// Callbacks to run after everything is done in platformStartFrame, on the main thread
std::vector<std::function<void()>> startFrameCallbacksOnce;

void platformStartFrame() {
	platformMouseWheel = 0;

	SDL_Event e;
	while (SDL_PollEvent(&e) != 0) {
		switch (e.type) {
			case SDL_QUIT:
				platformShouldClose = true;
				break;
			case SDL_KEYDOWN: {
				int key = e.key.keysym.sym;

				if (key >= 'a' && key <= 'z') key -= 'a' - 'A';
				if (key == SDLK_LSHIFT) key = KEY_SHIFT;
				if (key == SDLK_RSHIFT) key = KEY_SHIFT;
				if (key == SDLK_BACKSPACE) key = KEY_BACKSPACE;
				if (key == SDLK_LCTRL) key = KEY_CTRL;
				if (key == SDLK_RCTRL) key = KEY_CTRL;
				if (key == SDLK_UP) key = KEY_UP;
				if (key == SDLK_DOWN) key = KEY_DOWN;
				if (key == SDLK_LEFT) key = KEY_LEFT;
				if (key == SDLK_RIGHT) key = KEY_RIGHT;

				if (key == 27) platformShouldClose = true;
				pressKey(key);

				break;
			}
			case SDL_KEYUP: {
				int key = e.key.keysym.sym;

				if (key >= 'a' && key <= 'z') key -= 'a' - 'A';
				if (key == SDLK_LSHIFT) key = KEY_SHIFT;
				if (key == SDLK_RSHIFT) key = KEY_SHIFT;
				if (key == SDLK_BACKSPACE) key = KEY_BACKSPACE;
				if (key == SDLK_LCTRL) key = KEY_CTRL;
				if (key == SDLK_RCTRL) key = KEY_CTRL;
				if (key == SDLK_UP) key = KEY_UP;
				if (key == SDLK_DOWN) key = KEY_DOWN;
				if (key == SDLK_LEFT) key = KEY_LEFT;
				if (key == SDLK_RIGHT) key = KEY_RIGHT;

				releaseKey(key);

				break;
			}
			case SDL_MOUSEMOTION:
				SDL_GetMouseState(&platformMouseX, &platformMouseY);
				break;
			case SDL_MOUSEBUTTONDOWN:
				platformMouseLeftDown = true;
				break;
			case SDL_MOUSEBUTTONUP:
				platformMouseLeftDown = false;
				break;
			case SDL_MOUSEWHEEL:
				platformMouseWheel = e.wheel.y;
				break;
#ifdef SEMI_ANDROID
			case SDL_APP_WILLENTERBACKGROUND:
			    disableSound();
			    break;
			case SDL_APP_WILLENTERFOREGROUND:
			    enableSound();
			    break;
#endif
		}
	}

	for (auto &callback : startFrameCallbacksOnce) {
		callback();
	}
	startFrameCallbacksOnce.clear();
}

void platformEndFrame() {
	SDL_GL_SwapWindow(sdlWindow);
}

unsigned long platformGetTime() {
	return SDL_GetTicks();
}

void platformSleepMs(int millis) {
#ifdef _WIN32
	Sleep(millis);
#elif __linux__
	timespec ts;
	ts.tv_sec = millis / 1000;
	ts.tv_nsec = (millis % 1000) * 1000000;
	nanosleep(&ts, NULL);
#endif
}

#if defined(SEMI_HTML5)

EM_JS(void,
      download_file,
      (const char* text),
      {
          var element = document.createElement("a");
          element.setAttribute("href", "data:text/plain;charset=utf-8," + encodeURIComponent(UTF8ToString(text)));
          element.setAttribute("download", "Interphore.txt");
          element.setAttribute("style", "display: none;");

          try {
              document.body.appendChild(element);

              element.click();
          } finally {
              document.body.removeChild(element);
          }
      }
);

EM_JS(void,
      upload_file,
      ( void *loadCallback ),
      {
          var element = document.createElement("input");
          element.setAttribute("type", "file");
          element.setAttribute("name", "file");

          try {
              document.body.appendChild(element);

              element.click();
              element.onchange = function() {
                  var file = element.files[0];

                  // Don't do anything unless we actually have a file
                  if (file === undefined) return;

                  var fileReader = new FileReader();
                  fileReader.readAsText(file);
                  fileReader.onload = function() {
                      var fileData = fileReader.result;

                      // Lenght + \0
                      var lenght = lengthBytesUTF8(fileData) + 1;
                      var wasmHeapStr = _malloc(lenght);
                      stringToUTF8(fileData, wasmHeapStr, lenght);

                      dynCall("vii", loadCallback, [wasmHeapStr, lenght]);
                  };
              }
          } finally {
              document.body.removeChild(element);
          }
      }
);

void platformSaveToDisk(const char *str) {
    download_file(str);
}

void platformLoadFromDisk(void (*loadCallback)(char *, int)) {
    // Have to use a void pointer here, because EM_JS doesn't understand commas in types,
    // so a function pointer doesn't work
	upload_file((void*)loadCallback);
    // TODO: use std::string, the savegame string needs to be freed, otherwise it leaks

	// str[fileLen] = '\0';
}

#elif defined(SEMI_DESKTOP)

void platformSaveToDisk(const char *str) {
    nfdchar_t *savePath;

    nfdresult_t result = NFD_SaveDialog(&savePath, nullptr, 0, nullptr, "Interphore.txt");
    if (result == NFD_OKAY) {
        std::ofstream saveFile(savePath);
        saveFile << std::string(str);

        NFD_FreePath(savePath);
    }   
}

void platformLoadFromDisk(void (*loadCallback)(char *, int)) {
    nfdchar_t *savePath;

    nfdresult_t result = NFD_OpenDialog(&savePath, nullptr, 0, nullptr);
    if (result == NFD_OKAY) {
        std::ifstream saveFile(savePath);

        std::stringstream ss;
        ss << saveFile.rdbuf();

        NFD_FreePath(savePath);

        auto savegameString = ss.str();
        loadCallback(stringClone(savegameString.data()), strlen(savegameString.data()));
    }    
}

#elif defined(SEMI_ANDROID)

void platformSaveToDisk(const char *str) {
	jobject activity = (jobject)SDL_AndroidGetActivity();
	JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();

    jstring saveDataString = env->NewStringUTF(str);

    // Call the JVM function to handle the saving of the game
	jclass activityClass = env->GetObjectClass(activity);
	jmethodID saveToDiskID = env->GetMethodID(activityClass, "saveToDisk", "(Ljava/lang/String;)V");
	env->CallVoidMethod(activity, saveToDiskID, saveDataString);

	env->DeleteLocalRef(saveDataString);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_paraphore_interphore_InterphoreGameActivity_loadFromDiskCallback(JNIEnv *env, jobject obj, jlong callback, jstring data) {
	// Cast the callback back to the actual type
	auto loadCallback = reinterpret_cast<void (*)(const char *, int)>(callback);

	const char *loadData = env->GetStringUTFChars(data, nullptr);
	// Schedule a callback on the main thread, otherwise OpenGL can't find the context,
	// because this function is called from the activity thread
	startFrameCallbacksOnce.push_back([=]() { loadCallback(loadData, strlen(loadData)); });
}

void platformLoadFromDisk(void (*loadCallback)(char *, int)) {
    jobject activity = (jobject)SDL_AndroidGetActivity();
    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();

    jclass activityClass = env->GetObjectClass(activity);
    jmethodID loadFromDiskID = env->GetMethodID(activityClass, "loadFromDisk", "(J)V");

	// Call the jvm function to handle loading and pass the callback as a long
    env->CallVoidMethod(activity, loadFromDiskID, reinterpret_cast<long>(loadCallback));
}

#endif

void platformLoadFromUrl(const char *url, void (*loadCallback)(char *, int)) {
	// loadFromUrl(url, loadCallback);
}

void gotoUrl(const char *url) {
// #ifdef SEMI_WIN32
// 	winGotoUrl(url);
// #endif
}
