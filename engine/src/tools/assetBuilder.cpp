#ifdef _MSC_VER
#include <windows.h>
#endif

#ifdef __linux__
#include <unistd.h>
#include <dirent.h>
#endif

#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include "../defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define STB_SPRINTF_IMPLEMENTATION 
#include "stb_sprintf.h"

void collectAssets(const char *name);
void processFile(char *path);
bool checkIfSame(const char *fname, const char *str);

char *assetData;

struct AssetDef {
	char name[PATH_LIMIT];
	char legalPathName[PATH_LIMIT];
	char *data;
	int dataLen;
};

char *fileStr;
AssetDef assets[ASSET_LIMIT];
int assetsNum = 0;

char *compareString;
int compareStringLen;

// #ifdef _MSC_VER
// INT WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) {
// #else
int main(int argc, char **argv) {
	// #endif
	// collectAssets("assets");
	collectAssets("bin/assets");
	fileStr = (char *)malloc(Megabytes(5));
	compareString = (char *)malloc(Megabytes(5));
	int fileStrLen;

	FILE *masterFile = fopen("bin/assetCache/master.cpp", "wb");
	fprintf(masterFile, "#include <stdio.h>\n\n");
	fprintf(masterFile, "void assetLibInit() {\n");

	for (int i = 0; i < assetsNum; i++) {
		AssetDef *asset = &assets[i];
		// printf("file: %s(%s) was %d bytes\n", asset->name, asset->legalPathName, asset->dataLen);
		// memset(fileStr, 0, Megabytes(5));
		fileStr[0] = 0;
		fileStrLen = 0;
		fileStrLen += stbsp_sprintf(fileStr+fileStrLen, "#include <stdio.h>\n\n");
		fileStrLen += stbsp_sprintf(fileStr+fileStrLen, "void addAsset(const char *assetName, const char *base64, unsigned int base64Len);\n\n");
		fileStrLen += stbsp_sprintf(fileStr+fileStrLen, "void embed_%s() {\n", asset->legalPathName);
		fileStrLen += stbsp_sprintf(fileStr+fileStrLen, "static unsigned char %s[] = {\n", asset->legalPathName);
		for (int j = 0; j < asset->dataLen; j++)
			fileStrLen += stbsp_sprintf(fileStr+fileStrLen, "0x%02X,", asset->data[j] & 0xFF);
		fileStrLen += stbsp_sprintf(fileStr+fileStrLen, "};\n");
		fileStrLen += stbsp_sprintf(fileStr+fileStrLen, "int size = %d;\n", asset->dataLen);
		fileStrLen += stbsp_sprintf(fileStr+fileStrLen, "const char *name = \"%s\";\n", asset->name);
		fileStrLen += stbsp_sprintf(fileStr+fileStrLen, "addAsset(name, (const char *)%s, size);\n", asset->legalPathName);
		// assetAdd("assets/font/bitmap/Espresso-Dolce_50_0.png", (char *)assets_font_bitmap_Espresso_Dolce_50_0_png, assets_font_bitmap_Espresso_Dolce_50_0_png_len);
		fileStrLen += stbsp_sprintf(fileStr+fileStrLen, "}\n");

		char realPath[LARGE_STR];
#ifdef _MSC_VER
		stbsp_sprintf(realPath, "bin\\assetCache\\%s.cpp", asset->legalPathName);
#else
		stbsp_sprintf(realPath, "bin/assetCache/%s.cpp", asset->legalPathName);
#endif

		bool needsUpdate = true;

#if 0
		needsUpdate = !checkIfSame(realPath, fileStr);
#else
		FILE *compareFile = fopen(realPath, "r");
		if (compareFile) {
		fseek(compareFile, 0, SEEK_END);
		compareStringLen = ftell(compareFile);
		fseek(compareFile, 0, SEEK_SET);  //same as rewind(f);

		fread(compareString, 1, compareStringLen, compareFile);
		fclose(compareFile);
		compareString[compareStringLen] = '\0';

		// printf("comp: \n%s\n---\n%s\n", compareString, fileStr);
		// free(compareString);
		// exit(1);

		if (strncmp(compareString, fileStr, strlen(fileStr)) == 0) needsUpdate = false;
		}
#endif

		if (needsUpdate) {
			printf("Needs update %s\n", realPath);
			FILE *curFile = fopen(realPath, "wb");
			fprintf(curFile, "%s", fileStr);
			fclose(curFile);
		}

		fprintf(masterFile, "void embed_%s();\n", asset->legalPathName);
		fprintf(masterFile, "embed_%s();\n", asset->legalPathName);
	}

	fprintf(masterFile, "}\n");
	fclose(masterFile);

	free(fileStr);
}

#ifdef _MSC_VER
void collectAssets(const char *dirn) {
	char dirnPath[PATH_LIMIT];
	stbsp_sprintf(dirnPath, "%s\\*", dirn);

	WIN32_FIND_DATA f;
	HANDLE h = FindFirstFile(dirnPath, &f);

	if (h == INVALID_HANDLE_VALUE) return;

	do {
		const char *name = f.cFileName;

		if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

		char filePath[1024];
		stbsp_sprintf(filePath, "%s%s%s", dirn, "\\", name);

		if (!(f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) processFile(filePath);
		if (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) collectAssets(filePath);

	} while (FindNextFile(h, &f));

	FindClose(h);
}
#endif

#ifdef __linux__
void collectAssets(const char *name) {
	DIR *dir;
	dirent *entry;

	if (!(dir = opendir(name))) return;

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_type == DT_DIR) {
			char path[PATH_LIMIT+2];
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
			snprintf(path, PATH_LIMIT+2, "%s/%s", name, entry->d_name);
			collectAssets(path);
		} else {
			char fullPath[PATH_LIMIT+2];
			snprintf(fullPath, PATH_LIMIT+2, "%s/%s", name, entry->d_name);
			processFile(fullPath);
		}
	}
	closedir(dir);
}
#endif

void processFile(char *path) {
	FILE *fileptr = fopen(path, "rb");
	fseek(fileptr, 0, SEEK_END);
	int fileLen = ftell(fileptr);
	rewind(fileptr);

	char *buffer = (char *)malloc((fileLen+1)*sizeof(char));
	fread(buffer, fileLen, 1, fileptr);
	fclose(fileptr);
	
	AssetDef *asset = &assets[assetsNum++];
	strcpy(asset->name, path);
	strcpy(asset->legalPathName, path);
	asset->data = buffer;
	asset->dataLen = fileLen;

	for (int i = 0; i < strlen(asset->name); i++)
		if (asset->name[i] == '\\')
			asset->name[i] = '/';

	for (int i = 0; i < strlen(asset->legalPathName); i++)
		if (asset->legalPathName[i] == '/' || asset->legalPathName[i] == '.' || asset->legalPathName[i] == '-' || asset->legalPathName[i] == '\\')
			asset->legalPathName[i] = '_';
}

bool checkIfSame(const char *fname, const char *str) {
	FILE *fp = fopen(fname, "r");
	int strLen = strlen(str);
	int curChar = 0;

	int byteOn = 0;
	if (fp == NULL) {
		// printf("Couldn't open file %s\n", fname);
		return false;
	} else {
		int ch1 = getc(fp);
		int ch2 = str[curChar++];

		while ((ch1 != EOF) && (ch2 != '\0') && (ch1 == ch2)) {
			ch1 = getc(fp);
			ch2 = str[curChar++];
			byteOn++;
		}

		if (ch1 != ch2 && (ch2 != '\0')) {
			// printf("Failed on byte %d\n", byteOn);
			fclose(fp);
			return false;
		}
	}

	// printf("Files are same\n");
	fclose(fp);
	return true;
}
