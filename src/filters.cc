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

struct BodyHandler {
    BodyHandler() : body() {}
    std::string body;
};

apr_status_t
analyseRequest(ap_filter_t *pF, apr_bucket_brigade *pB ) {
    request_rec *pRequest = pF->r;
    if (pRequest) {
	struct DupConf *tConf = reinterpret_cast<DupConf *>(ap_get_module_config(pRequest->per_dir_config, &dup_module));
	if (!tConf) {
            return OK;
	}
        // Do we have a context?
        if (!pF->ctx) {
            pF->ctx = new BodyHandler();
        } else if (pF->ctx == (void *)1) {
            return OK;
        }
        BodyHandler *pBH = static_cast<BodyHandler *>(pF->ctx);
        // Body is stored only if the payload flag is activated
        for (apr_bucket *b = APR_BRIGADE_FIRST(pB);
             b != APR_BRIGADE_SENTINEL(pB);
             b = APR_BUCKET_NEXT(b) ) {
#ifndef UNIT_TESTING
            // Metadata end of stream
            if ( APR_BUCKET_IS_EOS(b) ) {
#endif

                Log::debug("### Pushing a request, body size:%s", boost::lexical_cast<std::string>(pBH->body.size()).c_str());
                Log::debug("### Uri:%s, dir name:%s", pRequest->uri, tConf->dirName);

                // TODO Do context enrichment synchronously

                apr_table_t *headersIn = pRequest->headers_in;
                volatile unsigned int rId = tConf->getNextReqId();
                rId = tConf->getNextReqId();
                std::string reqId = boost::lexical_cast<std::string>(rId);
                apr_table_set(headersIn, "request_id", reqId.c_str());
                //apr_table_set(pRequest->headers_out, "request_id", reqId.c_str());

                // Asynchronous push
                gThreadPool->push(RequestInfo(rId, tConf->dirName,
                                              pRequest->uri, pRequest->args ? pRequest->args : "", &pBH->body));

                delete pBH;
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
            pBH->body += std::string(lReqPart, lLength);
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
    apr_bucket_brigade *tmpbb;
    int filter_state;
    std::string answer;
};

apr_status_t
outputFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade) {
    if (DuplicationType::value != DuplicationType::REQUEST_WITH_ANSWER) {
        Log::debug("Conf error, do not setOutputFilter with duplicatinType != REQUEST_WITH_ANSWER");
        return ap_pass_brigade(pFilter->next, pBrigade);
    }
    struct RequestContext *ctx;
    ctx = (RequestContext *)pFilter->ctx;
    if (ctx == NULL) {
    // Context init
        ctx = (RequestContext *)apr_palloc(pFilter->r->pool, sizeof(*ctx));
        ctx = new (ctx) RequestContext();
        pFilter->ctx = ctx;
        ctx->tmpbb = apr_brigade_create(pFilter->r->pool, pFilter->c->bucket_alloc);
        ctx->filter_state = 1;
    } else if (ctx == (void *) -1) {
        return ap_pass_brigade(pFilter->next, pBrigade);
    }

    request_rec *pRequest = pFilter->r;
    apr_table_t *headers = pRequest->headers_in;
    const char *reqId = apr_table_get(headers, "request_id");
    unsigned int rId = boost::lexical_cast<unsigned int>(reqId);

    apr_bucket *currentBucket;
    while ((currentBucket = APR_BRIGADE_FIRST(pBrigade)) != APR_BRIGADE_SENTINEL(pBrigade)) {
        const char *data;
        apr_size_t len;
        apr_status_t rv;
        rv = apr_bucket_read(currentBucket, &data, &len, APR_BLOCK_READ);

        if ((rv == APR_SUCCESS) && (data != NULL)) {
            ctx->answer.append(data, len);
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
            AnswerHolder *ans = gProcessor->getAnswer(rId);
            ans->m_body = ctx->answer;
            ans->m_sync.unlock();
            ctx->~RequestContext();
            pFilter->ctx = (void *) -1;
        }
        else {
            apr_brigade_cleanup(ctx->tmpbb);
        }
    }
    return OK;
}

};
