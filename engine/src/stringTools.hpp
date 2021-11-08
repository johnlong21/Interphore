#pragma once

char *stringClone(const char *str);
char *stringGetDelim(const char *str);

bool stringStartsWith(const char *hayStack, const char *needle);
bool stringEndsWith(const char *hayStack, const char *needle);

bool streq(const char *str1, const char *str2);
bool strSimilar(char const *a, char const *b);

char *fastStrcat(char *endOfStr1, const char *str2);
void prependStr(char *src, const char *dest);
