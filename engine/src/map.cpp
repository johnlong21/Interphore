#include "map.h"
#include "stringTools.h"
#include "memoryTools.h"
#include "miniz.h"

const char *stringMapRecordSeperator = "\n__END__ENTRY__\n";

StringMap *stringMapCreate(int startLen) {
	StringMap *map = (StringMap *)Malloc(sizeof(StringMap));
	map->keys = (char **)Malloc(startLen * sizeof(char *));
	map->values = (void **)Malloc(startLen * sizeof(void *));
	map->types = (MapValueType *)Malloc(startLen * sizeof(MapValueType));
	map->maxLen = startLen;
	map->length = 0;

	return map;
}

bool StringMap::isNull(const char *key) {
	StringMap *map = this;

	for (int i = 0; i < map->length; i++)
		if (strcmp(map->keys[i], key) == 0)
			return map->values[i] == NULL;

	return true;
}

bool StringMap::exists(const char *key) {
	StringMap *map = this;

	for (int i = 0; i < map->length; i++)
		if (strcmp(map->keys[i], key) == 0)
			return true;

	return false;
}

int StringMap::indexOf(const char *key) {
	StringMap *map = this;

	for (int i = 0; i < map->length; i++)
		if (strcmp(map->keys[i], key) == 0)
			return i;

	return -1;
}

char *StringMap::serialize() {
	StringMap *map = this;

	unsigned char *mapStr = (unsigned char *)Malloc(SERIAL_SIZE);
	memset(mapStr, 0, SERIAL_SIZE);

	for (int i = 0; i < map->length; i++) {
		char tempStr[10 * 1024];
		// if (map->types[i] == MAP_STRING) printf("Saving key %s to value %s (type string)\n", map->keys[i], map->values[i]);
		// if (map->types[i] == MAP_INT) printf("Saving key %s to value %d (type int)\n", map->keys[i], map->values[i]);
		// if (map->types[i] == MAP_FLOAT) printf("Saving key %s to value %f (type float)\n", map->keys[i], map->values[i]);
		// if (map->types[i] == MAP_LONG) printf("Saving key %s to value %d (type long)\n", map->keys[i], map->values[i]);

		if (map->types[i] == MAP_LONG) sprintf(tempStr, "%s %s = %lld", (char *)"long", map->keys[i], (long long)map->values[i]);
		else if (map->types[i] == MAP_INT) sprintf(tempStr, "%s %s = %d", (char *)"int", map->keys[i], (int)(long long)map->values[i]);
		else if (map->types[i] == MAP_FLOAT) sprintf(tempStr, "%s %s = %f", (char *)"float", map->keys[i], *(float *)&map->values[i]);
		else if (map->types[i] == MAP_STRING) sprintf(tempStr, "%s %s = %s", (char *)"char*", map->keys[i], (char *)map->values[i]);
		else strcpy(tempStr, "error");

		strcat((char *)mapStr, tempStr);
		strcat((char *)mapStr, stringMapRecordSeperator);
	}

	// printf("Serializing string:\n%s\n", mapStr);
	unsigned char *compressedResult = (unsigned char *)zalloc(SERIAL_SIZE);
	size_t compressMaxLen = compressBound(SERIAL_SIZE * 1.001 + 12); // Whatever I guess, zlib told me to
	int compressStatus = compress(compressedResult, (mz_ulong *)&compressMaxLen, mapStr, strlen((char *)mapStr));
	Free(mapStr);
	Assert(compressStatus == Z_OK);

#if 1
	char *finalResult = (char *)zalloc(SERIAL_SIZE);
	for (int i = 0; i < compressMaxLen; i++) {
		char byteStr[16] = {}; //Should this be 8?
		sprintf(byteStr,"%x:", compressedResult[i]);
		strcat(finalResult, byteStr);
	}

	Free(compressedResult);
	return (char *)finalResult;
#else
	size_t outputLen;
	unsigned char *b64 = base64_encode(compressedResult, compressMaxLen, &outputLen);
	unsigned char *b64Str = (unsigned char *)Malloc(outputLen + 1);
	memcpy(b64Str, b64, outputLen);
	b64Str[outputLen] = '\0';

	Free(b64);
	Free(compressedResult);

	return (char *)b64Str;
#endif
}

void StringMap::deserialize(char *serial) {

	StringMap *map = this;
	for (int i = 0; i < map->length; i++) Free(map->keys[i]);
	map->length = 0;

	unsigned char *compressedData;
	unsigned long compressedDataLen = 0;

#if 1
	compressedData = (unsigned char *)zalloc(SERIAL_SIZE);
	char *nextColon = NULL;
	char *lastPos = serial;
	for (int i = 0;; i++) {
		nextColon = strchr(lastPos, ':');
		if (!nextColon) break;

		char byteStr[8] = {};
		strncpy(byteStr, lastPos, nextColon-lastPos);
		// printf("Byte is: %s\n", byteStr);
		compressedData[compressedDataLen++] = strtoul(byteStr, NULL, 16);
		lastPos = nextColon+1;
	}
#else
	compressedData = base64_decode((unsigned char *)serial, strlen(serial), (size_t *)&compressedDataLen);
#endif

	// printf("bytes parsed(%d): %s\n", compressedDataLen, compressedData);
	char *uncompressedResult = (char *)Malloc(SERIAL_SIZE);
	unsigned long uncompressMaxLen = compressBound(SERIAL_SIZE * 1.001 + 12); // Whatever I guess, zlib told me to
	int uncompressStatus = uncompress((unsigned char *)uncompressedResult, &uncompressMaxLen, (const unsigned char *)compressedData, compressedDataLen);
	Free(compressedData);
	Assert(uncompressStatus == Z_OK);

	char *uncompressedResultStr = (char *)Malloc((uncompressMaxLen + 1) * sizeof(char));
	memcpy(uncompressedResultStr, uncompressedResult, uncompressMaxLen);
	uncompressedResultStr[uncompressMaxLen] = '\0';

	Free(uncompressedResult);

	serial = uncompressedResultStr;

	// printf("Deserializing string:\n%s\n", serial);

	int serialLen = strlen(serial);
	char *lineStart = serial;
	for (int i = 0;; i++) {
		char key[MED_STR] = {};
		char type[MED_STR] = {};
		char value[LARGE_STR] = {};
		char *lineEnd = strstr(lineStart, stringMapRecordSeperator);
		char *typeEnd = strstr(lineStart, " ");
		char *keyEnd = strstr(typeEnd, " = ");
		// char *valueEnd = strstr(keyEnd, stringMapRecordSeperator);
		strncpy(type, lineStart, typeEnd - lineStart);
		strncpy(key, typeEnd+1, keyEnd - typeEnd - 1);
		strncpy(value, keyEnd+3, lineEnd - keyEnd - 3);
		// printf("type:%s|key:%s|value:%s|\n", type, key, value);

		if (streq(type, "long")) map->setLong(key, atol(value));
		if (streq(type, "int")) map->setInt(key, atoi(value));
		if (streq(type, "char*")) map->setString(key, stringClone(value));
		if (streq(type, "float")) map->setFloat(key, atof(value));
		lineStart = lineEnd + strlen(stringMapRecordSeperator);
		if (lineEnd - serial + strlen(stringMapRecordSeperator) - serialLen <= 0) break;
	}

	Free(serial);
}

void StringMap::remove(char *key, bool freeValue) {
	StringMap *map = this;

	int index = -1;
	for (int i = 0; i < map->length; i++) {
		if (strcmp(map->keys[i], key) == 0) {
			index = i;
			break;
		}
	}

	if (index == -1) {
		printf("Key %s not found\n", key);
		return;
	}

	if (freeValue) Free(map->values[index]);

	map->keys[index] = map->keys[map->length-1];
	map->values[index] = map->values[map->length-1];
	map->length--;
}

void StringMap::destroy() {
	StringMap *map = this;

	for (int i = 0; i < map->length; i++) {
		Free(map->keys[i]);
		if (map->types[i] == MAP_STRING) Free(map->values[i]);
	}

	Free(map->keys);
	Free(map->values);
	Free(map->types);
	Free(map);
}

int StringMap::setLong(const char *key, long value) {
	StringMap *map = this;

	/// Update
	for (int i = 0; i < map->length; i++) {
		if (strcmp(map->keys[i], key) == 0) {
			map->values[i] = (void *)value;
			map->types[i] = MAP_LONG;
			return i;
		}
	}

	/// Grow
	if (map->length >= map->maxLen) {
		map->maxLen *= 2;
		map->keys = (char **)realloc(map->keys, map->maxLen * sizeof(char *));
		map->values = (void **)realloc(map->values, map->maxLen * sizeof(void *));
		map->types = (MapValueType *)realloc(map->types, map->maxLen * sizeof(MapValueType));
	}

	/// Insert
	char *nonConstKey = stringClone(key);
	map->keys[map->length] = nonConstKey;
	map->values[map->length] = (void *)value;
	map->types[map->length] = MAP_LONG;
	map->length++;

	return map->length-1;
}

int StringMap::setInt(const char *key, int value) {
	StringMap *map = this;

	int pos = map->setLong(key, (long long)value);
	map->types[pos] = MAP_INT;

	return pos;
}

int StringMap::setFloat(const char *key, float value) {
	StringMap *map = this;

	long asLong = *(long *)&value;
	// long asLong = ((long *)&value)[0]; //@cleanup Maybe do this
	int pos = map->setLong(key, asLong);
	map->types[pos] = MAP_FLOAT;
	return pos;
}

int StringMap::setString(const char *key, const char *value) {
	StringMap *map = this;

	value = stringClone(value);

	int oldValueIndex = map->indexOf(key);
	if (oldValueIndex != -1 && map->types[oldValueIndex] == MAP_STRING) Free(map->getString(key));

	int pos = map->setLong(key, (long long)value);
	map->types[pos] = MAP_STRING;
	return pos;
}

long StringMap::getLong(const char *key) {
	StringMap *map = this;

	for (int i = 0; i < map->length; i++)
		if (streq(map->keys[i], key))
			return (long long)map->values[i];

	Assert(0);
	return 0;
}

int StringMap::getInt(const char *key) {
	StringMap *map = this;
	long asLong = map->getLong(key);
	return (int)asLong;
}

float StringMap::getFloat(const char *key) {
	StringMap *map = this;
	long asLong = map->getLong(key);
	return *(float *)&asLong;
}

char *StringMap::getString(const char *key) {
	StringMap *map = this;
	long asLong = map->getLong(key);
	return (char *)asLong;
}
