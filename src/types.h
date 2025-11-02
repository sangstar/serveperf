//
// Created by Sanger Steel on 10/22/25.
//

#ifndef SERVEPERF_TYPES_H
#define SERVEPERF_TYPES_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"

#define FMT(x) _Generic((x), \
    char: "%c", \
    signed char: "%hhd", \
    unsigned char: "%hhu", \
    short: "%hd", \
    unsigned short: "%hu", \
    int: "%d", \
    unsigned int: "%u", \
    long: "%ld", \
    unsigned long: "%lu", \
    long long: "%lld", \
    unsigned long long: "%llu", \
    float: "%f", \
    double: "%f", \
    long double: "%Lf", \
    char *: "%s", \
    const char *: "%s", \
    default: "%p" \
)

#define DEFINE_BUFFER_TYPE(name, datatype, format_string) \
    static struct  buffer_##name { \
        datatype *data; \
        size_t len; \
        size_t capacity; \
        size_t elem_size; \
        char *fmt; \
        char *type_name; \
    }; \
    static struct buffer_##name *buffer_##name##_new(size_t capacity) { \
        struct buffer_##name *buf = malloc(sizeof(struct buffer_##name)); \
        buf->capacity = capacity; \
        buf->data = (datatype *)calloc(1, sizeof(datatype) * buf->capacity); \
        buf->len = 0; \
        buf->fmt = format_string; \
        buf->elem_size = sizeof(datatype); \
        buf->type_name = "buffer_" #name; \
        return buf; \
    }; \
    static inline void buffer_##name##_append(struct buffer_##name *buf, datatype elem) { \
        if (buf->len + 1 >= buf->capacity) { \
            buf->data = realloc(buf->data, buf->capacity * 2); \
            if (!buf->data) { \
                perror("realloc"); \
                exit(1); \
            } \
            buf->capacity *= 2; \
        }; \
        datatype *buf_data = (datatype *) buf->data; \
        buf->data[buf->len] = elem; \
        buf->len++; \
    } \
    static inline void buffer_##name##_memcpy(struct buffer_##name *dest, void *src, size_t n) { \
        if (dest->len + sizeof(*src) * n >= dest->capacity) { \
            dest->data = realloc(dest->data, dest->capacity + sizeof(*src) * n * 2); \
            if (!dest->data) { \
                perror("realloc"); \
                exit(1); \
            } \
            dest->capacity *= sizeof(*src) * n * 2; \
        }; \
        memcpy(dest->data + dest->len, src, sizeof(*src) * n); \
        dest->len += sizeof(*src) * n; \
    } \
    static inline void buffer_##name##_refresh(struct buffer_##name *buf) { \
        char *buf_data = (char *) buf->data; \
        buf_data[buf->len] = 0; \
        buf->len = 0; \
    } \
    static inline void buffer_##name##_print(struct buffer_##name *buf) { \
        char str[256] = ""; \
        int wrote = 0; \
        wrote += snprintf(str + wrote, sizeof(str) - wrote, "%s", "["); \
        for (int i = 0; i < buf->len; ++i) { \
            wrote += snprintf(str + wrote, sizeof(str) - wrote, buf->fmt, buf->data[i]); \
            if (i == buf->len - 1) { \
                wrote += snprintf(str + wrote, sizeof(str) - wrote, "%s", "]"); \
                break; \
            } \
            wrote += snprintf(str + wrote, sizeof(str) - wrote, "%s", ", "); \
        } \
        logDebug("%s(len=%lu, capacity=%lu, elem_size=%lu, data=%s)", buf->type_name, buf->len, buf->capacity, buf->elem_size, str); \
    }

DEFINE_BUFFER_TYPE(int, int, "%d")
DEFINE_BUFFER_TYPE(char, char, "%s")

#endif //SERVEPERF_TYPES_H
