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

const unsigned int CMaxBytes = 8192;

bool
extractBrigadeContent(apr_bucket_brigade *bb, request_rec *pRequest, std::string &content) {
    if (ap_get_brigade(pRequest->input_filters,
                       bb, AP_MODE_READBYTES, APR_BLOCK_READ, CMaxBytes) == APR_SUCCESS) {
        // Read brigade content
        for (apr_bucket *b = APR_BRIGADE_FIRST(bb);
             b != APR_BRIGADE_SENTINEL(bb);
             b = APR_BUCKET_NEXT(b) ) {
            // Metadata end of stream
            if (APR_BUCKET_IS_EOS(b)) {
                return true;
            }
            const char *data = 0;
            apr_size_t len = 0;
            apr_status_t rv = apr_bucket_read(b, &data, &len, APR_BLOCK_READ);
            if (rv != APR_SUCCESS) {
                Log::error(42, "Bucket read failed, skipping the rest of the body");
                return true;
            }
            if (len) {
                content.append(data, len);
            }
        }
    }
    else {
        Log::error(42, "Get brigade failed, skipping the rest of the body");
        return true;
    }
    return false;
}


int
translateHook(request_rec *pRequest) {
    if (!pRequest->per_dir_config)
        return DECLINED;
    DupConf *tConf = reinterpret_cast<DupConf *>(ap_get_module_config(pRequest->per_dir_config, &dup_module));
    if (!tConf || !tConf->dirName) {
        // Not a location that we treat, we decline the request
        return DECLINED;
    }
    RequestInfo *info = new RequestInfo(tConf->getNextReqId());
    // Backup in request context
    ap_set_module_config(pRequest->request_config, &dup_module, (void *)info);

    if (!pRequest->connection->pool) {
        Log::error(42, "No connection pool associated to the request");
        return DECLINED;
    }
    if (!pRequest->connection->bucket_alloc) {
        pRequest->connection->bucket_alloc = apr_bucket_alloc_create(pRequest->connection->pool);
        if (!pRequest->connection->bucket_alloc) {
            Log::error(42, "Request bucket allocation failed");
            return DECLINED;
        }
    }

    apr_bucket_brigade *bb = apr_brigade_create(pRequest->connection->pool, pRequest->connection->bucket_alloc);
    if (!bb) {
        Log::error(42, "Bucket brigade allocation failed");
        return DECLINED;
    }
    while (!extractBrigadeContent(bb, pRequest, info->mBody)){
        apr_brigade_cleanup(bb);
    }
    apr_brigade_cleanup(bb);
    // Body read :)

    // Copy Request ID in both headers
    std::string reqId = boost::lexical_cast<std::string>(info->mId);
    apr_table_set(pRequest->headers_in, c_UNIQUE_ID, reqId.c_str());
    apr_table_set(pRequest->headers_out, c_UNIQUE_ID, reqId.c_str());

    // Synchronous context enrichment
    info->mConfPath = tConf->dirName;
    info->mArgs = pRequest->args ? pRequest->args : "";
    gProcessor->enrichContext(pRequest, *info);

    return DECLINED;
}


/**
 * Reinject the body we saved in earlyHook into a brigade
 */
apr_status_t
inputFilterBody2Brigade(ap_filter_t *pF, apr_bucket_brigade *pB, ap_input_mode_t pMode, apr_read_type_e pBlock, apr_off_t pReadbytes)
{

    request_rec *pRequest = pF->r;
    if (!pRequest || !pRequest->per_dir_config) {
        apr_bucket *e = apr_bucket_eos_create(pF->c->bucket_alloc);
        assert(e);
        APR_BRIGADE_INSERT_TAIL(pB, e);
        return APR_SUCCESS;
    }
    // Retrieve request info from context
    RequestInfo *info = reinterpret_cast<RequestInfo *>(ap_get_module_config(pRequest->request_config, &dup_module));
    if (!info) {
        // Should not happen
        apr_bucket *e = apr_bucket_eos_create(pF->c->bucket_alloc);
        assert(e);
        APR_BRIGADE_INSERT_TAIL(pB, e);
        return APR_SUCCESS;
    }
    if (!pF->ctx) {
        if (!info->mBody.empty()) {
            apr_status_t st;
            Log::warn(1, "Wrote body to brigade: %s",  info->mBody.c_str());
            if ((st = apr_brigade_write(pB, NULL, NULL, info->mBody.c_str(), info->mBody.size())) != APR_SUCCESS ) {
                Log::warn(1, "Failed to write request body in a brigade: %s",  info->mBody.c_str());
                return st;
            }
        }
        // Marks the body as sent
        pF->ctx = (void *) -1;
    }
    // Marks that we do not have any more data to read
    apr_bucket *e = apr_bucket_eos_create(pF->c->bucket_alloc);
    assert(e);
    APR_BRIGADE_INSERT_TAIL(pB, e);
    return APR_SUCCESS;
}

/*
 * Callback to iterate over the headers tables
 * Pushes a copy of key => value in a list
 */
static int iterateOverHeadersCallBack(void *d, const char *key, const char *value) {
    RequestInfo::tHeaders *headers = reinterpret_cast<RequestInfo::tHeaders *>(d);
    headers->push_back(std::pair<std::string, std::string>(key, value));
    return 1;
}

static void
prepareRequestInfo(DupConf *tConf, request_rec *pRequest, RequestInfo &r, bool withAnswer) {
    // Basic
    r.mPoison = false;
    r.mConfPath = tConf->dirName;
    r.mPath = pRequest->uri;
    r.mArgs = pRequest->args ? pRequest->args : "";

    // Copy headers in
    apr_table_do(&iterateOverHeadersCallBack, &r.mHeadersIn, pRequest->headers_in, NULL);
    if (withAnswer) {
        // Copy headers out
        apr_table_do(&iterateOverHeadersCallBack, &r.mHeadersOut, pRequest->headers_out, NULL);
    }
}

static void
printRequest(request_rec *pRequest, RequestInfo *pBH, DupConf *tConf) {
    const char *reqId = apr_table_get(pRequest->headers_in, c_UNIQUE_ID);
    Log::debug("### Pushing a request with ID: %s, body size:%ld", reqId, pBH->mBody.size());
    Log::debug("### Uri:%s, dir name:%s", pRequest->uri, tConf->dirName);
    Log::debug("### Request args: %s", pRequest->args);
}

/*
 * Request context used during brigade run
 */
class RequestContext {
public:
    apr_bucket_brigade  *mTmpBB;
    RequestInfo         *mReq; /** Req is pushed to requestprocessor for duplication and will be deleted later*/

    RequestContext(ap_filter_t *pFilter) {
        mTmpBB = apr_brigade_create(pFilter->r->pool, pFilter->c->bucket_alloc);
        mReq = reinterpret_cast<RequestInfo *>(ap_get_module_config(pFilter->r->request_config,
                                                                    &dup_module));
    }

    RequestContext()
        : mTmpBB(NULL)
        , mReq(NULL) {
    }

    ~RequestContext() {
        if (mTmpBB) {
            apr_brigade_cleanup(mTmpBB);
        }
    }
};


/**
 * Here we can get the response body and finally duplicate or not
 */
apr_status_t
outputFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade) {
    request_rec *pRequest = pFilter->r;
    // Reject requests that do not meet our requirements
    if (!pRequest || !pRequest->per_dir_config)
        return ap_pass_brigade(pFilter->next, pBrigade);

    struct DupConf *tConf = reinterpret_cast<DupConf *>(ap_get_module_config(pRequest->per_dir_config, &dup_module));
    if ((!tConf) || (!tConf->dirName) || (tConf->getHighestDuplicationType() == DuplicationType::NONE)) {
         return ap_pass_brigade(pFilter->next, pBrigade);
    }

    // Request answer analyse
    RequestContext *ctx = static_cast<RequestContext *>(pFilter->ctx);
    if (ctx == NULL) {
        // First pass in the output filter => context init
        ctx = new RequestContext(pFilter);
        pFilter->ctx = ctx;
    } else if (ctx == (void *) -1) {
        // Output filter already did his job, we return
        return ap_pass_brigade(pFilter->next, pBrigade);
    }

    // We need to get the highest one as we haven't matched which rule it is yet
    if (tConf->getHighestDuplicationType() != DuplicationType::REQUEST_WITH_ANSWER) {
        // Asynchronous push of request WITHOUT the answer
        RequestInfo *rH = ctx->mReq;
        prepareRequestInfo(tConf, pRequest, *rH, false);
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
            ctx->mReq->mAnswer.append(data, len);
        }
        /* Remove bucket e from bb. */
        APR_BUCKET_REMOVE(currentBucket);
        /* Insert it into  temporary brigade. */
        APR_BRIGADE_INSERT_HEAD(ctx->mTmpBB, currentBucket);
        /* Pass brigade downstream. */
        rv = ap_pass_brigade(pFilter->next, ctx->mTmpBB);
        if (rv != APR_SUCCESS) {
            // Something went wrong, no duplication performed
            delete ctx;
            pFilter->ctx = (void *) -1;
            return rv;
        }
        if (APR_BUCKET_IS_EOS(currentBucket)) {
            // Pushing the answer to the processor
            prepareRequestInfo(tConf, pRequest, *(ctx->mReq), true);
            printRequest(pRequest, ctx->mReq, tConf);
            gThreadPool->push(ctx->mReq);
            delete ctx;
            pFilter->ctx = (void *) -1;
        }
        else {
            apr_brigade_cleanup(ctx->mTmpBB);
        }
    }
    return OK;
}

};
