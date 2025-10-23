//
// Created by Sanger Steel on 10/22/25.
//

#include "../rb.h"
#include "debug.h"
#include <stdio.h>

struct mpmcRingBuffer *rb_create() {
    struct mpmcRingBuffer *rb = malloc(sizeof(struct mpmcRingBuffer));
    if (!rb) {
        fprintf(stderr, "Failed to allocate memory for ring buffer\n");
        exit(1);
    }
    rb->head = 0;
    rb->tail = 0;
    rb->capacity = NUM_SLOTS;
    rb->slots = calloc(rb->capacity, sizeof(void *)); // ✅ allocate slots array
    if (!rb->slots) {
        free(rb);
        return NULL;
    }
    return rb;
}

void rb_destroy(struct mpmcRingBuffer *rb) {
    free(rb);
}

void *rb_get(struct mpmcRingBuffer *rb) {
    size_t tail = 0;
    size_t head = 0;
    do {
        tail = __atomic_load_n(&rb->tail, __ATOMIC_ACQUIRE);
        head = __atomic_load_n(&rb->head, __ATOMIC_ACQUIRE);
        if (head == tail) {
            return NULL;
        }
    } while (!__atomic_compare_exchange_n(&rb->tail, &tail, tail + 1, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED));
    void *data = rb->slots[tail % rb->capacity];
    // logDebug("slot %lu get %p with head %lu tail %lu", tail % rb->capacity, data, head, tail);
    return data;
}

int rb_put(struct mpmcRingBuffer *rb, void *data) {
    size_t tail = 0;
    size_t head = 0;

    do {
        tail = __atomic_load_n(&rb->tail, __ATOMIC_ACQUIRE);
        head = __atomic_load_n(&rb->head, __ATOMIC_ACQUIRE);
        if (((head + 1) & (rb->capacity - 1)) == (tail & (rb->capacity - 1))) {
            return 0;
        }
    } while (!__atomic_compare_exchange_n(&rb->head, &head, head + 1, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED));

    rb->slots[head % rb->capacity] = data;
    // logDebug("slot %lu push %p with head %lu tail %lu", head % rb->capacity, data, head, tail);
    return 1;
}
