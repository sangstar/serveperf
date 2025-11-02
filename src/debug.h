//
// Created by Sanger Steel on 10/22/25.
//

#ifndef SERVEPERF_DEBUG_H
#define SERVEPERF_DEBUG_H
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>


static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static int set_nobuff = 0;
static pthread_once_t logfile_open_once = PTHREAD_ONCE_INIT;

FILE *logfile;

static void open_logfile() {
    logfile = fopen("/tmp/serveperf.log", "w");
}

#ifdef DEBUG
#define logDebug(fmt, ...) \
do { \
    pthread_mutex_lock(&log_mutex); \
    char _debug_log_buffer[1024]; \
    int len = snprintf(_debug_log_buffer, sizeof(_debug_log_buffer), \
    "[ serveperf %s tid%-15" PRIuMAX " | %-15s | %-35s:%-3i ] DEBUG: " fmt "\n", \
    __TIME__, (uintmax_t)pthread_self(), __FILE_NAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    fwrite(_debug_log_buffer, 1, len, stdout); \
    fflush(stdout); \
    pthread_mutex_unlock(&log_mutex); \
} while(0)
#define PRINT_OAI_RESPONSE(resp) print_oai_response(resp)
#define PRINT_OAI_RESPONSE_PERF(perf) print_oai_response_perf(perf)
#else
#define logDebug(fmt, ...) ((void)0)
#define PRINT_OAI_RESPONSE(resp) ((void)0)
#define PRINT_OAI_RESPONSE_PERF(perf) ((void)0)
#endif


struct oaiResponse;

void print_oai_response(struct oaiResponse *resp);

struct oaiResponsePerf;

void print_oai_response_perf(struct oaiResponsePerf *perf);

#endif //SERVEPERF_DEBUG_H
