//
// Created by Sanger Steel on 10/22/25.
//

#include "threads.h"

#include <stdlib.h>
#include <string.h>

#include "rb.h"
#include "parse.h"
#include "debug.h"
#include "types.h"
#include "shared_ptr.h"

// TODO: Eventually let this change so can get different perfs for different req rates
//       to get the ideal req rate?
int REQUEST_RATE = 10;

pthread_t curl_thread_pool[MAX_THREADS];
pthread_t parse_thread;


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
    SharedPtr_factoryExecutionContext *sp = arg;
    factoryExecutionContext *ctx = SharedPtr_factoryExecutionContext_get(sp);
    char *line = NULL;
    double elapsed = 0;
    struct timespec ts;
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    while (1) {
        struct curlHandler resp = {0};

        resp.curlResponse.data = buffer_char_new(MAX_RESPONSE_BODY);


        struct mpmcRingBuffer *buf_a = ctx->pipe->buf_a;
        struct mpmcRingBuffer *buf_b = ctx->pipe->buf_b;


        line = (char *) rb_get(buf_a);
        if (line) {
            struct oaiResponsePerf *perf = calloc(1, sizeof(struct oaiResponsePerf));
            clock_gettime(CLOCK_MONOTONIC, &start);
            query_openai_endpoint(&resp, ctx->request_metadata.endpoint, line,
                                  ctx->request_metadata.model_id, ctx->request_metadata.max_tokens);
            oaiResponsePerf_set_from_curlResponse(perf, &resp);
            clock_gettime(CLOCK_MONOTONIC, &end);

            if (perf->tokens_count > 0) rb_put((void *) buf_b, perf);
            elapsed = elapsed_sec(start, end);
            logDebug("Worker served 1 request in %f seconds", elapsed);
            __atomic_fetch_add(&global_metrics.num_requests, 1, __ATOMIC_RELAXED);

            int request_rate = __atomic_load_n(&REQUEST_RATE, __ATOMIC_ACQUIRE);
            double rate = 1 / elapsed;
            double goal_elapsed = 1.0 / (double) request_rate * MAX_THREADS;
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

                logDebug("Worker's latency was %f, %f less than %f. Sleeping for %f s to maintain request rate of %i",
                         elapsed, difference,
                         goal_elapsed, difference, request_rate);
                nanosleep(&ts, NULL);
            }
        } else if (__atomic_load_n(&ctx->pipe->producer_finished, __ATOMIC_ACQUIRE) == 1) {
            break;
        } else {
            sched_yield();
        };
    }
    SharedPtr_factoryExecutionContext_free(sp);
    return NULL;
}

void *get_and_rank_responses_worker_fn(void *arg) {
    logDebug("Worker started: arg=%p", arg);
    SharedPtr_factoryExecutionContext *sp = arg;
    factoryExecutionContext *ctx = SharedPtr_factoryExecutionContext_get(sp);

    struct oaiResponsePerf *best = malloc(sizeof(struct oaiResponsePerf));

    double reqs = 0;
    double total_throughput = 0;
    double total_ttft = 0;

    double best_score = 0;
    while (1) {
        struct oaiResponsePerf *perf = (struct oaiResponsePerf *) rb_get(ctx->pipe->buf_a);
        if (!perf) {
            if (__atomic_load_n(&ctx->pipe->producer_finished, __ATOMIC_ACQUIRE) == 1) {
                break;
            }
            sched_yield();
            continue;
        }
        if (perf->tokens_count == 0) {
            fprintf(stderr, "got perf with 0 tokens\n");
            exit(1);
        }
        reqs++;
        total_throughput += perf->throughput;
        total_ttft += perf->ttft;
        double perf_score = oaiResponsePerf_score(perf);
        if (perf_score > best_score) {
            best_score = perf_score;
            memcpy(best, perf, sizeof(struct oaiResponsePerf));
        }
        free(perf);
    }
    if (best_score > 0.0)
        PRINT_OAI_RESPONSE_PERF(best);
    fprintf(stderr, "reqs: %f, req rate: %i, average throughput: %f, average TTFT: %f\n",
            reqs, __atomic_load_n(&REQUEST_RATE, __ATOMIC_RELAXED),
            total_throughput / reqs,
            total_ttft / reqs);
    free(best);
    SharedPtr_factoryExecutionContext_free(sp);
    return NULL;
}
