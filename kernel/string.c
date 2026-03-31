#include "string.h"

size_t strlen(const char *str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && (*a == *b)) { a++; b++; }
    if (n == (size_t)-1) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

char *strcpy(char *dst, const char *src) {
    char *ret = dst;
    while ((*dst++ = *src++));
    return ret;
}

char *strncpy(char *dst, const char *src, size_t n) {
    char *ret = dst;
    while (n-- && (*dst++ = *src++));
    while (n-- > 0) *dst++ = '\0';
    return ret;
}

void *memset(void *ptr, int val, size_t n) {
    unsigned char *p = ptr;
    while (n--) *p++ = (unsigned char)val;
    return ptr;
}

void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

void itoa(int value, char *buf, int base) {
    if (base < 2 || base > 16) { buf[0] = '\0'; return; }
    char tmp[32];
    int i = 0, neg = 0;
    if (value < 0 && base == 10) { neg = 1; value = -value; }
    if (value == 0) { tmp[i++] = '0'; }
    while (value) {
        int r = value % base;
        tmp[i++] = r < 10 ? '0' + r : 'a' + r - 10;
        value /= base;
    }
    if (neg) tmp[i++] = '-';
    int j = 0;
    while (i--) buf[j++] = tmp[i];
    buf[j] = '\0';
}

void uitoa(uint32_t value, char *buf, int base) {
    if (base < 2 || base > 16) { buf[0] = '\0'; return; }
    char tmp[32];
    int i = 0;
    if (value == 0) { tmp[i++] = '0'; }
    while (value) {
        uint32_t r = value % base;
        tmp[i++] = r < 10 ? '0' + r : 'a' + r - 10;
        value /= base;
    }
    int j = 0;
    while (i--) buf[j++] = tmp[i];
    buf[j] = '\0';
}
