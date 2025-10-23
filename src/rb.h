//
// Created by Sanger Steel on 10/22/25.
//

#ifndef SERVEPERF_RB_H
#define SERVEPERF_RB_H

#define MAX_SLOT_MEMORY 1024
#define NUM_SLOTS 1024
#include <stdbool.h>
#include <stdlib.h>

struct mpmcRingBuffer {
    size_t head;
    size_t tail;
    size_t capacity;
    void **slots;
};

struct mpmcRingBuffer *rb_create();

void rb_destroy(struct mpmcRingBuffer *rb);

void *rb_get(struct mpmcRingBuffer *rb);

int rb_put(struct mpmcRingBuffer *rb, void *data);

#endif //SERVEPERF_RB_H
