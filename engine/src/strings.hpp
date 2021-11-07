//@cleanup This file likely doesn't need to exist
#pragma once

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

String *newString(char *value=nullptr, int len=-1);
String *newString(int startingLen);
