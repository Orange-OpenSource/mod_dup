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


namespace DupModule {

/*
 * Callback to iterate over the headers tables
 * Pushes a copy of key => value in a list passed without typing as the first argument
 */
static int iterateOverHeadersCallBack(void *d, const char *key, const char *value)
{
    RequestInfo::tHeaders *headers = reinterpret_cast<RequestInfo::tHeaders *>(d);
    headers->push_back(std::pair<std::string, std::string>(key, value));
    return 1;
}

static void
prepareRequestInfo(DupConf *tConf, request_rec *pRequest, RequestInfo &r)
{
    // Copy headers in
    apr_table_do(&iterateOverHeadersCallBack, &r.mHeadersIn, pRequest->headers_in, NULL);

    // Add the elapsed time header
    r.mHeadersOut.push_back(std::make_pair(std::string("ELAPSED_TIME_BY_DUP"), boost::lexical_cast<std::string>(r.getElapsedTimeMS())));

    // Basic
    r.mPoison = false;
    r.mConfPath = tConf->dirName;
    r.mPath = pRequest->uri;
    r.mArgs = pRequest->args ? pRequest->args : "";
}

static void
printRequest(request_rec *pRequest, RequestInfo *pBH, DupConf *tConf)
{
    const char *reqId = apr_table_get(pRequest->headers_in, CommonModule::c_UNIQUE_ID);
    Log::debug("### Pushing a request with ID: %s, body size:%ld", reqId, pBH->mBody.size());
    Log::debug("### Uri:%s, dir name:%s", pRequest->uri, tConf->dirName);
    Log::debug("### Request args: %s", pRequest->args);
}

apr_status_t
inputFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pB, ap_input_mode_t pMode, apr_read_type_e pBlock, apr_off_t pReadbytes)
{
    request_rec *pRequest = pFilter->r;
    Log::error(42, "inputFilterHandler");
    if (!pRequest || !pRequest->per_dir_config) {
        Log::error(42, "inputFilterHandler no request");
        return ap_get_brigade(pFilter->next, pB, pMode, pBlock, pReadbytes);
    }
    struct DupConf *conf = reinterpret_cast<DupConf *>(ap_get_module_config(pRequest->per_dir_config, &dup_module));
    if (!conf || !conf->dirName) {
        Log::error(42, "inputFilterHandler no conf");
        // Not a location that we treat, we decline the request
        return ap_get_brigade(pFilter->next, pB, pMode, pBlock, pReadbytes);
    }

    Log::error(42, "inputFilterHandler ctx %p", pFilter->ctx );

    RequestInfo *info;
    if (!pFilter->ctx) {

        boost::shared_ptr<RequestInfo> * reqInfo(reinterpret_cast<boost::shared_ptr<RequestInfo> *>(ap_get_module_config(pFilter->r->request_config, &dup_module)));
        if (!reqInfo || !reqInfo->get()) {
            // Unique request id
            std::string uid = CommonModule::getOrSetUniqueID(pRequest);
            info = new RequestInfo(uid);

            // Allocation on a shared pointer on the request pool
            // We guarantee that whatever happens, the RequestInfo will be deleted
            void *space = apr_palloc(pRequest->pool, sizeof(boost::shared_ptr<RequestInfo>));
            new (space) boost::shared_ptr<RequestInfo>(info);
            // Registering of the shared pointer destructor on the pool
            apr_pool_cleanup_register(pRequest->pool, space, cleaner<boost::shared_ptr<RequestInfo> >,
                                    apr_pool_cleanup_null);
            // Backup in request context
            ap_set_module_config(pRequest->request_config, &dup_module, (void *)space);
            // Backup in filter context
            pFilter->ctx = info;

            info->mConfPath = conf->dirName;
            info->mArgs = pRequest->args ? pRequest->args : "";

            Log::error(42, "inputFilterHandler new RequestInfo  %s?%s", info->mConfPath.c_str(), info->mArgs.c_str() );
        } else {
            Log::error(42, "inputFilterHandler reuse reqInfo" );
            info = reqInfo->get();
        }
    }
    if (pFilter->ctx != (void *) - 1) {
        // Request not read yet
        Log::error(42, "inputFilterHandler ctx not -1" );
        info = reinterpret_cast<RequestInfo *>(pFilter->ctx);
        apr_status_t st = ap_get_brigade(pFilter->next, pB, pMode, pBlock, pReadbytes);
        if (st != APR_SUCCESS) {
            pFilter->ctx = (void *) - 1;
            return st;
        }
        // Concats the brigade content to the reqinfo
        for (apr_bucket *b = APR_BRIGADE_FIRST(pB);
                b != APR_BRIGADE_SENTINEL(pB);
                b = APR_BUCKET_NEXT(b) ) {
            // Metadata end of stream
            Log::error(42, "inputFilterHandler bucket loop" );
            if (APR_BUCKET_IS_EOS(b)) {
                Log::error(42, "inputFilterHandler eos" );
                return APR_SUCCESS;
            }
            if (APR_BUCKET_IS_METADATA(b))
                continue;
            const char *data = 0;
            apr_size_t len = 0;
            apr_status_t rv = apr_bucket_read(b, &data, &len, APR_BLOCK_READ);
            if (rv != APR_SUCCESS) {
                Log::error(42, "Bucket read failed, skipping the rest of the body");
                return rv;
            }
            if (len) {
                info->mBody.append(data, len);
            }
        }
    }
    Log::error(42, "inputFilterHandler Received request %s?%s", info->mConfPath.c_str(), info->mArgs.c_str() );
    // Data is read
    return APR_SUCCESS;
}

/**
 * Output Body filter handler
 * Writes the response body to the RequestInfo
 * Unless not needed because we only duplicate request and no reponses
 */
apr_status_t
outputBodyFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade)
{
    Log::error(42, "outputBodyFilterHandler" );

    request_rec *pRequest = pFilter->r;
    apr_status_t rv;
    // Reject requests that do not meet our requirements
    if ((pFilter->ctx == (void *) - 1) || !pRequest || !pRequest->per_dir_config) {
        Log::error(42, "outputBodyFilterHandler ctx %p", pFilter->ctx );
        rv = ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return rv;
    }
    struct DupConf *tConf = reinterpret_cast<DupConf *>(ap_get_module_config(pRequest->per_dir_config, &dup_module));
    if ((!tConf) || (!tConf->dirName) || (tConf->getHighestDuplicationType() == DuplicationType::NONE)) {
        Log::error(42, "outputBodyFilterHandler no conf" );
        rv = ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return rv;
    }

    boost::shared_ptr<RequestInfo> * reqInfo(reinterpret_cast<boost::shared_ptr<RequestInfo> *>(ap_get_module_config(pFilter->r->request_config, &dup_module)));
    if (!reqInfo || !reqInfo->get()) {
        if (!pFilter->ctx) {

            // Unique request id
            std::string uid = CommonModule::getOrSetUniqueID(pRequest);
            RequestInfo * info = new RequestInfo(uid);

            // Allocation on a shared pointer on the request pool
            // We guarantee that whatever happens, the RequestInfo will be deleted
            void *space = apr_palloc(pRequest->pool, sizeof(boost::shared_ptr<RequestInfo>));
            reqInfo = new (space) boost::shared_ptr<RequestInfo>(info);
            // Registering of the shared pointer destructor on the pool
            apr_pool_cleanup_register(pRequest->pool, space, cleaner<boost::shared_ptr<RequestInfo> >,
                                      apr_pool_cleanup_null);
            // Backup in request context
            ap_set_module_config(pRequest->request_config, &dup_module, (void *)space);
            // Backup in filter context
            pFilter->ctx = info;

            info->mConfPath = tConf->dirName;
            info->mArgs = pRequest->args ? pRequest->args : "";

            Log::error(42, "inputFilterHandler new RequestInfo  %s?%s", info->mConfPath.c_str(), info->mArgs.c_str() );

        } else {

        Log::error(42, "outputBodyFilterHandler no reqInfo" );
        pFilter->ctx = (void *) - 1;
        rv = ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return rv;
        }
    }
    RequestInfo * ri = reqInfo->get();

    // Write the response body to the RequestInfo if found
    apr_bucket *currentBucket;
    for ( currentBucket = APR_BRIGADE_FIRST(pBrigade);
            currentBucket != APR_BRIGADE_SENTINEL(pBrigade);
            currentBucket = APR_BUCKET_NEXT(currentBucket) ) {

        Log::error(42, "outputBodyFilterHandler bucket loop" );

        if (APR_BUCKET_IS_EOS(currentBucket)) {
            ri->eos_seen(true);
            pFilter->ctx = (void *) - 1;
            rv = ap_pass_brigade(pFilter->next, pBrigade);
            apr_brigade_cleanup(pBrigade);
            Log::error(42, "outputBodyFilterHandler found EOS" );
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
                Log::error(42, "outputBodyFilterHandler append answer" );
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
apr_status_t
outputHeadersFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade)
{

    apr_status_t rv;
    if ( pFilter->ctx == (void *) - 1 ) {
        rv = ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return rv;
    }

    request_rec *pRequest = pFilter->r;
    // Reject requests that do not meet our requirements
    if (!pRequest || !pRequest->per_dir_config) {
        pFilter->ctx = (void *) - 1;
        rv = ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return rv;
    }

    struct DupConf *tConf = reinterpret_cast<DupConf *>(ap_get_module_config(pRequest->per_dir_config, &dup_module));
    if ((!tConf) || (!tConf->dirName) || (tConf->getHighestDuplicationType() == DuplicationType::NONE)) {
        pFilter->ctx = (void *) - 1;
        rv = ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return rv;
    }

    boost::shared_ptr<RequestInfo> * reqInfo(reinterpret_cast<boost::shared_ptr<RequestInfo> *>(ap_get_module_config(pFilter->r->request_config, &dup_module)));

    if (!reqInfo || !reqInfo->get()) {
        pFilter->ctx = (void *) - 1;
        rv = ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        Log::error(42, "outputHeadersFilterHandler reqInfo not found" );
        return rv;
    }
    RequestInfo *ri = reqInfo->get();

    // Copy headers out
    apr_table_do(&iterateOverHeadersCallBack, &ri->mHeadersOut, pRequest->headers_out, NULL);

    if (!ri->eos_seen()) {
        rv = ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        Log::error(42, "outputHeadersFilterHandler eos not seen yet" );
        return rv;
    }

    // Pushing the answer to the processor
    prepareRequestInfo(tConf, pRequest, *ri);

    if ( tConf->synchronous ) {
        static __thread CURL * lCurl = NULL;
        if ( ! lCurl ) {
            lCurl = gProcessor->initCurl();
        }
        Log::error(42, "outputHeadersFilterHandler run one in sync" );
        gProcessor->runOne(*ri, lCurl);
    }
    else {
        Log::error(42, "outputHeadersFilterHandler push to queue" );
        gThreadPool->push(*reqInfo);
    }
    pFilter->ctx = (void *) - 1;
    printRequest(pRequest, ri, tConf);
    rv = ap_pass_brigade(pFilter->next, pBrigade);
    apr_brigade_cleanup(pBrigade);
    return rv;
}


};
