//
// Created by Sanger Steel on 10/22/25.
//

#include "parse.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "http_tools.h"


void jsonBuf_append(struct jsonBuf *buf, char c) {
    if (c != '\"') buf->buf[buf->buf_idx++] = c;
}

void jsonBuf_refresh(struct jsonBuf *buf) {
    buf->buf[buf->buf_idx] = 0;
    buf->buf_idx = 0;
    memset(buf->buf, 0, sizeof(buf->buf));
}

void oaiResponse_set_from_jsonKeyValue(struct oaiResponse *resp, struct jsonKeyValue *kv) {
    if (strcmp(kv->key, "id") == 0) {
        resp->id = strdup(kv->value);
    } else if (strcmp(kv->key, "object") == 0) {
        resp->object = strdup(kv->value);
    } else if (strcmp(kv->key, "model") == 0) {
        resp->model = strdup(kv->value);
    } else if (strcmp(kv->key, "created") == 0) {
        resp->created = strtoul(kv->value, NULL, 10);
    } else if (strcmp(kv->key, "text") == 0) {
        resp->resp_metrics.text = strdup(kv->value);
    } else if (strcmp(kv->key, "finish_reason") == 0) {
        if (strcmp(kv->value, "stop") == 0) resp->resp_metrics.finish_reason = STOP;
        else if (strcmp(kv->value, "null") == 0) resp->resp_metrics.finish_reason = UNK;
        else resp->resp_metrics.finish_reason = UNK;
    }
}

void oaiResponsePerf_add_response(struct oaiResponsePerf *perf, struct oaiResponse *resp) {
    size_t len = strlen(resp->resp_metrics.text);
    if (perf->response_len + len >= MAX_RESPONSE_LEN) {
        fprintf(stderr, "Response would max response length of %i bytes: %s\n", MAX_RESPONSE_LEN,
                resp->resp_metrics.text);
        exit(1);
    }
    memcpy(perf->response + perf->response_len, resp->resp_metrics.text, len);
    perf->response_len += len;
    perf->response[perf->response_len + 1] = '\0';
    perf->tokens_count++;
}

void oaiResponsePerf_set_from_curlResponse(struct oaiResponsePerf *perf,
                                           struct curlResponse *curlResp) {
    enum oaiResponse_ParseState state = UNKNOWN;
    struct jsonBuf buf = {0};

    struct oaiResponse resp = {0};

    struct jsonKeyValue kv = {0};

    double ttft = curlResp->first_ts - curlResp->send_ts;
    perf->ttft = ttft;
    perf->latency = curlResp->last_ts - curlResp->send_ts;

    for (int i = 0; i < curlResp->responseSize; i++) {
        char c = curlResp->data[i];
        char next = curlResp->data[i + 1];
        switch (state) {
            case UNKNOWN:
                if (c == 'd') jsonBuf_refresh(&buf);
                jsonBuf_append(&buf, c);
                if (strcmp(buf.buf, "data: ") == 0) {
                    state = START;
                    if (resp.id) {
                        oaiResponsePerf_add_response(perf, &resp);
                    }
                    resp = (struct oaiResponse){0};
                    jsonBuf_refresh(&buf);
                }
            case START:
                if (next == '\"') state = READ_KEY;
                if (next == '}') state = UNKNOWN;
                break;
            case READ_KEY | GOT_COLON:
                if (next == '\"' || isdigit(next) || isalpha(next)) state = READ_VALUE;
                if (next == '[' || c == '{') state = START; // Ignore array/object types
                break;
            case READ_KEY:
                jsonBuf_append(&buf, c);
                if (next == ':') {
                    state = state | GOT_COLON;
                    kv.key = calloc(buf.buf_idx + 1, sizeof(char));
                    memcpy(kv.key, buf.buf, buf.buf_idx); // ignore the final \": bytes
                    jsonBuf_refresh(&buf);
                }
                break;
            case READ_VALUE:
                jsonBuf_append(&buf, c);
                if (next == '\"' || next == ',' || next == '\n' || next == '}' || next == ']') {
                    state = START;
                    kv.value = calloc(buf.buf_idx + 1, sizeof(char));
                    memcpy(kv.value, buf.buf, buf.buf_idx);

                    oaiResponse_set_from_jsonKeyValue(&resp, &kv);

                    free(kv.key);
                    kv.key = NULL;
                    free(kv.value);
                    kv.value = NULL;

                    jsonBuf_refresh(&buf);
                    break;
                }
            default: ;
        }
    }
    perf->throughput = perf->tokens_count / perf->latency;
}

double oaiResponsePerf_score(struct oaiResponsePerf *perf) {
    return perf->throughput / perf->ttft;
}
