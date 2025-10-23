//
// Created by Sanger Steel on 10/22/25.
//

#ifndef SERVEPERF_CURL_TYPES_H
#define SERVEPERF_CURL_TYPES_H
#include <stddef.h>
#include <curl/curl.h>
#include "parse.h"


#define MAX_HEADERS 8
#define MAX_HEADER_SIZE 256
#define MAX_RESPONSE_BODY (1024 * 10)

struct curlResponse {
    char data[MAX_RESPONSE_BODY];
    size_t responseSize;
    double send_ts;
    double first_ts;
    double last_ts;
};


struct curlRequest {
    char *url;
    char *data;

    struct {
        char data[MAX_HEADER_SIZE];
        size_t size;
    } headers[MAX_HEADERS];

    size_t num_headers;
};

void curlRequest_addheader(struct curlRequest *req, char *data);


size_t curl_callback_default(void *contents, size_t size, size_t nmemb, void *userp);

void curl_setopt_from_curlRequest(CURL *curl, struct curl_slist *headers, const struct curlRequest *req);


void query_openai_endpoint(
    struct curlResponse *resp,
    char *url,
    const char *prompt,
    const char *model
);

#endif //SERVEPERF_CURL_TYPES_H
