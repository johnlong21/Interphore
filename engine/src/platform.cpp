#include "platform.h"

#include <filesystem>
#include <sstream>
#include <fstream>

#ifdef SEMI_ANDROID
#include <regex>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

AAssetManager *assetManager;
#endif

void getDirList(const char *dirn, char **pathNames, int *pathNamesNum) {
#if defined(SEMI_ANDROID)
    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();

    auto context_object = (jobject)SDL_AndroidGetActivity();
    auto getAssets_method = env->GetMethodID(env->GetObjectClass(context_object), "getAssets", "()Landroid/content/res/AssetManager;");
    auto assetManager_object = env->CallObjectMethod(context_object, getAssets_method);
    auto list_method = env->GetMethodID(env->GetObjectClass(assetManager_object), "list", "(Ljava/lang/String;)[Ljava/lang/String;");

    assetManager = AAssetManager_fromJava(env, assetManager_object);

    std::function<void (const char *,char **, int *)> android_iterate = [&](const char *dirn, char **pathNames, int *pathNamesNum) {
        jstring path_object = env->NewStringUTF(dirn);

        auto files_object = (jobjectArray)env->CallObjectMethod(assetManager_object, list_method, path_object);

        env->DeleteLocalRef(path_object);

        auto length = env->GetArrayLength(files_object);

        for (int i = 0; i < length; i++)
        {
            jstring jstr = (jstring)env->GetObjectArrayElement(files_object, i);

            const char * filename = env->GetStringUTFChars(jstr, nullptr);

            if (filename != nullptr) {
                std::string strDirname(dirn);
                std::string strFilename(filename);

                std::filesystem::path filePath;
                if (strDirname != "") {
                    filePath = std::filesystem::path(dirn) / strFilename;
                } else {
                    filePath = strFilename;
                }

                auto filePathString = filePath.string();
                const char *filePathCStr = filePathString.c_str();
                AAsset *asset = AAssetManager_open(assetManager, filePathCStr, AASSET_MODE_UNKNOWN);

                if (asset == nullptr) {
                    // Is a directory
                    android_iterate(filePathCStr, pathNames, pathNamesNum);
                } else {
                    // Add "assets" at the beginning
                    filePathString = std::filesystem::path("assets") / filePathString;
                    filePathCStr = filePathString.c_str();

                    pathNames[*pathNamesNum] = stringClone(filePathCStr);
                    *pathNamesNum = *pathNamesNum + 1;

                    AAsset_close(asset);
                }

                env->ReleaseStringUTFChars(jstr, filename);
            }

            env->DeleteLocalRef(jstr);
        }
    };
    android_iterate("", pathNames, pathNamesNum);
#elif defined(SEMI_FLASH)
    char *filesStr = (char *)Malloc(Megabytes(1));
	inline_as3("Console.getAssetNames(%0);" :: "r"(filesStr));

	char *fileStart = filesStr;
	for (;;) {
		char *fileEnd = strstr(fileStart, ",");
		bool lastEntry = false;
		if (!fileEnd) {
			fileEnd = &filesStr[strlen(filesStr)-1];
			lastEntry = true;
		}

		char fileName[PATH_LIMIT];

		if (!lastEntry) {
			fileName[fileEnd - fileStart] = '\0';
			strncpy(fileName, fileStart, fileEnd - fileStart);
		} else {
			fileName[fileEnd - fileStart + 1] = '\0';
			strncpy(fileName, fileStart, fileEnd - fileStart+1);
		}

		if (strstr(fileName, ".")) {
			pathNames[*pathNamesNum] = stringClone(fileName);
			*pathNamesNum = *pathNamesNum + 1;
		}

		fileStart = fileEnd + 1;
		if (lastEntry) break;
	}

	Free(filesStr);
#else
    std::filesystem::path dir_path = dirn;
    if (!std::filesystem::exists(dir_path)) {
        printf("Asset directory %s does not exist", dirn);

        return;
    }
    for (auto const &entry : std::filesystem::recursive_directory_iterator(dir_path)) {
        if (!std::filesystem::is_directory(entry.path())) {
            pathNames[*pathNamesNum] = stringClone(entry.path().string().c_str());
            *pathNamesNum = *pathNamesNum + 1;
        }
    }
#endif
}

void writeFile(const char *filename, const char *str) {
    std::ofstream file(filename);
    if (!file.good()) {
		printf("Failed to open file %s to write\n", filename);
		return;
    }

    file << std::string(str);
}

bool fileExists(const char *filename) {
    return std::filesystem::exists(filename);
}


long readFile(const char *filename, void **storage) {
#ifdef SEMI_ANDROID
    std::string strFilename = std::regex_replace(filename, std::regex("assets/"), "");

	AAsset *aFile = AAssetManager_open(assetManager, strFilename.c_str(), AASSET_MODE_UNKNOWN);
	Assert(aFile);

	long fileSize = AAsset_getLength(aFile);
	char *str = (char *)Malloc(fileSize + 1);

	AAsset_read(aFile, str, fileSize);
	str[fileSize] = '\0';
	AAsset_close(aFile);

	*storage = str;
	return fileSize;
#else
    std::ifstream fileStream(filename, std::ios_base::in | std::ios_base::binary);
    if (!fileStream.good())
        return 0;

    // Read the file into a string
    std::stringstream contents;
    contents << fileStream.rdbuf();

    // + 1 for the null byte
    auto fileLength = long(fileStream.tellg()) + 1;

    // Copy the string into a newly allocated place
    *storage = Malloc(fileLength);
    memcpy(*storage, contents.str().data(), fileLength);

    return fileLength;
#endif
}

void getNanoTime(NanoTime *time) {
#ifdef _WIN32
	LARGE_INTEGER winTime;
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq); 
	QueryPerformanceCounter(&winTime);
	unsigned int nanos = winTime.QuadPart * 1000000000 / freq.QuadPart;
	time->seconds = nanos / 1000000000;
	time->nanos = nanos % 1000000000;

	// time->time = StartingTime;
	// time->winFreq = Frequency;
#elif __linux__
	timespec ts;
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
	time->seconds = ts.tv_sec;
	time->nanos = ts.tv_nsec;
#else
	timespec ts;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
	time->seconds = ts.tv_sec;
	time->nanos = ts.tv_nsec;
#endif
}
