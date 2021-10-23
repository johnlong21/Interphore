#pragma once

enum MapValueType { MAP_LONG, MAP_INT, MAP_FLOAT, MAP_STRING};

struct StringMap;

StringMap *stringMapCreate(int startLen=16);

struct StringMap {
	char **keys;
	void **values;
	MapValueType *types;
	int length;
	int maxLen;

	bool exists(const char *key);
	bool isNull(const char *key);
	int indexOf(const char *key);

	int setLong(const char *key, long value);
	int setInt(const char *key, int value);
	int setFloat(const char *key, float value);
	int setString(const char *key, const char *value);

	long getLong(const char *key);
	int getInt(const char *key);
	float getFloat(const char *key);
	char *getString(const char *key);

	char *serialize();
	void deserialize(char *serial);

	void remove(char *key, bool freeValue);
	void destroy();
};
