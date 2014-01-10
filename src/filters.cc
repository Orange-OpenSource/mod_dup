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

namespace DupModule {


struct InterFilterContext {

    RequestInfo *getRequestInfo(unsigned int requestId);

    void rmRequestInfo(unsigned int requestId);

    std::map<unsigned int, RequestInfo *>     mReqs;
    boost::mutex                              mAnswerSync;

};

RequestInfo *
InterFilterContext::getRequestInfo(unsigned int requestId) {
    RequestInfo *ret = NULL;
    boost::lock_guard<boost::mutex> guard(mAnswerSync);
    std::map<unsigned int, RequestInfo *>::iterator a;
    a = mReqs.find(requestId);
    if (a == mReqs.end()) {
        ret = new RequestInfo();
        mReqs[requestId] = ret;
    }
    else
        ret = a->second;
    return ret;
}

void
InterFilterContext::rmRequestInfo(unsigned int requestId) {
    boost::lock_guard<boost::mutex> guard(mAnswerSync);
    mReqs.erase(requestId);
}


static InterFilterContext interFilterContext;


apr_status_t
analyseRequest(ap_filter_t *pF, apr_bucket_brigade *pB ) {
    request_rec *pRequest = pF->r;
    if (pRequest) {
	struct DupConf *tConf = reinterpret_cast<DupConf *>(ap_get_module_config(pRequest->per_dir_config, &dup_module));
        // Request ID extract
        apr_table_t *headersIn = pRequest->headers_in;
	if (!tConf) {
            return OK;
	}
        // No context? new request
        if (!pF->ctx) {
            // Set the request id
            unsigned int rId = tConf->getNextReqId();
            std::string reqId = boost::lexical_cast<std::string>(rId);
            apr_table_set(headersIn, "UNIQUE_ID", reqId.c_str());
            apr_table_set(pRequest->headers_out, "UNIQUE_ID", reqId.c_str());
            pF->ctx = (void *)interFilterContext.getRequestInfo(rId);
        } else if (pF->ctx == (void *)1) {
            return OK;
        }
        RequestInfo *pBH = static_cast<RequestInfo *>(pF->ctx);
        // Body is stored only if the payload flag is activated
        for (apr_bucket *b = APR_BRIGADE_FIRST(pB);
             b != APR_BRIGADE_SENTINEL(pB);
             b = APR_BUCKET_NEXT(b) ) {
#ifndef UNIT_TESTING
            // Metadata end of stream
            if ( APR_BUCKET_IS_EOS(b) ) {
#endif
                // TODO Do context enrichment synchronously
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

apr_status_t
inputFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade, ap_input_mode_t pMode, apr_read_type_e pBlock, apr_off_t pReadbytes)
{
    apr_status_t lStatus = ap_get_brigade(pFilter->next, pBrigade, pMode, pBlock, pReadbytes);
    if (lStatus != APR_SUCCESS) {
        return lStatus;
    }
    return analyseRequest(pFilter, pBrigade);
}

struct RequestContext {

    RequestContext() : tmpbb(NULL), req(NULL) {
    }

    apr_bucket_brigade  *tmpbb;
    RequestInfo       *req;
};

void
prepareRequestInfo(unsigned int rId, DupConf *tConf, request_rec *pRequest, RequestInfo &r) {
    r.mId = rId;
    r.mPoison = false;
    r.mConfPath = tConf->dirName;
    r.mPath = pRequest->uri;
    r.mArgs = pRequest->args ? pRequest->args : "";
}

void
printRequest(request_rec *pRequest, RequestInfo *pBH, DupConf *tConf) {

    const char *reqId = apr_table_get(pRequest->headers_in, "UNIQUE_ID");
    Log::debug("### Pushing a request with ID: %s, body size:%s", reqId, boost::lexical_cast<std::string>(pBH->mBody.size()).c_str());
    Log::debug("### Uri:%s, dir name:%s", pRequest->uri, tConf->dirName);
    Log::debug("### Request args: %s", pRequest->args);
}

apr_status_t
outputFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade) {

    // Unique ID extraction
    request_rec *pRequest = pFilter->r;
    apr_table_t *headers = pRequest->headers_in;
    if (!headers)
        return ap_pass_brigade(pFilter->next, pBrigade);
    const char *reqId = apr_table_get(headers, "UNIQUE_ID");
    if (!reqId)
        return ap_pass_brigade(pFilter->next, pBrigade);
    unsigned int rId = boost::lexical_cast<unsigned int>(reqId);

    struct DupConf *tConf = reinterpret_cast<DupConf *>(ap_get_module_config(pRequest->per_dir_config, &dup_module));

    // Request answer analyse
    struct RequestContext *ctx;
    ctx = (RequestContext *)pFilter->ctx;
    if (ctx == NULL) {
        // Context init
        ctx = new RequestContext();
        pFilter->ctx = ctx;
        ctx->req = interFilterContext.getRequestInfo(rId);
        ctx->tmpbb = apr_brigade_create(pFilter->r->pool, pFilter->c->bucket_alloc);
    } else if (ctx == (void *) -1) {
        return ap_pass_brigade(pFilter->next, pBrigade);
    }

    if (DuplicationType::value != DuplicationType::REQUEST_WITH_ANSWER) {
        // Asynchronous push of request without the answer
        RequestInfo *rH = interFilterContext.getRequestInfo(rId);
        prepareRequestInfo(rId, tConf, pRequest, *rH);
        printRequest(pRequest, rH, tConf);
        gThreadPool->push(rH);
        interFilterContext.rmRequestInfo(rId);
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
            prepareRequestInfo(rId, tConf, pRequest, *(ctx->req));
            printRequest(pRequest, ctx->req, tConf);
            gThreadPool->push(ctx->req);
            interFilterContext.rmRequestInfo(rId);
            delete (RequestContext *)pFilter->ctx;
            pFilter->ctx = (void *) -1;
        }
        else {
            apr_brigade_cleanup(ctx->tmpbb);
        }
    }
    return OK;
}

};
