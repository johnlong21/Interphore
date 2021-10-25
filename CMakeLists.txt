cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(Interphore CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS FALSE)

set(ENGINE_SRC
  "src/main.cpp")

if(WIN32)
  add_executable(interphore WIN32 ${ENGINE_SRC})
else()
  add_executable(interphore ${ENGINE_SRC})
endif()

target_compile_definitions(interphore PUBLIC STATIC_ASSETS)
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
add_custom_target(assets SOURCES ${ASSETS} ${RESULT_ASSETS})
add_dependencies(assets audio-ogg)
add_dependencies(interphore assets)

# === Build libraries

add_library(tinyxml2 STATIC ${PROJECT_SOURCE_DIR}/engine/lib/tinyxml2.cpp)
add_library(miniz STATIC ${PROJECT_SOURCE_DIR}/engine/lib/miniz.c)
add_library(duktape STATIC ${PROJECT_SOURCE_DIR}/engine/lib/duktape.c)

# === Find and add system dependencies

if(${CMAKE_C_COMPILER} MATCHES "emcc")
  set(EMSCRIPTEN TRUE)
endif()

if(${EMSCRIPTEN})
  target_compile_options(interphore PUBLIC "SHELL:-s USE_SDL=2")
  target_link_options(interphore PUBLIC
    "SHELL:-s USE_SDL=2 -s USE_WEBGL2=1 -s ALLOW_MEMORY_GROWTH=1 -s FETCH=1 -s ASYNCIFY=1 --emrun --preload-file assets")

  set_target_properties(interphore PROPERTIES SUFFIX ".html")

  # add_custom_command(TARGET interphore POST_BUILD
  #   COMMAND
  #   ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/engine/buildSystem/emscriptenTemplate.html" "${CMAKE_BINARY_DIR}/index.html")
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
