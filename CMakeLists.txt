cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(Interphore CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS FALSE)

set(ENGINE_SRC
  "src/main.cpp")
add_executable(interphore ${ENGINE_SRC})
target_compile_definitions(interphore PUBLIC
  DESKTOP_BUILD STATIC_ASSETS)
target_include_directories(interphore PUBLIC
  "${PROJECT_SOURCE_DIR}/engine/lib"
  "${PROJECT_SOURCE_DIR}/engine/src"
  "${PROJECT_SOURCE_DIR}/engine/include/libIncludes")

# === Generate OGGs from WAVs ===

file(GLOB_RECURSE AUDIO_WAVS
  LIST_DIRECTORIES FALSE
  RELATIVE "${PROJECT_SOURCE_DIR}/sourceAssets/"
  CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/sourceAssets/audio/*")
find_program(FFMPEG "ffmpeg" REQUIRED)

list(TRANSFORM AUDIO_WAVS
  REPLACE "(.wav)|(.mp3)" ".ogg"
  OUTPUT_VARIABLE AUDIO_OGGS)
list(TRANSFORM AUDIO_OGGS
  PREPEND "${PROJECT_BINARY_DIR}/assets/${WAV_DIR}"
  OUTPUT_VARIABLE AUDIO_OGGS)

foreach(AUDIO IN ZIP_LISTS AUDIO_WAVS AUDIO_OGGS)
  # Create the directory for output
  get_filename_component(OUT_DIR ${AUDIO_1} DIRECTORY)

  add_custom_command(
    OUTPUT ${AUDIO_1}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${OUT_DIR}
    COMMAND ${FFMPEG} -i "${PROJECT_SOURCE_DIR}/sourceAssets/${AUDIO_0}" -loglevel error -qscale:a 2 ${AUDIO_1} -y)
endforeach()
add_custom_target(audio-ogg SOURCES ${AUDIO_OGGS})

# === Copy all the other assets ===

file(GLOB_RECURSE ASSETS
  LIST_DIRECTORIES FALSE
  RELATIVE "${PROJECT_SOURCE_DIR}/sourceAssets/"
  CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/sourceAssets/*")
list(FILTER ASSETS EXCLUDE REGEX "(.wav)|(.mp3)|(.psd)|(/frames/)")
list(TRANSFORM ASSETS PREPEND "${PROJECT_BINARY_DIR}/assets/" OUTPUT_VARIABLE RESULT_ASSETS)
foreach(ASSET IN ZIP_LISTS ASSETS RESULT_ASSETS)
  if(ASSET_1 MATCHES "img/.*/.*_data/")
    list(REMOVE_ITEM RESULT_ASSETS ${ASSET_1})

    get_filename_component(ASSET_NAME ${ASSET_1} NAME_WE)
    string(REPLACE "${ASSET_NAME}_data/" "" ASSET_1 ${ASSET_1})

    list(APPEND RESULT_ASSETS ${ASSET_1})
  endif()
  add_custom_command(
    OUTPUT "${ASSET_1}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/sourceAssets/${ASSET_0}" "${ASSET_1}"
  )
endforeach()
add_custom_target(assets SOURCES ${RESULT_ASSETS})
add_dependencies(assets audio-ogg)
add_dependencies(interphore assets)

# === Build libraries

add_library(tinyxml2 STATIC ${PROJECT_SOURCE_DIR}/engine/lib/tinyxml2.cpp)
add_library(miniz STATIC ${PROJECT_SOURCE_DIR}/engine/lib/miniz.c)
add_library(duktape STATIC ${PROJECT_SOURCE_DIR}/engine/lib/duktape.c)

# === Find and add system dependencies

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenAL REQUIRED)
find_package(SDL2 REQUIRED)
find_package(CURL REQUIRED)

target_link_libraries(interphore
  tinyxml2 miniz duktape
  GLEW::GLEW SDL2::SDL2 OpenGL::GL
  ${OPENAL_LIBRARY} ${CURL_LIBRARIES})
