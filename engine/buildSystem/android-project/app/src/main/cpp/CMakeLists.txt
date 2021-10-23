cmake_minimum_required(VERSION 3.4.1)

set(${CMAKE_C_FLAGS}, "${CMAKE_C_FLAGS}")
add_library(native_app_glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

add_definitions(-D__ANDROID__)

include_directories(../../../../../../src)
include_directories(
	replaceEnginePath/lib
	replaceEnginePath/include/libIncludes
	replaceEnginePath/include/curlIncludes
	replaceEnginePath/src
	)


FILE(
	GLOB AssetSources
	../../../../../../bin/androidAssetCache/*.c
	../../../../../../bin/androidAssetCache/*.cpp
	)

add_library(
	semiphore SHARED
	androidMain.cpp
	${AssetSources}
	)

target_include_directories(semiphore PRIVATE ${ANDROID_NDK}/sources/android/native_app_glue)

target_link_libraries(semiphore
	native_app_glue
	android
	log 
	EGL
	GLESv3
	OpenSLES
	z
	${CMAKE_SOURCE_DIR}/../libcurl/armeabi-v7a/libcurl.a
	)
