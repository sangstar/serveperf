//
// Created by Sanger Steel on 10/22/25.
//

#include "parse.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "http_tools.h"
#include "types.h"


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
        fprintf(
            stderr,
            "response would max response length, strlen(%s)=%lu + perf->response_len=%lu = %lu > %i bytes: %s\n",
            resp->resp_metrics.text,
            len,
            perf->response_len,
            perf->response_len + len,
            MAX_RESPONSE_LEN,
            resp->resp_metrics.text);
        PRINT_OAI_RESPONSE_PERF(perf);
        exit(1);
    }
    memcpy(perf->response + perf->response_len, resp->resp_metrics.text, len);
    perf->response_len += len;
    perf->response[perf->response_len + 1] = '\0';
    perf->tokens_count++;
}

void oaiResponsePerf_set_from_curlResponse(struct oaiResponsePerf *perf,
                                           curlHandler *curl_handler) {
    enum oaiResponse_ParseState state = UNKNOWN;
    struct jsonBuf buf = {0};

    struct oaiResponse resp = {0};

    struct jsonKeyValue kv = {0};

    double ttft = curl_handler->curlResponse.first_ts - curl_handler->curlResponse.send_ts;
    perf->ttft = ttft;
    perf->latency = curl_handler->curlResponse.last_ts - curl_handler->curlResponse.send_ts;

    for (int i = 0; i < curl_handler->curlResponse.data->len; i++) {
        char c = curl_handler->curlResponse.data->data[i];
        char next = curl_handler->curlResponse.data->data[i + 1];
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
    if (perf->response_len == 0) {
        fprintf(stderr, "response invalid for parsing: %s", curl_handler->data);
        exit(1);
    }
    perf->throughput = perf->tokens_count / perf->latency;
}

double oaiResponsePerf_score(struct oaiResponsePerf *perf) {
    return perf->throughput / perf->ttft;
}

void query_openai_endpoint(curlHandler *resp, const char *endpoint, const struct buffer_char *prompt,
                           const char *model,
                           int max_tokens) {
    CURL *curl;
    curl = curl_easy_init();

    struct oaiRequest req = {0};
    req.endpoint = endpoint;
    req.model_id = model;
    req.prompt = prompt;
    req.max_tokens = max_tokens;

    try_curl(curl, &req, resp, curlHandler_openai_curl_opt_setter);


    if (!curl) {
        fprintf(stderr, "Curl initialization failed\n");
        return;
    }

    CURLcode res = curl_easy_perform(curl);

    if (res == CURLE_OPERATION_TIMEDOUT) {
        logDebug("curl timed out, retrying..");
    } else if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
        exit(1);
    }
    curl_easy_cleanup(curl);
};


void curlHandler_openai_curl_opt_setter(curlHandler *req, void *data, CURL *curl) {
    struct oaiRequest *oaiReq = (struct oaiRequest *) data;
    char payload[2048];
    memset(payload, 0, sizeof(payload));

    snprintf(payload,
             sizeof(payload),
             "{"
             "\"model\": \"%s\","
             "\"prompt\": \"%s\","
             "\"max_tokens\": %d,"
             "\"stream\": true"
             "}",
             oaiReq->model_id,
             oaiReq->prompt->data,
             oaiReq->max_tokens);
    req->url = oaiReq->endpoint;
    req->data = strdup(payload);

    char *api_key = getenv("OPENAI_API_KEY");
    if (!api_key) {
        fprintf(stderr, "OPENAI_API_KEY not set\n");
        exit(1);
    }
    char auth_text[256];
    sprintf(auth_text, "Authorization: Bearer %s", api_key);

    curlHandler_addheader(req, strdup(auth_text));
    curlHandler_addheader(req, strdup("Content-Type: application/json"));

    curl_setopt_from_curlHandler(curl, NULL, req);

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)req);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback_oai);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req->data);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(req->data));
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);


    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    req->curlResponse.send_ts = ts.tv_sec + ts.tv_nsec / 1e9;
}

