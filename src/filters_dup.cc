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
#include "Utils.hh"

#include <boost/shared_ptr.hpp>
#include <http_config.h>
#include <algorithm>

namespace DupModule {

/*
 * Callback to iterate over the headers tables
 * Pushes a copy of key => value in a list passed without typing as the first argument
 */
static int iterateOverHeadersCallBack(void *d, const char *key, const char *value)
{
    tKeyValList *headers = reinterpret_cast<tKeyValList *>(d);
    headers->push_back(std::pair<std::string, std::string>(key, value));
    return 1;
}

static int checkAdditionalHeaders(const RequestInfo &r, request_rec *pRequest)
{
    if (not r.mValidationHeaderDup && not r.mValidationHeaderComp){
        return 0;
    }

    for (tKeyValList::value_type header : r.mHeadersOut) {
        if (header.first == std::string("X_DUP_LOG")) {
            apr_table_set(pRequest->headers_out,"X_DUP_LOG", header.second.c_str());
        }
        else if (header.first == std::string("X_COMP_LOG")) {
            apr_table_set(pRequest->headers_out,"X_COMP_LOG", header.second.c_str());
        }
    }

    return 0;
}

static int checkCurlResponseCompareStatus(const RequestInfo &ri, request_rec *pRequest)
{
    if( not apr_table_get(pRequest->headers_in, "X_DUP_LOG"))
    {
        return 0;
    }
    if (ri.mCurlCompResponseHeader.count("X-COMP-STATUS")) {
        Log::debug("[DEBUG][DUP] header contains the X-COMP-STATUS");
        apr_table_set( pRequest->headers_out,"X-COMP-STATUS", ri.mCurlCompResponseHeader.at("X-COMP-STATUS").c_str());
        return 0;
    }
    if (ri.mCurlCompResponseStatus != CURLE_OK){
        Log::debug("[DEBUG][DUP] Curl error happened the destination is not reached");
        std::string out("NOT REACHED - curl status ");
        out += std::to_string(ri.mCurlCompResponseStatus);
        apr_table_set( pRequest->headers_out,"X-COMPARE-STATUS", out.c_str());
        return 0;
    }

    Log::debug("[DEBUG][DUP] Curl returned OK but there was no comparison.");
    apr_table_set( pRequest->headers_out,"X-COMPARE-STATUS", "REACHED DESTINATION - NO COMPARISON");
    return 0;
}

static bool prepareRequestInfo(DupConf *tConf, request_rec *pRequest, RequestInfo &r)
{
    Log::debug("[DUP] Pepare request info");
    // Add the HTTP Status Code Header
    r.mHeadersIn.push_front(std::make_pair("X_DUP_HTTP_STATUS", boost::lexical_cast<std::string>(pRequest->status)));
    // Add the HTTP Request Method
    r.mHeadersIn.push_front(std::make_pair("X_DUP_METHOD", pRequest->method));
    // Add the HTTP Content Type
    const char* contentType = apr_table_get(pRequest->headers_in,"Content-Type");
    if (contentType) r.mHeadersIn.push_front(std::make_pair("X_DUP_CONTENT_TYPE", contentType));

    // Increment the dup count and make sure we didn't duplicate more than 4 times
    // avoids an infinite loop of duplication when destination is localhost or a loop in the network
    const char* dupCount = apr_table_get(pRequest->headers_in,"X-DUP-COUNT");
    int count = 0;
    if (dupCount) {
        count = atoi(dupCount);
        if ( count >= 4 ) {
            return false;
        }
    }
    count++;
    r.mHeadersIn.push_front(std::make_pair("X-DUP-COUNT", std::to_string(count).c_str()));
    
    // Copy headers in, we might have duplicate headers in case of double dup but we'll deal with it later
    apr_table_do(&iterateOverHeadersCallBack, &r.mHeadersIn, pRequest->headers_in, NULL);

    // Check if X_DUP_LOG header is present
    if (apr_table_get(pRequest->headers_in, "X_DUP_LOG")) r.mValidationHeaderDup = true;

    // Basic
    r.mPoison = false;
    r.mConf = tConf;
    r.mPath = pRequest->uri;
    r.mArgs = pRequest->args ? pRequest->args : "";
    return true;
}

static void printRequest(request_rec *pRequest, RequestInfo *pBH, DupConf *tConf)
{
    const char *reqId = apr_table_get(pRequest->headers_in, CommonModule::c_UNIQUE_ID);
    Log::debug("[DUP] Pushing a request with ID: %s, body size:%ld", reqId, pBH->mBody.size());
    Log::debug("[DUP] Uri:%s, dir name:%s", pRequest->uri, tConf->dirName);
    Log::debug("[DUP] Request args: %s", pRequest->args);
}

/**
 * @brief Duplicate this request, async or sync operation
 */
static void initiateDuplication(DupConf *tConf, request_rec *pRequest, boost::shared_ptr<RequestInfo> * reqInfo)
{
    RequestInfo * ri = reqInfo->get();
    // Pushing the answer to the processor
    if ( !  prepareRequestInfo(tConf, pRequest, *ri) ) {
        return;
    }

    // Force synchronous mode when X_DUP_LOG to retrieve X_DUP_LOG header
    if (tConf->synchronous || apr_table_get(pRequest->headers_in, "X_DUP_LOG")) {
        static __thread CURL * lCurl = NULL;
        if (!lCurl) {
            lCurl = gProcessor->initCurl();
        }
        // Run synchronously without pushing to the queue
        bool stillRunning = true;
        gProcessor->runOne(*ri, lCurl, stillRunning);
    } else {
        // will be popped by RequestProcessor::Run
        gThreadPool->push(*reqInfo);
    }
    checkAdditionalHeaders(*ri, pRequest);
    checkCurlResponseCompareStatus(*ri, pRequest);
    printRequest(pRequest, ri, tConf);
}

apr_status_t inputFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pB, ap_input_mode_t pMode, apr_read_type_e pBlock, apr_off_t pReadbytes)
{
    Log::debug("[DUP] Input filter handler");
    request_rec *pRequest = pFilter->r;
    if (!pRequest || !pRequest->per_dir_config) {
        return ap_get_brigade(pFilter->next, pB, pMode, pBlock, pReadbytes);
    }
    struct DupConf *conf = reinterpret_cast<DupConf *>(ap_get_module_config(pRequest->per_dir_config, &dup_module));
    if (!conf || !conf->dirName) {
        // Not a location that we treat, we decline the request
        return ap_get_brigade(pFilter->next, pB, pMode, pBlock, pReadbytes);
    }

    if (!pFilter->ctx) {
        boost::shared_ptr<RequestInfo> * reqInfo(reinterpret_cast<boost::shared_ptr<RequestInfo> *>(ap_get_module_config(pFilter->r->request_config, &dup_module)));
        if (!reqInfo || !reqInfo->get()) {
            reqInfo = CommonModule::makeRequestInfo<RequestInfo, &dup_module>(pRequest);
            RequestInfo *info = reqInfo->get();
 
            info->mConf = conf;
            info->mArgs = pRequest->args ? pRequest->args : "";
        }
        pFilter->ctx = reqInfo->get();
    }
    if (pFilter->ctx != (void *) -1) {
        // Request not completely read yet
        RequestInfo *info = reinterpret_cast<RequestInfo *>(pFilter->ctx);
        assert(info);
        apr_status_t st = ap_get_brigade(pFilter->next, pB, pMode, pBlock, pReadbytes);
        if (st != APR_SUCCESS) {
            pFilter->ctx = (void *) -1;
            return st;
        }
        // Concats the brigade content to the reqinfo
        for (apr_bucket *b = APR_BRIGADE_FIRST(pB); b != APR_BRIGADE_SENTINEL(pB); b = APR_BUCKET_NEXT(b)) {
            // Metadata end of stream
            if (APR_BUCKET_IS_EOS(b)) {
                return APR_SUCCESS;
            }
            if (APR_BUCKET_IS_METADATA(b))
                continue;
            const char *data = 0;
            apr_size_t len = 0;
            apr_status_t rv = apr_bucket_read(b, &data, &len, APR_BLOCK_READ);
            if (rv != APR_SUCCESS) {
                Log::error(42, "[DUP] Bucket read failed, skipping the rest of the body");
                return rv;
            }
            if (len) {
                info->mBody.append(data, len);
            }
        }
    }
    // Data is read
    return APR_SUCCESS;
}

/**
 * Output Body filter handler
 * Writes the response body to the RequestInfo
 * Unless not needed because we only duplicate request and no reponses
 */
apr_status_t outputBodyFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade)
{
    Log::debug("[DUP] Output body filter handler");

    request_rec *pRequest = pFilter->r;
    apr_status_t rv;
    // Reject requests that do not meet our requirements
    if ((pFilter->ctx == (void *) -1) || !pRequest || !pRequest->per_dir_config) {
        rv = ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return rv;
    }
    struct DupConf *tConf = reinterpret_cast<DupConf *>(ap_get_module_config(pRequest->per_dir_config, &dup_module));
    if ((!tConf) || (!tConf->dirName) || (tConf->getHighestDuplicationType() == DuplicationType::NONE)) {
        rv = ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return rv;
    }

    RequestInfo * ri = NULL;
    boost::shared_ptr<RequestInfo> * reqInfo(reinterpret_cast<boost::shared_ptr<RequestInfo> *>(ap_get_module_config(pFilter->r->request_config, &dup_module)));
    if (!reqInfo || !reqInfo->get()) {
        if (!pFilter->ctx) {
            // When the body of the response is large, and there is no request body (i.e. GET)
            // for some unknown reason
            // apache calls the output filter before the input filter
            // so we need to handle this gracefully
            // by creating the RequestInfo
            reqInfo = CommonModule::makeRequestInfo<DupModule::RequestInfo,&dup_module>(pRequest);
			ri = reqInfo->get();
            pFilter->ctx = ri;

            ri->mConf = tConf;
            ri->mArgs = pRequest->args ? pRequest->args : "";

        } else {
            pFilter->ctx = (void *) -1;
            rv = ap_pass_brigade(pFilter->next, pBrigade);
            apr_brigade_cleanup(pBrigade);
            return rv;
        }
    } else {
        ri = reqInfo->get();
    }

    // Write the response body to the RequestInfo if found
    apr_bucket *currentBucket;
    for (currentBucket = APR_BRIGADE_FIRST(pBrigade); currentBucket != APR_BRIGADE_SENTINEL(pBrigade); currentBucket = APR_BUCKET_NEXT(currentBucket)) {

        if (APR_BUCKET_IS_EOS(currentBucket)) {
            ri->eos_seen(true);
            if (apr_table_get(pRequest->headers_in, "X_DUP_LOG")) {
                apr_table_do(&iterateOverHeadersCallBack, &ri->mHeadersOut, pRequest->headers_out, NULL);
                initiateDuplication(tConf, pRequest, reqInfo);
            }
            pFilter->ctx = (void *) -1;
            rv = ap_pass_brigade(pFilter->next, pBrigade);
            apr_brigade_cleanup(pBrigade);
            return rv;
        }

        if (APR_BUCKET_IS_METADATA(currentBucket))
            continue;

        // We need to get the highest one as we haven't matched which rule it is yet
        if (tConf->getHighestDuplicationType() == DuplicationType::REQUEST_WITH_ANSWER) {

            const char *data;
            apr_size_t len;
            rv = apr_bucket_read(currentBucket, &data, &len, APR_BLOCK_READ);

            if ((rv == APR_SUCCESS) && data) {
                // Appends the part read to the answer
                ri->mAnswer.append(data, len);
            }
        }
    }

    rv = ap_pass_brigade(pFilter->next, pBrigade);
    apr_brigade_cleanup(pBrigade);
    return rv;
}

/**
 * Output filter handler
 * Retrieves in/out headers
 * Pushes the RequestInfo object to the RequestProcessor
 */
apr_status_t outputHeadersFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade)
{
    Log::debug("[DUP] Output headers filter handler");

    apr_status_t rv;
    if (pFilter->ctx == (void *) -1) {
        rv = ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return rv;
    }

    request_rec *pRequest = pFilter->r;
    // Reject requests that do not meet our requirements
    if (!pRequest || !pRequest->per_dir_config) {
        pFilter->ctx = (void *) -1;
        rv = ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return rv;
    }

    struct DupConf *tConf = reinterpret_cast<DupConf *>(ap_get_module_config(pRequest->per_dir_config, &dup_module));
    if ((!tConf) || (!tConf->dirName) || (tConf->getHighestDuplicationType() == DuplicationType::NONE)) {
        pFilter->ctx = (void *) -1;
        rv = ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return rv;
    }

    RequestInfo * ri = NULL;
    boost::shared_ptr<RequestInfo> * reqInfo(reinterpret_cast<boost::shared_ptr<RequestInfo> *>(ap_get_module_config(pFilter->r->request_config, &dup_module)));

    if (!reqInfo || !reqInfo->get()) {
        if (!pFilter->ctx) {
            // Allocation on a shared pointer on the request pool
            // We guarantee that whatever happens, the RequestInfo will be deleted
            // Registering of the shared pointer destructor on the pool
            // Backup in request context
            // Backup in filter context
            reqInfo = CommonModule::makeRequestInfo<DupModule::RequestInfo,&dup_module>(pRequest);
            ri = reqInfo->get();
            pFilter->ctx = ri;

            ri->mConf = tConf;
            ri->mArgs = pRequest->args ? pRequest->args : "";
        } else {
            pFilter->ctx = (void *) -1;
            rv = ap_pass_brigade(pFilter->next, pBrigade);
            apr_brigade_cleanup(pBrigade);
            return rv;
        }
    } else {
        ri = reqInfo->get();
    }

    apr_table_do(&iterateOverHeadersCallBack, &ri->mHeadersOut, pRequest->headers_out, NULL);

    if (!ri->eos_seen()) {
        rv = ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return rv;
    }

    // Normal case, duplicate when the output headers are processed
    // which seems late if we don't care about the response
    if (!apr_table_get(pRequest->headers_in, "X_DUP_LOG")) {
        initiateDuplication(tConf, pRequest, reqInfo);
    }

    pFilter->ctx = (void *) -1;
    rv = ap_pass_brigade(pFilter->next, pBrigade);
    apr_brigade_cleanup(pBrigade);
    return rv;
}

}
