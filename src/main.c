#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <string.h>
#include "http_tools.h"
#include "parse.h"
#include "rb.h"
#include <pthread.h>
#include "threads.h"

#include "debug.h"


#define INPUT_LEN 1024

void score_perf_by_request_rate(int request_rate) {
    __atomic_store_n(&REQUEST_RATE, (int) request_rate, __ATOMIC_RELEASE);
    logDebug("Request rate set to %d", __atomic_load_n(&REQUEST_RATE, __ATOMIC_ACQUIRE));
    FILE *fp = fopen("../alice29.txt", "r");
    if (!fp) {
        fprintf(stderr, "error: failed to open alice29.txt\n");
        exit(1);
    }

    // Create pipe for text and query preparation rbs
    struct rbPipe *text_pipe = malloc(sizeof(struct rbPipe));
    struct mpmcRingBuffer *text_rb = rb_create();
    struct mpmcRingBuffer *curl_rb = rb_create();

    text_pipe->buf_a = text_rb;
    text_pipe->buf_b = curl_rb;
    text_pipe->producer_finished = 0;

    // Create pipe for previous query preparation rb and resp parsing rb
    struct rbPipe *resp_pipe = malloc(sizeof(struct rbPipe));
    resp_pipe->buf_a = curl_rb;

    struct mpmcRingBuffer *resp_rb = rb_create();
    resp_pipe->buf_b = resp_rb;
    resp_pipe->producer_finished = 0;


    // Deploy monitor thread
    pthread_t monitor_thread;
    pthread_create(&monitor_thread, NULL, monitor_request_metrics_fn, NULL);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Deploy worker fn for text_pipe
    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_create(&curl_thread_pool[i], NULL, read_text_and_send_req_worker_fn, text_pipe);
    }

    // Producer logic for text_rb
    char line[INPUT_LEN];

    int n_requests = 300;
    int writes = 0;
    while (fgets(line, INPUT_LEN, fp)) {
        if (writes >= n_requests) {
            logDebug("Leaving early.");
            break;
        }
        line[strcspn(line, "\r\n")] = '\0'; // trim newline
        if (strlen(line) == 0) {
            continue;
        }
        char *copy = strdup(line);
        if (!copy) {
            perror("strdup");
            exit(1);
        }
        rb_put(text_rb, copy);
        writes++;
    }
    __atomic_store_n(&text_pipe->producer_finished, 1, __ATOMIC_RELEASE);

    // Deploy worker fn for resp_pipe
    pthread_create(&parse_thread, NULL, get_and_rank_responses_worker_fn, resp_pipe);

    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_join(curl_thread_pool[i], NULL);
    }
    __atomic_store_n(&resp_pipe->producer_finished, 1, __ATOMIC_RELEASE);
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = elapsed_sec(start, end);
    pthread_join(parse_thread, NULL);
    fprintf(stderr, "req rate: %i, request throughput: %f\n",
            __atomic_load_n(&REQUEST_RATE, __ATOMIC_ACQUIRE),
            (double) n_requests / elapsed);
    free(text_pipe);
    free(resp_pipe);
}

int main(void) {
    curl_global_init(CURL_GLOBAL_ALL);

    score_perf_by_request_rate(5);
    score_perf_by_request_rate(25);
    score_perf_by_request_rate(50);
    score_perf_by_request_rate(150);
    logDebug("Finished.");
    return 0;
}

