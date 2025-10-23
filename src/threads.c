//
// Created by Sanger Steel on 10/22/25.
//

#include "threads.h"

#include <stdlib.h>
#include "rb.h"
#include "http_tools.h"
#include "debug.h"

// TODO: Eventually let this change so can get different perfs for different req rates
//       to get the ideal req rate?
const double REQUEST_RATE = 5.0;

pthread_t curl_thread_pool[MAX_THREADS];

struct RequestMetrics {
    uint32_t elapsed_time;
    uint32_t num_requests;
};

static struct RequestMetrics global_metrics = {0, 0};

double elapsed_sec(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) +
           (end.tv_nsec - start.tv_nsec) / 1e9;
}

// TODO: Endless loop here even though this is meant to be a daemon
void *monitor_request_metrics_fn(void *arg) {
    long long wait_seconds = 60;
    struct timespec ts = {.tv_sec = wait_seconds, .tv_nsec = 0};
    while (1) {
        uint32_t requests = __atomic_load_n(&global_metrics.num_requests, __ATOMIC_ACQUIRE);

        if (requests != 0) {
            double request_rate = (double) requests / (double) wait_seconds;
            logDebug("Request monitor got %f reqs/s", request_rate);
            __atomic_store_n(&global_metrics.num_requests, 0, __ATOMIC_RELAXED);
        }

        nanosleep(&ts, NULL);
    }
}

void *read_text_and_send_req_worker_fn(void *arg) {
    logDebug("Worker started: arg=%p", arg);
    struct rbPipe *pipe = (struct rbPipe *) arg;
    char *line = NULL;
    double elapsed = 0;
    struct timespec ts;
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    while (1) {
        struct curlResponse resp = {0};

        struct mpmcRingBuffer *buf_a = pipe->buf_a;
        struct mpmcRingBuffer *buf_b = pipe->buf_b;

        struct oaiResponsePerf *perf = malloc(sizeof(struct oaiResponsePerf));

        line = (char *) rb_get(buf_a);
        if (line) {
            clock_gettime(CLOCK_MONOTONIC, &start);
            query_openai_endpoint(&resp, "https://api.openai.com/v1/completions", line,
                                  "gpt-3.5-turbo-instruct");
            oaiResponsePerf_set_from_curlResponse(perf, &resp);
            clock_gettime(CLOCK_MONOTONIC, &end);
            PRINT_OAI_RESPONSE_PERF(perf);

            rb_put((void *) buf_b, perf);
            elapsed = elapsed_sec(start, end);
            logDebug("Worker served 1 request in %f seconds", elapsed);
            __atomic_fetch_add(&global_metrics.num_requests, 1, __ATOMIC_RELAXED);

            double rate = 1 / elapsed;
            double goal_elapsed = 1 / REQUEST_RATE * MAX_THREADS;
            if (elapsed < goal_elapsed) {
                double difference = goal_elapsed - elapsed;
                if (difference >= 1) {
                    // This means it's greater than a second, so can't
                    // be directly put in to tv_nsec
                    long long difference_ns = (long long) (difference * 1e9);
                    long long seconds = (int) difference / 1;
                    double ns = difference_ns % (int) 1e9;
                    ts.tv_sec = seconds;
                    ts.tv_nsec = ns;
                } else {
                    ts.tv_nsec = difference * 1e9;
                }

                logDebug("Worker's latency was %f, %f less than %f. Sleeping for %f s to maintain request rate of %f",
                         elapsed, difference,
                         goal_elapsed, ts.tv_nsec / 1e9, REQUEST_RATE);
                nanosleep(&ts, NULL);
            }
        } else if (__atomic_load_n(&pipe->finished, __ATOMIC_ACQUIRE) == 1) {
            break;
        } else {
            sched_yield();
        };
    }
    logDebug("Worker finished with pipe %i", __atomic_load_n(&pipe->finished, __ATOMIC_ACQUIRE));
    return NULL;
}
