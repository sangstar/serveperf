//
// Created by Sanger Steel on 10/22/25.
//

#ifndef SERVEPERF_PARSE_H
#define SERVEPERF_PARSE_H
#include <stddef.h>
#include <stdint.h>
#include "http_tools.h"

#define MAX_RESPONSE_LEN (1024 * 1024)

enum FinishReason {
    STOP,
    NONE,
    UNK,
};


struct oaiResponsePerf {
    double latency;
    double throughput;
    double ttft;
    double tokens_count;
    char response[MAX_RESPONSE_LEN];
    size_t response_len;
};

struct oaiResponse {
    char *id;
    char *object;
    uint32_t created;
    char *model;

    struct {
        char *text;
        enum FinishReason finish_reason;
    } resp_metrics;

    // TODO: We're just measuring latencies and throughputs,
    //       so we don't care about marshalling logprobs and related
    //       stuff

    // struct {
    //     char *text;
    //     int value;
    // } logprob;

    // struct {
    //     char **tokens;
    //     int *token_logprobs;
    //     struct logprob **top_logprobs;
    //     uint8_t *text_offset;
    // } logprobs;

    // struct {
    //     char *text;
    //     uint8_t index;
    //     struct logprobs *logprobs;
    //     enum FinishReason finish_reason;
    // } *choices;
};

enum oaiResponse_ParseState {
    START = (1 << 0),
    READ_KEY = (1 << 1),
    READ_VALUE = (1 << 2),
    GOT_COLON = (1 << 3),
    GOT_ARRAY = (1 << 4),
    GOT_OBJECT = (1 << 5),
    UNKNOWN = (1 << 6)
};

struct jsonKeyValue {
    char *key;
    char *value;
};

struct jsonBuf {
    char buf[256];
    size_t buf_idx;
    size_t buf_size;
};

void jsonBuf_append(struct jsonBuf *buf, char c);

void jsonBuf_refresh(struct jsonBuf *buf);


void oaiResponse_set_from_jsonKeyValue(struct oaiResponse *resp, struct jsonKeyValue *kv);

void oaiResponsePerf_set_from_curlResponse(struct oaiResponsePerf *perf,
                                           struct curlResponse *curlResp);

double oaiResponsePerf_score(struct oaiResponsePerf *perf);

#endif //SERVEPERF_PARSE_H
