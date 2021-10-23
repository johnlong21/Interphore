cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(Interphore CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS FALSE)

set(ENGINE_SRC
  "engine/src/main.cpp")
add_executable(Interphore ${ENGINE_SRC})

# Generate OGGs from WAVs

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
  PREPEND "${PROJECT_BINARY_DIR}/bin/assets/${WAV_DIR}"
  OUTPUT_VARIABLE AUDIO_OGGS)

foreach(AUDIO IN ZIP_LISTS AUDIO_WAVS AUDIO_OGGS)
  # Creat the directory for output
  get_filename_component(OUT_DIR ${AUDIO_1} DIRECTORY)

  get_filename_component(WAV_NAME ${AUDIO_0} NAME)

  add_custom_command(
    OUTPUT ${AUDIO_1}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${OUT_DIR}
    COMMAND ${FFMPEG} -i "${PROJECT_SOURCE_DIR}/sourceAssets/${AUDIO_0}" -loglevel error -qscale:a 2 ${AUDIO_1} -y)
endforeach()
add_custom_target(audio-ogg SOURCES ${AUDIO_OGGS})

file(GLOB_RECURSE ASSETS
  LIST_DIRECTORIES FALSE
  RELATIVE "${PROJECT_SOURCE_DIR}/sourceAssets/"
  CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/sourceAssets/*")
list(FILTER ASSETS EXCLUDE REGEX "(.wav)|(.mp3)|(.psd)|(/frames/)")
list(TRANSFORM ASSETS PREPEND "${PROJECT_BINARY_DIR}/bin/assets/" OUTPUT_VARIABLE RESULT_ASSETS)
foreach(ASSET IN ZIP_LISTS ASSETS RESULT_ASSETS)
  add_custom_command(
    OUTPUT "${ASSET_1}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/sourceAssets/${ASSET_0}" "${ASSET_1}"
  )
endforeach()
add_custom_target(assets SOURCES ${RESULT_ASSETS})
add_dependencies(assets audio-ogg)
