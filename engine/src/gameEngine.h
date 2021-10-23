#pragma once

/// Game
/// Debugging
// #define LOG_ASSET_ADD
// #define LOG_ENGINE_UPDATE
// #define LOG_ASSET_GET
// #define LOG_ASSET_EVICT
// #define LOG_SOUND_GEN
// #define LOG_TEXTURE_GEN
// #define LOG_TEXTURE_STREAM
// #define LOG_MALLOC
// #define LOG_FREE


#ifndef SEMI_RELEASE
# ifndef SEMI_DEV
#  define SEMI_INTERNAL
# endif
#endif

#ifdef SEMI_INTERNAL
# ifndef SEMI_DEV
#  define SEMI_DEV
# endif
# ifndef SEMI_EARLY
#  define SEMI_EARLY
# endif
#endif

#ifdef SEMI_DEV
# define SEMI_DEBUG
#endif

#ifdef _MSC_VER
# define SEMI_WIN32

# ifdef SEMI_INTERNAL
#  define SEMI_VS_LEAK_CHECK
#  define _CRTDBG_MAP_ALLOC
#  include <stdlib.h>
#  include <crtdbg.h>
# endif

# include <windows.h>
#endif

#ifdef __linux__
# define SEMI_LINUX
#endif

#ifdef __FLASHPLAYER__
# define SEMI_FLASH
#endif

#ifdef __ANDROID__
# undef SEMI_LINUX
# define SEMI_ANDROID
#endif

#ifdef __APPLE__
# define SEMI_MAC
#endif

#ifdef __EMSCRIPTEN__
# define SEMI_HTML5
#endif

#ifdef SEMI_WIN32
# define WIN32_LEAN_AND_MEAN
# define SEMI_SDL
# define SEMI_GL_CORE
# define SEMI_GL
# define SEMI_AL
# define SEMI_CURL_NETWORK
#endif

#ifdef SEMI_LINUX
# define SEMI_SDL
# define SEMI_GL_CORE
# define SEMI_GL
# define SEMI_AL
# define SEMI_CURL_NETWORK
#endif

#ifdef SEMI_HTML5
// # define SEMI_GLFW
# define SEMI_SDL
// # define SEMI_GL_CORE
# define SEMI_GL
# define SEMI_STUB_SOUND
# define SEMI_EM_NETWORK
#endif

#ifdef SEMI_MAC
# define SEMI_SDL
# define SEMI_GL_CORE
# define SEMI_GL
# undef SEMI_AL
# define SEMI_STUB_SOUND
# define SEMI_CURL_NETWORK
#endif

#ifdef SEMI_ANDROID
# define SEMI_CURL_NETWORK
# define SEMI_GL_CORE
# define SEMI_GLES
# define SEMI_GL
# define SEMI_EGL
# define SEMI_SL
#endif

#ifndef SEMI_WIN32
# include <pthread.h>
// # define SEMI_MULTITHREAD
#endif

#define __STDC_LIMIT_MACROS
#include <stdint.h>

#if 0
# define STB_LEAKCHECK_IMPLEMENTATION
# include "stb_leakcheck.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "tinyxml2.h"
#include "miniz.h"

#ifdef SEMI_SDL
# include <SDL2/SDL.h>
# define GLEW_STATIC
# include <GL/glew.h>
# include <SDL2/SDL_opengl.h>
#endif

#ifdef SEMI_HTML5
# include <emscripten/emscripten.h>
#endif

#ifdef SEMI_FLASH
# include <AS3/AS3.h>
#endif

#include "defines.h"
#include "perlin.cpp"
#include "base64.cpp"
#include "matrix.cpp"
#include "random.cpp"
#include "map.cpp"
#include "rect.cpp" //@hack This must go after map.cpp to redefine x.inflate() from map.cpp importing miniz
#include "pathfinder.cpp"
#include "point.cpp"
#include "mathTools.cpp"
#include "arrayTools.cpp"
#include "stringTools.cpp"
#include "memoryTools.cpp"
#include "strings.cpp"

#ifdef SEMI_WIN32
# include "win32_platform.cpp"
#endif

#include "asset.cpp" // Assets after platform because of gl stuff?

#ifdef SEMI_CURL_NETWORK
# include "curl_networking.cpp"
#endif

#ifdef SEMI_EM_NETWORK
# include "em_networking.cpp"
#endif

#ifndef SEMI_SOUND_NEW
# ifdef SEMI_AL
#  include "al_sound.cpp"
# endif

# ifdef SEMI_SL
#  include "sl_sound.cpp"
# endif

# ifdef SEMI_STUB_SOUND
#  include "stub_sound.cpp"
# endif
#endif

#ifdef SEMI_SDL
# include "sdl2_platform.cpp"
#endif

#include "platform.cpp"

#ifdef SEMI_GLFW
# include "glfw_platform.cpp"
#endif

#ifdef SEMI_FLASH
# include "flash_platform.cpp"
# include "flash_renderer.cpp"
# ifndef SEMI_SOUND_NEW
#  include "flash_sound.cpp"
# endif
#endif

#ifdef SEMI_ANDROID
# include "android_platform.cpp"
#endif

#ifdef SEMI_GL
# include "gl_renderer.cpp"
#endif

#ifdef SEMI_SOUND_NEW
# include "newSound.cpp"
#else
# include "sound.cpp"
#endif

#include "mintSprite.cpp"
#include "mintParticleSystem.cpp"
#include "tilemap.cpp"
#include "profiler.cpp"
#include "text.cpp"
#include "engine.cpp"
#include "replay.cpp"
#include "js.cpp"
