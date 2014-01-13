/*
* mod_dup - duplicates apache requests
*
* Copyright (C) 2013 Orange
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "mod_dup.hh"

#include <http_config.h>

namespace DupModule {

apr_status_t
inputFilterHandler(ap_filter_t *pF, apr_bucket_brigade *pB, ap_input_mode_t pMode, apr_read_type_e pBlock, apr_off_t pReadbytes)
{
    apr_status_t lStatus = ap_get_brigade(pF->next, pB, pMode, pBlock, pReadbytes);
    if (lStatus != APR_SUCCESS) {
        return lStatus;
    }
    request_rec *pRequest = pF->r;
    if (pRequest) {
	struct DupConf *tConf = reinterpret_cast<DupConf *>(ap_get_module_config(pRequest->per_dir_config, &dup_module));
	if (!tConf) {
            return OK; // SHOULD NOT HAPPEN
	}
        // Request ID extract
        apr_table_t *headersIn = pRequest->headers_in;
        // No context? new request
        if (!pF->ctx) {
            RequestInfo *info = new RequestInfo(tConf->getNextReqId());
            std::string reqId = boost::lexical_cast<std::string>(info->mId);
            apr_table_set(headersIn, c_UNIQUE_ID, reqId.c_str());
            apr_table_set(pRequest->headers_out, c_UNIQUE_ID, reqId.c_str());
            ap_set_module_config(pRequest->request_config, &dup_module, (void *)info);
            pF->ctx = info;
        } else if (pF->ctx == (void *)1) {
            return OK;
        }
        RequestInfo *pBH = static_cast<RequestInfo *>(pF->ctx);
        // TODO Body is stored only if the payload flag is activated
        for (apr_bucket *b = APR_BRIGADE_FIRST(pB);
             b != APR_BRIGADE_SENTINEL(pB);
             b = APR_BUCKET_NEXT(b) ) {
#ifndef UNIT_TESTING
            // Metadata end of stream
            if ( APR_BUCKET_IS_EOS(b) ) {
#endif
                // TODO Do context enrichment synchronously
                gProcessor->enrichContext();
                pF->ctx = (void *)1;
                break;
#ifndef UNIT_TESTING
            }
#endif
            const char* lReqPart = NULL;
            apr_size_t lLength = 0;
            apr_status_t lStatus = apr_bucket_read(b, &lReqPart, &lLength, APR_BLOCK_READ);
            if ((lStatus != APR_SUCCESS) || (lReqPart == NULL)) {
                continue;
            }
            pBH->mBody += std::string(lReqPart, lLength);
        }
    }
    return OK;
}

/*
 * Request context used during brigade run
 */
class RequestContext {
public:
    apr_bucket_brigade  *tmpbb;
    RequestInfo         *req; /** Req is pushed to requestprocessor for duplication and will be deleted later*/

    RequestContext(ap_filter_t *pFilter) {
        tmpbb = apr_brigade_create(pFilter->r->pool, pFilter->c->bucket_alloc);
        req = reinterpret_cast<RequestInfo *>(ap_get_module_config(pFilter->r->request_config, &dup_module));
        assert(req);
    }

    ~RequestContext() {
        apr_brigade_cleanup(tmpbb);
    }
};

void
prepareRequestInfo(DupConf *tConf, request_rec *pRequest, RequestInfo &r) {
    r.mPoison = false;
    r.mConfPath = tConf->dirName;
    r.mPath = pRequest->uri;
    r.mArgs = pRequest->args ? pRequest->args : "";
}

static void
printRequest(request_rec *pRequest, RequestInfo *pBH, DupConf *tConf) {
    const char *reqId = apr_table_get(pRequest->headers_in, c_UNIQUE_ID);
    Log::debug("### Pushing a request with ID: %s, body size:%s", reqId, boost::lexical_cast<std::string>(pBH->mBody.size()).c_str());
    Log::debug("### Uri:%s, dir name:%s", pRequest->uri, tConf->dirName);
    Log::debug("### Request args: %s", pRequest->args);
}

apr_status_t
outputFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade) {
    request_rec *pRequest = pFilter->r;
    if (!pRequest || !pRequest->per_dir_config)
        return ap_pass_brigade(pFilter->next, pBrigade);
    struct DupConf *tConf = reinterpret_cast<DupConf *>(ap_get_module_config(pRequest->per_dir_config, &dup_module));
    assert(tConf);
    // Request answer analyse
    RequestContext *ctx = static_cast<RequestContext *>(pFilter->ctx);
    if (ctx == NULL) {
        // Context init
        ctx = new RequestContext(pFilter);
        pFilter->ctx = ctx;

    } else if (ctx == (void *) -1) {
        return ap_pass_brigade(pFilter->next, pBrigade);
    }

    if (DuplicationType::value != DuplicationType::REQUEST_WITH_ANSWER) {
        // Asynchronous push of request without the answer
        RequestInfo *rH = ctx->req;
        prepareRequestInfo(tConf, pRequest, *rH);
        printRequest(pRequest, rH, tConf);
        gThreadPool->push(rH);
        delete ctx;
        pFilter->ctx = (void *) -1;
        return ap_pass_brigade(pFilter->next, pBrigade);
    }
    // Asynchronous push of request WITH the answer
    apr_bucket *currentBucket;
    while ((currentBucket = APR_BRIGADE_FIRST(pBrigade)) != APR_BRIGADE_SENTINEL(pBrigade)) {
        const char *data;
        apr_size_t len;
        apr_status_t rv;
        rv = apr_bucket_read(currentBucket, &data, &len, APR_BLOCK_READ);

        if ((rv == APR_SUCCESS) && (data != NULL)) {
            ctx->req->mAnswer.append(data, len);
        }
        /* Remove bucket e from bb. */
        APR_BUCKET_REMOVE(currentBucket);
        /* Insert it into  temporary brigade. */
        APR_BRIGADE_INSERT_HEAD(ctx->tmpbb, currentBucket);
        /* Pass brigade downstream. */
        rv = ap_pass_brigade(pFilter->next, ctx->tmpbb);
        // TODO if (rv) ...;
        if (APR_BUCKET_IS_EOS(currentBucket)) {
            apr_brigade_cleanup(ctx->tmpbb);
            // Pushing the answer to the processor
            // TODO dissociate body from header if possible
            prepareRequestInfo(tConf, pRequest, *(ctx->req));
            printRequest(pRequest, ctx->req, tConf);
            gThreadPool->push(ctx->req);
            delete ctx;
            pFilter->ctx = (void *) -1;
        }
        else {
        }
    }
    return OK;
}

};
