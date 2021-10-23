#include "stringTools.h"

char *stringClone(const char *str) {
	char *ret = (char *)Malloc((strlen(str)+1) * sizeof(char));
	strcpy(ret, str);
	return ret;
}

char *stringGetDelim(const char *str) {
	if (strstr(str, "\r\n")) return stringClone("\r\n");
	return stringClone("\n");
}

bool stringStartsWith(const char *hayStack, const char *needle) {
	if (strlen(needle) > strlen(hayStack)) return false;

	int len = strlen(needle);
	bool areSame = true;
	for (int i = 0; i < len; i++)
		if (needle[i] != hayStack[i])
			areSame = false;

	return areSame;
}

bool stringEndsWith(const char *hayStack, const char *needle) {
	if (!hayStack || !needle) return false;

	int hayStackLen = strlen(hayStack);
	int needleLen = strlen(needle);
	if (needleLen > hayStackLen) return false;

	return strncmp(hayStack + hayStackLen - needleLen, needle, needleLen) == 0;
}

bool streq(const char *str1, const char *str2) {
	if (!str1 || !str2) return false;
	return strcmp(str1, str2) == 0;
}

char *fastStrcat(char *endOfStr1, const char *str2) {
	while (*endOfStr1) str2++;
	while ((*endOfStr1++ = *str2++));
	return --endOfStr1;
}

void prependStr(char *src, const char *dest) {
	size_t len = strlen(dest);
	size_t i;

	memmove(src + len, src, strlen(src) + 1);

	for (i = 0; i < len; ++i) {
		src[i] = dest[i];
	}
}

bool strSimilar(char const *a, char const *b) {
	for (;; a++, b++) {
		int d = tolower(*a) - tolower(*b);
		if (d != 0 || !*a) {
			int ret = d;
			return !ret;
		}
	}
}

void stringPrepend(char *s, const char *t) {
	size_t len = strlen(t);
	size_t i;

	memmove(s + len, s, strlen(s) + 1);

	for (i = 0; i < len; ++i) s[i] = t[i];
}
