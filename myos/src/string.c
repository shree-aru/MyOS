/* ============================================================================
 * MyOS - String Utilities
 * Basic string and memory functions (no libc dependency)
 * ============================================================================ */

#include "kernel.h"

void* memset(void* dst, int val, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    uint32_t v = (uint8_t)val;
    v |= (v << 8);
    v |= (v << 16);
    
    size_t n4 = n / 4;
    uint32_t* d4 = (uint32_t*)dst;
    for (size_t i = 0; i < n4; i++) {
        d4[i] = v;
    }
    for (size_t i = n4 * 4; i < n; i++) {
        d[i] = (uint8_t)val;
    }
    return dst;
}

void* memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    
    size_t n4 = n / 4;
    uint32_t* d4 = (uint32_t*)dst;
    const uint32_t* s4 = (const uint32_t*)src;
    for (size_t i = 0; i < n4; i++) {
        d4[i] = s4[i];
    }
    for (size_t i = n4 * 4; i < n; i++) {
        d[i] = s[i];
    }
    return dst;
}

void* memmove(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    if (d < s) {
        for (size_t i = 0; i < n; i++)
            d[i] = s[i];
    } else {
        for (size_t i = n; i > 0; i--)
            d[i-1] = s[i-1];
    }
    return dst;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const uint8_t* a = (const uint8_t*)s1;
    const uint8_t* b = (const uint8_t*)s2;
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i])
            return (int)a[i] - (int)b[i];
    }
    return 0;
}

size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return (int)(uint8_t)*s1 - (int)(uint8_t)*s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i])
            return (int)(uint8_t)s1[i] - (int)(uint8_t)s2[i];
        if (s1[i] == '\0')
            return 0;
    }
    return 0;
}

char* strcpy(char* dst, const char* src) {
    char* d = dst;
    while ((*d++ = *src++));
    return dst;
}

char* strncpy(char* dst, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++)
        dst[i] = src[i];
    for (; i < n; i++)
        dst[i] = '\0';
    return dst;
}

char* strcat(char* dst, const char* src) {
    char* d = dst + strlen(dst);
    while ((*d++ = *src++));
    return dst;
}

char* itoa(int val, char* buf, int base) {
    if (base < 2 || base > 36) {
        buf[0] = '\0';
        return buf;
    }

    char* p = buf;
    char* p1, *p2;
    int neg = 0;
    uint32_t uval;
    if (val < 0 && base == 10) {
        neg = 1;
        uval = (uint32_t)(-(int64_t)val);
    } else {
        uval = (uint32_t)val;
    }
    do {
        int rem = uval % base;
        *p++ = (rem < 10) ? ('0' + rem) : ('a' + rem - 10);
        uval /= base;
    } while (uval);

    if (neg) *p++ = '-';
    *p = '\0';

    /* Reverse the string */
    p1 = buf;
    p2 = p - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }

    return buf;
}

char* utoa(uint32_t val, char* buf, int base) {
    if (base < 2 || base > 36) {
        buf[0] = '\0';
        return buf;
    }

    char* p = buf;
    char* p1, *p2;

    do {
        int rem = val % base;
        *p++ = (rem < 10) ? ('0' + rem) : ('a' + rem - 10);
        val /= base;
    } while (val);

    *p = '\0';

    p1 = buf;
    p2 = p - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }

    return buf;
}
