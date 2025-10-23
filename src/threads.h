//
// Created by Sanger Steel on 10/22/25.
//

#ifndef SERVEPERF_THREADS_H
#define SERVEPERF_THREADS_H

#include <pthread.h>
#include <stdint.h>

#define MAX_THREADS 64

extern int REQUEST_RATE;

extern pthread_t curl_thread_pool[MAX_THREADS];
pthread_t parse_thread;

void *monitor_request_metrics_fn(void *arg);

struct rbPipe {
    struct mpmcRingBuffer *buf_a;
    struct mpmcRingBuffer *buf_b;
    int producer_finished;
};

double elapsed_sec(struct timespec start, struct timespec end);

void *read_text_and_send_req_worker_fn(void *arg);


void *get_and_rank_responses_worker_fn(void *arg);

#endif //SERVEPERF_THREADS_H
