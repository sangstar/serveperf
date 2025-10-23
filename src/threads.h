//
// Created by Sanger Steel on 10/22/25.
//

#ifndef SERVEPERF_THREADS_H
#define SERVEPERF_THREADS_H

#include <pthread.h>
#include <stdint.h>

#define MAX_THREADS 64

extern const double REQUEST_RATE;

extern pthread_t curl_thread_pool[MAX_THREADS];

void *monitor_request_metrics_fn(void *arg);

struct rbPipe {
    struct mpmcRingBuffer *buf_a;
    struct mpmcRingBuffer *buf_b;
    int finished;
};

void *read_text_and_send_req_worker_fn(void *arg);

#endif //SERVEPERF_THREADS_H
