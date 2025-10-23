//
// Created by Sanger Steel on 10/22/25.
//

#include <stdlib.h>
#include <string.h>
#include "http_tools.h"
#include "debug.h"

void curlRequest_addheader(struct curlRequest *req, char *data) {
    req->headers[req->num_headers].size = strlen(data);
    if (req->headers[req->num_headers].size > MAX_HEADER_SIZE) {
        fprintf(stderr, "header too long: %s\n", data);
        exit(1);
    }
    sprintf(req->headers[req->num_headers].data, "%s", data);
    req->num_headers++;
}

int curlResponse_check_for_done(struct curlResponse *resp) {
    size_t max_window = sizeof("[DONE]") - 1;

    char buf[32];
    memset(buf, 0, sizeof(buf));
    size_t buf_idx = 0;

    for (int i = resp->responseSize - (resp->responseSize / 4); i < resp->responseSize; i++) {
        char c = resp->data[i];
        if (c == '[') {
            // Possibly [DONE]. Refresh buf so it'll fit
            buf_idx = 0;
            memset(buf, 0, sizeof(buf));
        }
        buf[buf_idx++] = resp->data[i];
        if (buf_idx > max_window) {
            buf_idx = 0;
            memset(buf, 0, sizeof(buf));
        }
        if (strcmp(buf, "[DONE]") == 0) {
            return 1;
        }
    }
    return 0;
}

size_t curl_callback_default(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct curlResponse *resp = (struct curlResponse *) userp;
    if (resp->responseSize + realsize >= MAX_RESPONSE_LEN) {
        fprintf(stderr, "Response exceeds max response length of %i bytes\n", MAX_RESPONSE_LEN);
        exit(1);
    }
    if (resp->responseSize == 0) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        resp->first_ts = ts.tv_sec + ts.tv_nsec / 1e9;
    }
    memcpy(resp->data + resp->responseSize, contents, realsize);
    resp->responseSize += realsize;
    resp->data[resp->responseSize + 1] = '\0';
    if (curlResponse_check_for_done(resp)) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        resp->last_ts = ts.tv_sec + ts.tv_nsec / 1e9;
    };
    return realsize;
}

void curl_setopt_from_curlRequest(CURL *curl, struct curl_slist *headers, const struct curlRequest *req) {
    curl_easy_setopt(curl, CURLOPT_URL, req->url);
    for (int i = 0; i < req->num_headers; i++) {
        if (req->headers[i].size > 0) {
            headers = curl_slist_append(headers, req->headers[i].data);
        }
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
}

void query_openai_endpoint(struct curlResponse *resp, char *url, const char *prompt, const char *model) {
    CURL *curl;
    struct curlRequest req = {0};

    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Curl initialization failed\n");
        return;
    }

    char payload[512];
    memset(payload, 0, sizeof(payload));

    sprintf(payload,
            "{"
            "\"model\": \"%s\","
            "\"prompt\": \"%s\","
            "\"stream\": true"
            "}",
            model,
            prompt);
    logDebug("payload=%.*s", (int)strlen(payload), payload);
    req.url = url;
    req.data = payload;

    char *api_key = getenv("OPENAI_API_KEY");
    if (!api_key) {
        fprintf(stderr, "OPENAI_API_KEY not set\n");
        exit(1);
    }
    char auth_text[256];
    sprintf(auth_text, "Authorization: Bearer %s", api_key);

    curlRequest_addheader(&req, strdup(auth_text));
    curlRequest_addheader(&req, strdup("Content-Type: application/json"));

    curl_setopt_from_curlRequest(curl, NULL, &req);

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)resp);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback_default);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.data);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(req.data));
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L); // seconds to wait for a connection
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // total time for entire transfer
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L); // <-- critical for threads


    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    resp->send_ts = ts.tv_sec + ts.tv_nsec / 1e9;
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
        exit(1);
    }

    curl_easy_cleanup(curl);
}
