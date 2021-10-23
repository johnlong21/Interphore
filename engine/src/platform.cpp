#include "platform.h"

#if defined(SEMI_LINUX) || defined(SEMI_ANDROID)
#include <unistd.h>
#include <dirent.h>
#endif

void getDirList(const char *dirn, char **pathNames, int *pathNamesNum) {
#ifdef SEMI_WIN32
	char dirnPath[PATH_LIMIT];
	sprintf(dirnPath, "%s\\*", dirn);

	WIN32_FIND_DATA f;
	HANDLE h = FindFirstFile(dirnPath, &f);

	if (h == INVALID_HANDLE_VALUE) return;

	do {
		const char *name = f.cFileName;

		if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

		char filePath[1024];
		sprintf(filePath, "%s%s%s", dirn, "\\", name);

		if (!(f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			for (int i = 0; i < strlen(filePath); i++)
				if (filePath[i] == '\\') filePath[i] = '/';
			pathNames[*pathNamesNum] = stringClone(filePath);
			*pathNamesNum = *pathNamesNum + 1;
		}
		if (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			getDirList(filePath, pathNames, pathNamesNum);
		}

	} while (FindNextFile(h, &f));

	FindClose(h);
#endif

#ifdef	SEMI_ANDROID
	AAsset *aListFile = AAssetManager_open(assetManager, "fullAssetList.txt", AASSET_MODE_UNKNOWN);
	Assert(aListFile);

	long fileSize = AAsset_getLength(aListFile);
	char *assetsString = (char *)Malloc(fileSize + 1);

	AAsset_read(aListFile, assetsString, fileSize);
	assetsString[fileSize] = '\0';

	char *fileStart = assetsString;
	for (;;) {
		char *fileEnd = strstr(fileStart, "\n");
		bool lastEntry = false;
		if (!fileEnd) {
			fileEnd = &assetsString[strlen(assetsString)-1];
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

		if (strstr(fileName, dirn)) {
			pathNames[*pathNamesNum] = stringClone(fileName);
			*pathNamesNum = *pathNamesNum + 1;
		}

		fileStart = fileEnd + 1;
		if (lastEntry) break;
	}

	Free(assetsString);
	AAsset_close(aListFile);

#endif

#ifdef SEMI_LINUX
	DIR *dir;
	dirent *entry;

	if (!(dir = opendir(dirn))) return;

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_type == DT_DIR) {
			char path[PATH_LIMIT+2];
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
			snprintf(path, PATH_LIMIT+2, "%s/%s", dirn, entry->d_name);
			getDirList(path, pathNames, pathNamesNum);
		} else {
			char fullPath[PATH_LIMIT+2];
			snprintf(fullPath, PATH_LIMIT+2, "%s/%s", dirn, entry->d_name);
			pathNames[*pathNamesNum] = stringClone(fullPath);
			*pathNamesNum = *pathNamesNum + 1;
		}
	}
	closedir(dir);
#endif

#ifdef SEMI_FLASH
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
#endif
}

void writeFile(const char *filename, const char *str) {
	FILE *f = fopen(filename, "w");

	if (f == NULL) {
		printf("Failed to open file %s to write\n", filename);
		return;
	}

	fprintf(f, "%s", str);
	fclose(f);
}

bool fileExists(const char *filename) {
	FILE *f = fopen(filename, "r");

	if (f) {
		fclose(f);
		return true;
	}

	return false;
}


long readFile(const char *filename, void **storage) {
#ifdef SEMI_ANDROID
	AAsset *aFile = AAssetManager_open(assetManager, filename, AASSET_MODE_UNKNOWN);
	Assert(aFile);

	long fileSize = AAsset_getLength(aFile);
	char *str = (char *)Malloc(fileSize + 1);

	AAsset_read(aFile, str, fileSize);
	str[fileSize] = '\0';
	AAsset_close(aFile);

	*storage = str;
	return fileSize;
#else
	FILE *filePtr = fopen(filename, "rb");
	if (!filePtr) return 0;

	fseek(filePtr, 0, SEEK_END);
	long fileSize = ftell(filePtr);
	fseek(filePtr, 0, SEEK_SET);  //same as rewind(f);

	char *str = (char *)malloc(fileSize + 1);
	fread(str, 1, fileSize, filePtr);
	fclose(filePtr);

	str[fileSize] = '\0';

	*storage = str;

	return fileSize;
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
