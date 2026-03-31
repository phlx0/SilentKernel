#pragma once
#include <stddef.h>
#include <stdint.h>

size_t strlen(const char *str);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcpy(char *dst, const char *src);
char  *strncpy(char *dst, const char *src, size_t n);
void  *memset(void *ptr, int val, size_t n);
void  *memcpy(void *dst, const void *src, size_t n);
void   itoa(int value, char *buf, int base);
void   uitoa(uint32_t value, char *buf, int base);
