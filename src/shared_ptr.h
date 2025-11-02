//
// Created by Sanger Steel on 11/2/25.
//

#ifndef SERVEPERF_SHARED_PTR_H
#define SERVEPERF_SHARED_PTR_H

#include <stdlib.h>

#define MAX_ENTRANTS 2048


struct SharedPtrRegistryEntry {
    u_int64_t magic;
    void *sp;
    const char *type;
};


struct SharedPtrRegistryEntry SharedPtrRegistry[MAX_ENTRANTS];
extern uint64_t SharedPtrRegistryIdx;

#define DECLARE_SHARED_PTR(Type) \
    typedef struct { \
        Type *ptr; \
        uint64_t ref_count; \
    } SharedPtr_##Type; \
    \
    static inline SharedPtr_##Type *SharedPtr_##Type##_new(Type *ptr) { \
        SharedPtr_##Type *sp = malloc(sizeof(SharedPtr_##Type)); \
        sp->ref_count = 0; \
        uint64_t idx = __atomic_add_fetch(&SharedPtrRegistryIdx, 1, __ATOMIC_RELAXED); \
        sp->ptr = ptr; \
        SharedPtrRegistry[idx].sp = sp; \
        SharedPtrRegistry[idx].magic = 0xDEADBEEF; \
        SharedPtrRegistry[idx].type = #Type; \
    return sp; } \
    \
    static inline void SharedPtr_##Type##_free(SharedPtr_##Type *sp) { \
        if (__atomic_load_n(&sp->ref_count, __ATOMIC_ACQUIRE) <= 0) return; \
        __atomic_store_n(&sp->ref_count, (__atomic_load_n(&sp->ref_count, __ATOMIC_ACQUIRE) - 1), __ATOMIC_RELEASE); \
    }; \
    static inline Type *SharedPtr_##Type##_get(SharedPtr_##Type *sp) { \
        __atomic_add_fetch(&sp->ref_count, 1, __ATOMIC_RELAXED); \
        return (Type *)sp->ptr; \
    };


#endif //SERVEPERF_SHARED_PTR_H
