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
#include "types.h"

#include "debug.h"

uint64_t SharedPtrRegistryIdx = 0;

void score_perf_by_request_rate(struct buffer_char *src, int request_rate, int input_len, int max_tokens,
                                int num_requests, const char *endpoint,
                                const char *model_id) {
    __atomic_store_n(&REQUEST_RATE, (int) request_rate, __ATOMIC_RELEASE);
    logDebug("Request rate set to %d", __atomic_load_n(&REQUEST_RATE, __ATOMIC_ACQUIRE));
    logDebug("Performing a test with request_rate=%i, input_len=%i, max_tokens=%i, num_requests=%i, endpoint=%s",
             request_rate, input_len, max_tokens, num_requests, endpoint);

    // Create pipe for text and query preparation rbs
    struct rbPipe *text_pipe = malloc(sizeof(struct rbPipe));
    struct mpmcRingBuffer *text_rb = rb_create();
    struct mpmcRingBuffer *curl_rb = rb_create();

    text_pipe->buf_a = text_rb;
    text_pipe->buf_b = curl_rb;
    text_pipe->producer_finished = 0;

    factoryExecutionContext *text_ctx = malloc(sizeof(factoryExecutionContext));
    text_ctx->pipe = text_pipe;
    text_ctx->type = TYPE_REQUEST;
    text_ctx->request_metadata.endpoint = endpoint;
    text_ctx->request_metadata.model_id = model_id;
    text_ctx->request_metadata.max_tokens = max_tokens;

    // Create pipe for previous query preparation rb and resp parsing rb
    struct rbPipe *resp_pipe = malloc(sizeof(struct rbPipe));
    resp_pipe->buf_a = curl_rb;

    struct mpmcRingBuffer *resp_rb = rb_create();
    resp_pipe->buf_b = resp_rb;
    resp_pipe->producer_finished = 0;

    factoryExecutionContext *resp_ctx = malloc(sizeof(factoryExecutionContext));
    resp_ctx->pipe = resp_pipe;
    resp_ctx->type = TYPE_PARSE_RANK;

    SharedPtr_factoryExecutionContext *text_sp = SharedPtr_factoryExecutionContext_new(text_ctx);
    SharedPtr_factoryExecutionContext *resp_sp = SharedPtr_factoryExecutionContext_new(resp_ctx);

    // Deploy monitor thread
    pthread_t monitor_thread;
    pthread_create(&monitor_thread, NULL, monitor_request_metrics_fn, NULL);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Deploy worker fn for text_pipe
    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_create(&curl_thread_pool[i], NULL, read_text_and_send_req_worker_fn, text_sp);
    }


    // Deploy worker fn for resp_pipe
    pthread_create(&parse_thread, NULL, get_and_rank_responses_worker_fn, resp_sp);

    // Producer logic for text_rb

    int writes = 0;
    while (1) {
        struct buffer_char *line = buffer_char_new(input_len + 100);
        if (writes >= num_requests) {
            logDebug("Leaving early.");
            break;
        }
        buffer_char_memcpy(line, src->data + input_len * writes, sizeof(char) * input_len);
        if (strstr(line->data, "\r\n") != NULL) {
            line->data[strcspn(line->data, "\r\n")] = '\0'; // trim newline
            line->len = strlen(line->data);
        }
        if (strstr(line->data, "\n\n") != NULL) {
            line->data[strcspn(line->data, "\n\n")] = '\0'; // trim newline
            line->len = strlen(line->data);
        }
        if (strstr(line->data, "\"") != NULL) {
            line->data[strcspn(line->data, "\"")] = '\0'; // trim newline
            line->len = strlen(line->data);
        }
        if (strlen(line->data) == 0) {
            writes++;
            continue;
        }
        rb_put(text_rb, line);
        writes++;
    }
    __atomic_store_n(&text_pipe->producer_finished, 1, __ATOMIC_RELEASE);


    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_join(curl_thread_pool[i], NULL);
    }
    __atomic_store_n(&resp_pipe->producer_finished, 1, __ATOMIC_RELEASE);
    clock_gettime(CLOCK_MONOTONIC, &end);

    // TODO: The end marker here includes processing time here which isn't ideal
    double elapsed = elapsed_sec(start, end);
    pthread_join(parse_thread, NULL);
    fprintf(stderr, "req rate: %i, request throughput: %f\n",
            __atomic_load_n(&REQUEST_RATE, __ATOMIC_ACQUIRE),
            (double) num_requests / elapsed);
}

volatile int kill_thread = 0;

void *gc_worker_fn() {
    while (__atomic_load_n(&kill_thread, __ATOMIC_ACQUIRE) == 0) {
        for (uint64_t i = 1; i <= SharedPtrRegistryIdx; i++) {
            if (!SharedPtrRegistry[i].sp || SharedPtrRegistry[i].magic != 0xDEADBEEF) {
                continue;
            }
            if (strcmp(SharedPtrRegistry[i].type, "factoryExecutionContext") == 0) {
                SharedPtr_factoryExecutionContext *sp = SharedPtrRegistry[i].sp;
                if (__atomic_load_n(&sp->ref_count, __ATOMIC_ACQUIRE) <= 0) {
                    logDebug("Freeing factoryExecutionContext %p at idx %" PRIu64 "", sp, i);
                    free(sp->ptr->pipe);
                    free(sp->ptr);
                    free(sp);
                }
                SharedPtrRegistry[i].sp = NULL;
            }
        }
        nanosleep(&((struct timespec){5, 100000000}), NULL);
    }
    return NULL;
}


int main(int argc, char **argv) {
    curl_global_init(CURL_GLOBAL_ALL);

    pthread_t gc_thread;
    pthread_create(&gc_thread, NULL, gc_worker_fn, NULL);


    size_t buf_size = 64;

    char req_rates[buf_size];
    memset(req_rates, 0, sizeof(req_rates));

    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "error: failed to initialize curl\n");
        exit(1);
    }

    int input_len = 100;
    int max_tokens = 50;
    int num_requests = 100;
    char endpoint[buf_size];
    char model[buf_size];
    char text_url[buf_size];

    for (int i = 0; i < argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "--req-rate") == 0) {
            sprintf(req_rates, "%s", argv[i + 1]);
        }
        if (strcmp(arg, "--input-len") == 0) {
            input_len = strtol(argv[i + 1], NULL, 10);
        }
        if (strcmp(arg, "--max-tokens") == 0) {
            max_tokens = strtol(argv[i + 1], NULL, 10);
        }
        if (strcmp(arg, "--num-requests") == 0) {
            num_requests = strtol(argv[i + 1], NULL, 10);
        }
        if (strcmp(arg, "--endpoint") == 0) {
            sprintf(endpoint, "%s", argv[i + 1]);
        }
        if (strcmp(arg, "--model") == 0) {
            sprintf(model, "%s", argv[i + 1]);
        }
        if (strcmp(arg, "--text-url") == 0) {
            sprintf(text_url, "%s", argv[i + 1]);
        }
    }


    curlHandler *curl_handler = calloc(1, sizeof(curlHandler));
    curl_handler->curlResponse.data = buffer_char_new(buf_size);
    curl_handler->url = text_url;
    try_curl(curl, NULL, curl_handler, curlHandler_dataset_download_opt_setter);


    struct buffer_int *int_buf = buffer_int_new(buf_size);
    struct buffer_char *char_buf = buffer_char_new(buf_size);

    if (req_rates[0] != '\0') {
        for (int i = 0; i < buf_size; i++) {
            char c = req_rates[i];
            if (c == '\0') {
                int num = strtol(char_buf->data, NULL, 10);
                buffer_char_append(char_buf, c);
                buffer_int_append(int_buf, num);
                buffer_char_refresh(char_buf);
                break;
            }
            if (isdigit(c)) {
                buffer_char_append(char_buf, c);
            }
            if (c == ',') {
                buffer_char_append(char_buf, c);
                int num = strtol(char_buf->data, NULL, 10);
                buffer_int_append(int_buf, num);
                buffer_char_refresh(char_buf);
            }
        }
    }

    buffer_int_print(int_buf);

    int *int_values = (int *) int_buf->data;
    for (int i = 0; i < int_buf->len; i++) {
        score_perf_by_request_rate(curl_handler->curlResponse.data, int_values[i], input_len, max_tokens, num_requests,
                                   endpoint, model);
    }
    logDebug("Finished.");
    __atomic_store_n(&kill_thread, 1, __ATOMIC_RELEASE);
    pthread_join(gc_thread, NULL);
    return 0;
}

