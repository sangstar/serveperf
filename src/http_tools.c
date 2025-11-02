//
// Created by Sanger Steel on 10/22/25.
//

#include <stdlib.h>
#include <string.h>
#include "http_tools.h"
#include "debug.h"
#include "types.h"


void curlHandler_addheader(struct curlHandler *req, char *data) {
    req->headers[req->num_headers].size = strlen(data);
    if (req->headers[req->num_headers].size > MAX_HEADER_SIZE) {
        fprintf(stderr, "header too long: %s\n", data);
        exit(1);
    }
    sprintf(req->headers[req->num_headers].data, "%s", data);
    req->num_headers++;
}

int curlHandler_check_for_done(curlHandler *curl_handler) {
    size_t max_window = sizeof("[DONE]") - 1;

    char buf[32];
    memset(buf, 0, sizeof(buf));
    size_t buf_idx = 0;

    for (int i = curl_handler->curlResponse.data->len - (curl_handler->curlResponse.data->len / 4);
         i < curl_handler->curlResponse.data->len; i++) {
        char c = curl_handler->curlResponse.data->data[i];
        if (c == '[') {
            // Possibly [DONE]. Refresh buf so it'll fit
            buf_idx = 0;
            memset(buf, 0, sizeof(buf));
        }
        buf[buf_idx++] = curl_handler->curlResponse.data->data[i];
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

size_t curl_callback_dataset(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct curlHandler *curl_handler = (struct curlHandler *) userp;
    if (curl_handler->curlResponse.data->len == 0) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        curl_handler->curlResponse.first_ts = ts.tv_sec + ts.tv_nsec / 1e9;
    }
    memcpy(curl_handler->curlResponse.data, contents, realsize);
    if (curlHandler_check_for_done(curl_handler)) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        curl_handler->curlResponse.last_ts = ts.tv_sec + ts.tv_nsec / 1e9;
    };
    return realsize;
}

size_t curl_callback_oai(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct curlHandler *curl_handler = (struct curlHandler *) userp;

    if (curl_handler->curlResponse.data->len == 0) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        curl_handler->curlResponse.first_ts = ts.tv_sec + ts.tv_nsec / 1e9;
    }
    buffer_char_memcpy(curl_handler->curlResponse.data, contents, realsize);
    if (curlHandler_check_for_done(curl_handler)) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        curl_handler->curlResponse.last_ts = ts.tv_sec + ts.tv_nsec / 1e9;
    };
    return realsize;
}

void curl_setopt_from_curlHandler(CURL *curl, struct curl_slist *headers, const struct curlHandler *req) {
    curl_easy_setopt(curl, CURLOPT_URL, req->url);
    for (int i = 0; i < req->num_headers; i++) {
        if (req->headers[i].size > 0) {
            headers = curl_slist_append(headers, req->headers[i].data);
        }
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
}

CURLcode try_curl(CURL *curl, void *type, curlHandler *resp,
                  void (*curl_opt_setter)(curlHandler *resp, void *, CURL *)) {
    curl_opt_setter(resp, type, curl);
    return curl_easy_perform(curl);
}

void curlHandler_dataset_download_opt_setter(struct curlHandler *req, void *data, CURL *curl) {
    if (curl) {
        FILE *fp = fopen("output.txt", "wb");
        curl_easy_setopt(curl, CURLOPT_URL, "https://norvig.com/big.txt");
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // follow redirects
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-test/1.0");
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 102400L); // large buffer
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(fp);
    }
}

