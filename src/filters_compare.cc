/*
* mod_compare - compare apache requests
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

#include "mod_compare.hh"
#include "RequestInfo.hh"

#include <http_config.h>
#include <assert.h>
#include <stdexcept>

const char *c_UNIQUE_ID = "UNIQUE_ID";

namespace CompareModule {


static void
printRequest(request_rec *pRequest, std::string pBody, CompareConf *tConf) {
    const char *reqId = apr_table_get(pRequest->headers_in, c_UNIQUE_ID);
    Log::debug("### Pushing a request with ID: %s, body size:%ld", reqId, pBody.size());
    Log::debug("### Uri:%s", pRequest->uri);
    Log::debug("### Request args: %s", pRequest->args);
}

/**
 * @brief extract the request body, the header answer and the response answer
 * @param pReqInfo object containing the request infos
 * @param lReqBody body of the request
 * @return a http status
 */
static apr_status_t serializeBody(DupModule::RequestInfo &pReqInfo, std::string &lReqBody)
{
    int BAD_REQUEST = 400;
    size_t lBodyReqSize, lHeaderResSize, lBodyResSize;

    try
    {
        lBodyReqSize =   boost::lexical_cast<unsigned int>( pReqInfo.mBody.substr(0,8));
        lHeaderResSize = boost::lexical_cast<unsigned int>( pReqInfo.mBody.substr(8+lBodyReqSize,8));
        lBodyResSize = boost::lexical_cast<unsigned int>( pReqInfo.mBody.substr(16 + lBodyReqSize + lHeaderResSize,8));

        lReqBody = pReqInfo.mBody.substr(8,lBodyReqSize);
        pReqInfo.mResponseHeader = pReqInfo.mBody.substr(16 + lBodyReqSize,lHeaderResSize);
        pReqInfo.mResponseBody = pReqInfo.mBody.substr(24 + lBodyReqSize +  lHeaderResSize, lBodyResSize);
    }
    catch (boost::bad_lexical_cast &)
    {
        Log::error(12, "Invalid size value");
        return BAD_REQUEST;
    }
    catch ( const std::out_of_range &oor)
    {
        Log::error(13, "Out of range error: %s", oor.what());
        return BAD_REQUEST;
    }

    return OK;

}

apr_status_t
inputFilterHandler(ap_filter_t *pF, apr_bucket_brigade *pB, ap_input_mode_t pMode, apr_read_type_e pBlock, apr_off_t pReadbytes)
{
    apr_status_t lStatus = ap_get_brigade(pF->next, pB, pMode, pBlock, pReadbytes);
    if (lStatus != APR_SUCCESS) {
        return lStatus;
    }
    request_rec *pRequest = pF->r;
    const char *lDupType = apr_table_get(pRequest->headers_in, "Duplication-Type");
    if (strcmp("answer", lDupType) != 0)
    {
        return DECLINED;
    }

    if (pRequest) {
        struct CompareConf *tConf = reinterpret_cast<CompareConf *>(ap_get_module_config(pRequest->per_dir_config, &compare_module));
        if (!tConf) {
                return DECLINED; // SHOULD NOT HAPPEN
        }
        // No context? new request
        if (!pF->ctx) {
            DupModule::RequestInfo *info = new DupModule::RequestInfo(tConf->getNextReqId());
            ap_set_module_config(pRequest->request_config, &compare_module, (void *)info);
            // Copy Request ID in both headers
            std::string reqId = boost::lexical_cast<std::string>(info->mId);
            apr_table_set(pRequest->headers_in, c_UNIQUE_ID, reqId.c_str());
            apr_table_set(pRequest->headers_out, c_UNIQUE_ID, reqId.c_str());
            // Backup of info struct in the request context
            pF->ctx = info;
        } else if (pF->ctx == (void *)1) {
            return OK;
        }

        DupModule::RequestInfo *pBH = static_cast<DupModule::RequestInfo *>(pF->ctx);
        for (apr_bucket *b = APR_BRIGADE_FIRST(pB);
             b != APR_BRIGADE_SENTINEL(pB);
             b = APR_BUCKET_NEXT(b) ) {
            // Metadata end of stream
            if ( APR_BUCKET_IS_EOS(b) ) {
                pF->ctx = (void *)1;
                break;
            }
            const char* lReqPart = NULL;
            apr_size_t lLength = 0;
            apr_status_t lStatus = apr_bucket_read(b, &lReqPart, &lLength, APR_BLOCK_READ);
            if ((lStatus != APR_SUCCESS) || (lReqPart == NULL)) {
                continue;
            }
            pBH->mBody += std::string(lReqPart, lLength);
        }
        apr_brigade_cleanup(pB);

        std::string lReqBody;
        apr_status_t lStatus =  serializeBody(*pBH, lReqBody);
        std::stringstream lStringSize;
        lStringSize << lReqBody.size();
        apr_table_set(pRequest->headers_in, "Content-Length", lStringSize.str().c_str());
        apr_brigade_write(pB, ap_filter_flush, pF, lReqBody.c_str(), lReqBody.length() );

        printRequest(pRequest, lReqBody, tConf);

        return lStatus;
    }
    return DECLINED;
}

/*
 * Request context used during brigade run
 */
class RequestContext {
public:
    apr_bucket_brigade  *tmpbb;
    DupModule::RequestInfo         *req; /** Req is pushed to requestprocessor for duplication and will be deleted later*/

    RequestContext(ap_filter_t *pFilter) {
        tmpbb = apr_brigade_create(pFilter->r->pool, pFilter->c->bucket_alloc);
        req = reinterpret_cast<DupModule::RequestInfo *>(ap_get_module_config(pFilter->r->request_config, &compare_module));
        assert(req);
    }

    ~RequestContext() {
        apr_brigade_cleanup(tmpbb);
    }
};

/*
 * Callback to iterate over the headers tables
 * Pushes a copy of key => value in a list
 */
/*static int iterateOverHeadersCallBack(void *d, const char *key, const char *value) {
    DupModule::RequestInfo::tHeaders *headers = reinterpret_cast<DupModule::RequestInfo::tHeaders *>(d);
    headers->push_back(std::pair<std::string, std::string>(key, value));
    return 0;
}*/

/*static void
prepareRequestInfo(CompareConf *tConf, request_rec *pRequest, DupModule::RequestInfo &r, bool withAnswer) {
    // Basic
    r.mPoison = false;
    r.mPath = pRequest->uri;
    r.mArgs = pRequest->args ? pRequest->args : "";

    // Copy headers in
    apr_table_do(&iterateOverHeadersCallBack, &r.mHeadersIn, pRequest->headers_in, NULL);
    if (withAnswer) {
        // Copy headers out
        apr_table_do(&iterateOverHeadersCallBack, &r.mHeadersOut, pRequest->headers_out, NULL);
    }
}*/

/*apr_status_t
outputFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade) {
    request_rec *pRequest = pFilter->r;
    if (!pRequest || !pRequest->per_dir_config)
        return ap_pass_brigade(pFilter->next, pBrigade);
    struct CompareConf *tConf = reinterpret_cast<CompareConf *>(ap_get_module_config(pRequest->per_dir_config, &compare_module));
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
        DupModule::RequestInfo *rH = ctx->req;
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
            ctx->req->mAnswer.append(data, len);
        }
        // Remove bucket e from bb.
        APR_BUCKET_REMOVE(currentBucket);
        // Insert it into  temporary brigade.
        APR_BRIGADE_INSERT_HEAD(ctx->tmpbb, currentBucket);
        // Pass brigade downstream.
        rv = ap_pass_brigade(pFilter->next, ctx->tmpbb);
        // TODO if (rv) ...;
        if (APR_BUCKET_IS_EOS(currentBucket)) {
            apr_brigade_cleanup(ctx->tmpbb);
            // Pushing the answer to the processor
            prepareRequestInfo(tConf, pRequest, *(ctx->req), true);
            printRequest(pRequest, ctx->req, tConf);
            gThreadPool->push(ctx->req);
            delete ctx;
            pFilter->ctx = (void *) -1;
        }
        else {
            apr_brigade_cleanup(ctx->tmpbb);
        }
    }
    return OK;
}*/

};
