//
// Created by Sanger Steel on 10/22/25.
//

#ifndef SERVEPERF_CURL_TYPES_H
#define SERVEPERF_CURL_TYPES_H
#include <stddef.h>
#include <curl/curl.h>
#include "shared_ptr.h"


#define MAX_HEADERS 8
#define MAX_HEADER_SIZE 256
#define MAX_RESPONSE_BODY (1024 * 100)


typedef struct curlHandler {
    char *url;
    char *data;

    struct {
        char data[MAX_HEADER_SIZE];
        size_t size;
    } headers[MAX_HEADERS];

    size_t num_headers;

    void (*curl_opt_setter)(CURL *);

    struct {
        struct buffer_char *data;
        double send_ts;
        double first_ts;
        double last_ts;
    } curlResponse;
} curlHandler;

DECLARE_SHARED_PTR(curlHandler);

void curlHandler_addheader(struct curlHandler *req, char *data);


size_t curl_callback_oai(void *contents, size_t size, size_t nmemb, void *userp);

void curl_setopt_from_curlHandler(CURL *curl, struct curl_slist *headers, const struct curlHandler *req);

CURLcode try_curl(CURL *curl, void *type, curlHandler *resp,
                  void (*curl_opt_setter)(curlHandler *, void *, CURL *));

void curlHandler_dataset_download_opt_setter(curlHandler *req, void *data, CURL *curl);

#endif //SERVEPERF_CURL_TYPES_H
