#include "strings.h"

struct String {
	char *cStr;
	int length;
	int maxLength;

	void set(const char *value, int chars=-1);
	void append(const char *value, int chars=-1);
	void appendChar(const char value);
	void pop(int amount=1);
	void shift(int amount=1);
	void resize(int newLength);
	String *clone();

	int indexOf(const char *needle, int startPoint=0);
	String *subStrAbs(int start, int end);
	String *subStrRel(int start, int length);
	char charAt(int index);
	char getLastChar();
	String *replace(const char *src, const char *dest);
	void split(const char *delim, String ***returnArray, int *returnArrayNum);
	int count(const char *src);

	bool equals(String *other);

	void clear();
	void destroy();
};

String *newString(char *value=NULL, int len=-1);
String *newString(int startingLen);

String *newString(char *value, int len) {
	String *str = (String *)Malloc(sizeof(String));
	str->clear();
	if (value) str->set(value, len);
	return str;
}

String *newString(int startingLen) {
	String *str = (String *)Malloc(sizeof(String));
	str->clear();
	str->resize(startingLen);
	return str;
}

void String::set(const char *value, int chars) {
	String *str = this;

	if (chars == -1) chars = strlen(value);

	str->length = chars;
	str->resize(str->length);
	strncpy(str->cStr, value, chars);
	str->cStr[chars] = '\0';
}

void String::append(const char *value, int chars) {
	String *str = this;

	if (chars == -1) chars = strlen(value);
	for (int i = 0; i < chars; i++) str->appendChar(value[i]);
}

void String::appendChar(const char value) {
	String *str = this;

	str->resize(str->length+1);
	str->cStr[str->length] = value;
	str->cStr[str->length+1] = '\0';
	str->length++;
}

void String::pop(int amount) {
	String *str = this;

	for (int i = 0; i < amount; i++) {
		str->cStr[str->length-1] = '\0';
		str->length--;
	}
}

void String::shift(int amount) {
	String *str = this;
	// printf("Pre shift by %d |%s|(%d)\n", amount, str->cStr, str->length);
	memmove(str->cStr, &str->cStr[amount], str->length - amount);
	str->length -= amount;
	str->cStr[length] = '\0';
	// printf("Post shift by %d |%s|(%d)\n", amount, str->cStr, str->length);
}

void String::resize(int newLength) {
	String *str = this;
	if (str->maxLength >= newLength+1) return;

	int wantedLen = 2;
	while (wantedLen < newLength+1) {
		wantedLen *= 2;
	}

	newLength = wantedLen;

	str->maxLength = newLength;
	char *newCStr = (char *)Malloc(str->maxLength);
	memset(newCStr, 0, str->maxLength);

	if (str->cStr) {
		strcpy(newCStr, cStr);
		Free(str->cStr);
	}

	str->cStr = newCStr;
}

String *String::clone() {
	String *str = this;
	String *newStr = newString();

	newStr->set(str->cStr);

	return newStr;

}

int String::indexOf(const char *needle, int startPoint) {
	String *str = this;

	char *res = strstr(&str->cStr[startPoint], needle);
	if (!res) return -1;
	return res - str->cStr;
}

String *String::subStrAbs(int start, int end) {
	return this->subStrRel(start, end-start);
}

String *String::subStrRel(int start, int length) {
	String *str = this;
	String *newStr = newString();

	newStr->set(&str->cStr[start], length);

	return newStr;
}

char String::getLastChar() {
	return this->cStr[this->length-1];
}

char String::charAt(int index) {
	if (index > this->length-1) return '\0';
	return this->cStr[index];
}

String *String::replace(const char *src, const char *dest) {
	String *str = this;
	String *newStr = newString();

	int curPos = 0;
	for (;;) {
		int nextPos = str->indexOf(src, curPos);
		if (nextPos == -1) {
			nextPos = str->length - 1;
			newStr->append(&str->cStr[curPos], nextPos - curPos + 1);
			break;
		}

		newStr->append(&str->cStr[curPos], nextPos - curPos);
		newStr->append(dest);

		curPos = nextPos + strlen(src);
	}

	return newStr;
}

void String::split(const char *delim, String ***returnArray, int *returnArrayNum) { //@incomplete Look into this not creating a final string
	String *str = this;

	String **strArr = (String **)Malloc(sizeof(String *) * (str->count(delim) + 1));
	int strArrNum = 0;

	int curPos = 0;
	for (;;) {
		int nextPos = str->indexOf(delim, curPos);

		if (nextPos == -1) nextPos = str->length;

		// printf("Splitting %d %d\n", curPos, nextPos);
		String *newStr = str->subStrAbs(curPos, nextPos);
		// printf("Got |%s|\n", newStr->cStr);
		strArr[strArrNum++] = newStr;

		curPos = nextPos + strlen(delim);
		if (nextPos >= str->length-1) break;
	}

	*returnArray = strArr;
	*returnArrayNum = strArrNum;
}

int String::count(const char *src) {
	String *str = this;
	int total = 0;

	int curPos = 0;
	for (;;) {
		int nextPos = str->indexOf(src, curPos);
		if (nextPos == -1) break;
		total++;
		curPos = nextPos + 1;
	}

	return total;
}

bool String::equals(String *other) {
	return streq(this->cStr, other->cStr);
}

void String::clear() {
	memset(this, 0, sizeof(String));
}

void String::destroy() {
	if (this->cStr) Free(this->cStr);
	Free(this);
}
