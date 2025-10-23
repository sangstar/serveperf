//
// Created by Sanger Steel on 10/22/25.
//

#include "debug.h"
#include <stdio.h>
#include "parse.h"

void print_oai_response(struct oaiResponse *resp) {
    logDebug(
        "oaiResponse "
        "p=%p id=%s object=%s "
        "created=%u model=%s text=%s finish_reason=%i",
        resp, resp->id, resp->object, resp->created, resp->model,
        resp->resp_metrics.text, resp->resp_metrics.finish_reason
    );
}

void print_oai_response_perf(struct oaiResponsePerf *perf) {
    logDebug(
        "oaiResponsePerf "
        "p=%p latency=%f throughput=%f "
        "ttft=%f tokens_count=%f  response_len=%lu response=\"%s\"",
        perf, perf->latency, perf->throughput, perf->ttft, perf->tokens_count,
        perf->response_len, perf->response
    );
}
