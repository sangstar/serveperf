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

int main(void) {
    curl_global_init(CURL_GLOBAL_ALL);

    FILE *fp = fopen("../alice29.txt", "r");
    if (!fp) {
        fprintf(stderr, "error: failed to open alice29.txt\n");
        exit(1);
    }


    struct mpmcRingBuffer *text_rb = rb_create();

    struct rbPipe *text_pipe = malloc(sizeof(struct rbPipe));
    struct mpmcRingBuffer *curl_rb = rb_create();

    text_pipe->buf_a = text_rb;
    text_pipe->buf_b = curl_rb;
    text_pipe->finished = 0;

    pthread_t monitor_thread;
    pthread_create(&monitor_thread, NULL, monitor_request_metrics_fn, NULL);

    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_create(&curl_thread_pool[i], NULL, (void *(*)(void *)) read_text_and_send_req_worker_fn, text_pipe);
    }

    char line[INPUT_LEN];

    while (fgets(line, INPUT_LEN, fp)) {
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
    }

    __atomic_store_n(&text_pipe->finished, 1, __ATOMIC_RELEASE);


    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_join(curl_thread_pool[i], NULL);
    }

    struct mpmcRingBuffer *perf_rb = rb_create();
    struct oaiResponsePerf *perf_ptr = NULL;

    logDebug("Finished.");
    return 0;
}
