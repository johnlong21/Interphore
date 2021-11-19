cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(Interphore)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS FALSE)

set(GAME_SRC
  src/debugOverlay.cpp
  src/textArea.cpp
  src/unzipper.cpp
  src/game.cpp
  src/main.cpp)

set(ENGINE_SRC
  engine/src/perlin.cpp
  engine/src/base64.cpp
  engine/src/matrix.cpp
  engine/src/random.cpp
  engine/src/rect.cpp
  engine/src/point.cpp
  engine/src/mathTools.cpp
  engine/src/arrayTools.cpp
  engine/src/stringTools.cpp
  engine/src/memoryTools.cpp
  engine/src/asset.cpp
  engine/src/newSound.cpp
  engine/src/mintSprite.cpp
  engine/src/mintParticleSystem.cpp
  engine/src/tilemap.cpp
  engine/src/profiler.cpp
  engine/src/text.cpp
  engine/src/engine.cpp
  engine/src/replay.cpp
  engine/src/js.cpp
  engine/src/platform.cpp
  # Unused
  # engine/src/pathfinder.cpp
  )

set(ALL_SRC
  ${ENGINE_SRC} ${GAME_SRC})

if(WIN32)
  add_executable(interphore WIN32 ${ALL_SRC})
elseif(ANDROID)
  add_library(interphore SHARED ${ALL_SRC})
else()
  add_executable(interphore ${ALL_SRC})
endif()

target_compile_definitions(interphore PUBLIC STATIC_ASSETS SEMI_SOUND_NEW)
target_include_directories(interphore PUBLIC
  "${PROJECT_SOURCE_DIR}/engine/lib"
  "${PROJECT_SOURCE_DIR}/engine/src"
  "${PROJECT_SOURCE_DIR}/engine/include/libIncludes")

# === Generate OGGs from WAVs ===

file(GLOB_RECURSE AUDIO_WAVS
  LIST_DIRECTORIES FALSE
  CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/sourceAssets/audio/*")
find_program(FFMPEG "ffmpeg" REQUIRED)

list(TRANSFORM AUDIO_WAVS
  REPLACE "(.wav)|(.mp3)" ".ogg"
  OUTPUT_VARIABLE AUDIO_OGGS)
list(TRANSFORM AUDIO_OGGS
  REPLACE "${PROJECT_SOURCE_DIR}/sourceAssets/" "${PROJECT_BINARY_DIR}/assets/${WAV_DIR}"
  OUTPUT_VARIABLE AUDIO_OGGS)

foreach(AUDIO IN ZIP_LISTS AUDIO_WAVS AUDIO_OGGS)
  # Create the directory for output
  get_filename_component(OUT_DIR ${AUDIO_1} DIRECTORY)

  add_custom_command(
    OUTPUT "${AUDIO_1}"
    DEPENDS "${AUDIO_0}"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${OUT_DIR}
    COMMAND ${FFMPEG} -i "${AUDIO_0}" -loglevel error -qscale:a 2 ${AUDIO_1} -y)
endforeach()
add_custom_target(audio-ogg SOURCES ${AUDIO_WAVS} ${AUDIO_OGGS})

# === Copy all the other assets ===

file(GLOB_RECURSE ASSETS
  LIST_DIRECTORIES FALSE
  CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/sourceAssets/*")
list(FILTER ASSETS EXCLUDE REGEX "(.wav)|(.mp3)|(.psd)|(/frames/)")
list(TRANSFORM ASSETS REPLACE "${PROJECT_SOURCE_DIR}/sourceAssets/" "${PROJECT_BINARY_DIR}/assets/" OUTPUT_VARIABLE RESULT_ASSETS)
foreach(ASSET IN ZIP_LISTS ASSETS RESULT_ASSETS)
  if(ASSET_1 MATCHES "img/.*/.*_data/")
    list(REMOVE_ITEM RESULT_ASSETS ${ASSET_1})

    get_filename_component(ASSET_NAME ${ASSET_1} NAME_WE)
    string(REPLACE "${ASSET_NAME}_data/" "" ASSET_1 ${ASSET_1})

    list(APPEND RESULT_ASSETS ${ASSET_1})
  endif()
  add_custom_command(
    OUTPUT "${ASSET_1}"
    DEPENDS "${ASSET_0}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ASSET_0}" "${ASSET_1}"
  )
endforeach()
add_custom_target(assets ALL SOURCES ${ASSETS} ${RESULT_ASSETS})
add_dependencies(assets audio-ogg)
add_dependencies(assets interphore)

# === Build libraries

add_library(tinyxml2 STATIC ${PROJECT_SOURCE_DIR}/engine/lib/tinyxml2.cpp)
add_library(miniz STATIC ${PROJECT_SOURCE_DIR}/engine/lib/miniz.c)
add_library(duktape STATIC ${PROJECT_SOURCE_DIR}/engine/lib/duktape.c)

# === Find and add system dependencies

if(EMSCRIPTEN)
  target_compile_options(interphore PUBLIC "SHELL:-s USE_SDL=2")
  target_link_options(interphore PUBLIC
    "SHELL:-s USE_SDL=2 -s USE_WEBGL2=1 -s ALLOW_MEMORY_GROWTH=1 -s FETCH=1 -s ASYNCIFY=1 -s FORCE_FILESYSTEM=1 --emrun")

  set(HTML5_OUTPUT
    "${PROJECT_BINARY_DIR}/html5/index.html" "${PROJECT_BINARY_DIR}/html5/favicon.png"
    "${PROJECT_BINARY_DIR}/html5/interphore.js" "${PROJECT_BINARY_DIR}/html5/interphore.wasm"
    "${PROJECT_BINARY_DIR}/html5/assets.data" "${PROJECT_BINARY_DIR}/html5/assets.js")
  add_custom_command(TARGET assets POST_BUILD
    DEPENDS interphore
    COMMAND "${EMSCRIPTEN_ROOT_PATH}/tools/file_packager" assets.data --preload assets --js-output=assets.js &> /dev/null
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/engine/buildSystem/emscriptenTemplate.html" "${PROJECT_BINARY_DIR}/html5/index.html"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/res/appIcon.png" "${PROJECT_BINARY_DIR}/html5/favicon.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_BINARY_DIR}/interphore.js" "${PROJECT_BINARY_DIR}/interphore.wasm"
    "${PROJECT_BINARY_DIR}/assets.data" "${PROJECT_BINARY_DIR}/assets.js" "${PROJECT_BINARY_DIR}/html5/")
elseif(ANDROID)
  add_subdirectory("${PROJECT_SOURCE_DIR}/externalDeps/SDL")
  target_link_libraries(interphore SDL2::SDL2 SDL2::SDL2main)
  target_link_libraries(interphore GLESv2 EGL OpenSLES android log)

  add_custom_command(TARGET assets POST_BUILD
    DEPENDS interphore
    COMMAND COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_BINARY_DIR}/assets"
    "${CMAKE_SOURCE_DIR}/engine/buildSystem/android-project/app/src/main/assets/")
else()
  find_package(GLEW REQUIRED)
  find_package(OpenAL REQUIRED)
  find_package(OpenGL REQUIRED)
  find_package(SDL2 REQUIRED)
  find_package(CURL REQUIRED)

  target_link_libraries(interphore
    GLEW::GLEW OpenGL::GL SDL2::SDL2
    ${OPENAL_LIBRARY} ${CURL_LIBRARIES})
endif()
if(WIN32)
  set(WINDOWS_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/engine/include/")

  target_include_directories(interphore PUBLIC
    ${WINDOWS_INCLUDE_DIR}/alIncludes
    ${WINDOWS_INCLUDE_DIR}/curlIncludes
    ${WINDOWS_INCLUDE_DIR}/glIncludes
    ${WINDOWS_INCLUDE_DIR}/sdlIncludes)
endif()

target_link_libraries(interphore tinyxml2 miniz duktape)

if (NOT EMSCRIPTEN AND NOT ANDROID)
  add_subdirectory(externalDeps/nativefiledialog-extended)
  target_link_libraries(interphore nfd)
endif()

function(definition_and_variable)
  target_compile_definitions(interphore PUBLIC ${ARGV})

  foreach(var ${ARGV})
    set(${var} TRUE CACHE BOOL "" FORCE)
  endforeach()
endfunction()

# Platform defines
if(UNIX AND NOT ANDROID AND NOT APPLE AND NOT EMSCRIPTEN)
  definition_and_variable(
    SEMI_LINUX

    SEMI_SDL SEMI_GL_CORE SEMI_GL SEMI_AL SEMI_CURL_NETWORK
    SEMI_DESKTOP)
elseif(WIN32 OR MINGW)
  definition_and_variable(
    SEMI_WIN32

    SEMI_SDL SEMI_GL_CORE SEMI_GL SEMI_AL
    SEMI_CURL_NETWORK SEMI_DESKTOP _USE_MATH_DEFINES)
  if(MINGW)
    definition_and_variable(interphore SEMI_MINGW)
  endif()
elseif(ANDROID)
  definition_and_variable(
    SEMI_ANDROID

    SEMI_GLES SEMI_GL SEMI_EGL SEMI_SL SEMI_SDL)
  target_sources(interphore PUBLIC engine/src/androidPrintf.cpp)
elseif(EMSCRIPTEN)
  definition_and_variable(
    SEMI_HTML5

    SEMI_SDL SEMI_GL SEMI_AL SEMI_EM_NETWORK)
elseif(APPLE)
  definition_and_variable(
    SEMI_MAC

    SEMI_SDL SEMI_GL_CORE SEMI_GL SEMI_AL
    SEMI_CURL_NETWORK SEMI_DESKTOP)
endif()

if(SEMI_CURL_NETWORK)
  target_sources(interphore PUBLIC engine/src/curl_networking.cpp)
elseif(SEMI_EM_NETWORK)
  target_sources(interphore PUBLIC engine/src/em_networking.cpp)
endif()

if(SEMI_GL)
  target_sources(interphore PUBLIC engine/src/gl_renderer.cpp)
endif()
if(SEMI_SDL)
  target_sources(interphore PUBLIC engine/src/sdl2_platform.cpp)
endif()
