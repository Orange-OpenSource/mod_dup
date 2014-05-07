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
#include "Utils.hh"

#include <http_config.h>
#include <assert.h>
#include <stdexcept>
#include <boost/thread/detail/singleton.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/tokenizer.hpp>
#include <iomanip>
#include <apache2/httpd.h>



namespace CompareModule {

void
printRequest(request_rec *pRequest, std::string pBody)
{
    const char *reqId = apr_table_get(pRequest->headers_in, DupModule::c_UNIQUE_ID);
    Log::debug("### Filtering a request with ID: %s, body size:%ld", reqId, pBody.size());
    Log::debug("### Uri:%s", pRequest->uri);
    Log::debug("### Request args: %s", pRequest->args);
}


/*
 * Callback to iterate over the headers tables
 * Pushes a copy of key => value in a list
 */
int iterateOverHeadersCallBack(void *d, const char *key, const char *value) {
    std::map< std::string, std::string> *lHeader = reinterpret_cast< std::map< std::string, std::string> *>(d);

    (*lHeader)[std::string(key)] = std::string(value);

    return 1;
}


apr_status_t inputFilterHandler(ap_filter_t *pF, apr_bucket_brigade *pB, ap_input_mode_t pMode, apr_read_type_e pBlock, apr_off_t pReadbytes)
{
    request_rec *pRequest = pF->r;
    if (!pRequest) {
        return ap_get_brigade(pF->next, pB, pMode, pBlock, pReadbytes);
    }

    const char *lDupType = apr_table_get(pRequest->headers_in, "Duplication-Type");
    if (( lDupType == NULL ) || ( strcmp("Response", lDupType) != 0) ) {
        return ap_get_brigade(pF->next, pB, pMode, pBlock, pReadbytes);
    }

    if(pRequest->per_dir_config == NULL){
        return ap_get_brigade(pF->next, pB, pMode, pBlock, pReadbytes);
    }
    struct CompareConf *tConf = reinterpret_cast<CompareConf *>(ap_get_module_config(pRequest->per_dir_config, &compare_module));
    if (!tConf) {
        return ap_get_brigade(pF->next, pB, pMode, pBlock, pReadbytes); // SHOULD NOT HAPPEN
    }
    // No context? new request
    if (!pF->ctx) {
        // If there is no UNIQUE_ID in the request header copy thr Request ID generated in both headers
        const char* lID = apr_table_get(pRequest->headers_in, DupModule::c_UNIQUE_ID);
        unsigned int lReqID;
        DupModule::RequestInfo *info;
        if( lID == NULL){
        lReqID = DupModule::getNextReqId();
        std::string reqId = boost::lexical_cast<std::string>(lReqID);
        apr_table_set(pRequest->headers_in, DupModule::c_UNIQUE_ID, reqId.c_str());
        apr_table_set(pRequest->headers_out, DupModule::c_UNIQUE_ID, reqId.c_str());
        info = new DupModule::RequestInfo(reqId);
        }
        else {
            info = new DupModule::RequestInfo(std::string(lID));
            apr_table_set(pRequest->headers_out, DupModule::c_UNIQUE_ID, lID);
        }

        // Allocation on a shared pointer on the request pool
        // We guarantee that whatever happens, the RequestInfo will be deleted
        void *space = apr_palloc(pRequest->pool, sizeof(boost::shared_ptr<DupModule::RequestInfo>));
        new (space) boost::shared_ptr<DupModule::RequestInfo>(info);
        // Registering of the shared pointer destructor on the pool
        apr_pool_cleanup_register(pRequest->pool, space, cleaner<boost::shared_ptr<DupModule::RequestInfo>>,
                                  apr_pool_cleanup_null);
        // Backup in request context
        ap_set_module_config(pRequest->request_config, &compare_module, (void *)space);

        // Backup of info struct in the request context
        pF->ctx = info;

        DupModule::RequestInfo *lRI = static_cast<DupModule::RequestInfo *>(pF->ctx);
        while (!DupModule::extractBrigadeContent(pB, pF->next, lRI->mBody)){
            apr_brigade_cleanup(pB);
        }
        pF->ctx = (void *)1;
    }

    // Everything is read, returning DECLINED to call the subrequest
    return DECLINED;
}

apr_status_t inputFilterSubReq(ap_filter_t *pF, apr_bucket_brigade *pB, ap_input_mode_t pMode, apr_read_type_e pBlock, apr_off_t pReadbytes)
{
    apr_status_t lStatus;
    request_rec *pRequest = pF->r->main;
    if (!pRequest) {
        return ap_get_brigade(pF->next, pB, pMode, pBlock, pReadbytes);
    }

    const char *lDupType = apr_table_get(pRequest->headers_in, "Duplication-Type");
    if (( lDupType == NULL ) || ( strcmp("Response", lDupType) != 0) ) {
        return ap_get_brigade(pF->next, pB, pMode, pBlock, pReadbytes);
    }

    if(pRequest->per_dir_config == NULL){
        return ap_get_brigade(pF->next, pB, pMode, pBlock, pReadbytes);
    }

    // No context? new request
    if (!pF->ctx) {
        boost::shared_ptr<DupModule::RequestInfo> *shPtr(reinterpret_cast<boost::shared_ptr<DupModule::RequestInfo> *>(ap_get_module_config(pRequest->request_config, &compare_module)));

        if (!shPtr || !shPtr->get()) {
            pF->ctx = (void *) -1;
            lStatus =  ap_pass_brigade(pF->next, pB);
            apr_brigade_cleanup(pB);
            return lStatus;
        }
        DupModule::RequestInfo *lRI = shPtr->get();

        lStatus =  deserializeBody(*lRI);
        if(lStatus != APR_SUCCESS){
            return lStatus;
        }

        pF->ctx = (void *)1;
        apr_brigade_cleanup(pB);
        lRI->offset = 0;

#ifndef UNIT_TESTING
        apr_table_set(pRequest->headers_in, "Content-Length",boost::lexical_cast<std::string>(lRI->mReqBody.size()).c_str());
        apr_table_do(&iterateOverHeadersCallBack, &(lRI->mReqHeader), pRequest->headers_in, NULL);
#endif
        printRequest(pRequest, lRI->mReqBody);
    }
    if (pF->ctx == (void *)1) {
        // Request is already read and deserialized, sending it to the client
        boost::shared_ptr<DupModule::RequestInfo> * reqInfo(reinterpret_cast<boost::shared_ptr<DupModule::RequestInfo> *>(ap_get_module_config(pF->r->request_config,
                                                                                                                         &compare_module)));
        DupModule::RequestInfo *lRI = reqInfo->get();
        std::string &lBodyToSend = lRI->mReqBody;

        int toSend = std::min((lBodyToSend.size() - lRI->offset), (size_t)pReadbytes);
        if (toSend > 0){
            apr_status_t st;
            if ((st = apr_brigade_write(pB, NULL, NULL, lBodyToSend.c_str() + lRI->offset, toSend)) != APR_SUCCESS ) {
                Log::warn(1, "Failed to write request body in a brigade");
                return st;
            }
            lRI->offset += toSend;
            return APR_SUCCESS;
        } else {
            return ap_get_brigade(pF->next, pB, pMode, pBlock, pReadbytes);
        }
    }
    // Everything is read and rewritten, simply returning a get brigade call
    return ap_get_brigade(pF->next, pB, pMode, pBlock, pReadbytes);
}


apr_status_t
outputFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade) {
    request_rec *pRequest = pFilter->r->main;
    apr_status_t lStatus;
    if (pFilter->ctx == (void *)-1){
        lStatus =  ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return lStatus;
    }

    // Truncate the log before writing if the URI is set to "comp_truncate"
    std::string lArgs( static_cast<const char *>(pRequest->uri) );
    if ( lArgs.find("comp_truncate") != std::string::npos){
        gFile.close();
        gFile.open(gFilePath, std::ofstream::out | std::ofstream::trunc );
        pFilter->ctx = (void *) -1;
        lStatus = ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return lStatus;
    }

    if (!pRequest || !pRequest->per_dir_config) {
        pFilter->ctx = (void *) -1;
        lStatus =  ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return lStatus;
    }

    const char *lDupType = apr_table_get(pRequest->headers_in, "Duplication-Type");
    if ( ( lDupType == NULL ) || ( strcmp("Response", lDupType) != 0) ) {
        pFilter->ctx = (void *) -1;
        lStatus = ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return lStatus;
    }

    struct CompareConf *tConf = reinterpret_cast<CompareConf *>(ap_get_module_config(pRequest->per_dir_config, &compare_module));
    if( tConf == NULL ){
        pFilter->ctx = (void *) -1;
        lStatus =  ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return lStatus;
    }

    boost::shared_ptr<DupModule::RequestInfo> *shPtr(reinterpret_cast<boost::shared_ptr<DupModule::RequestInfo> *>(ap_get_module_config(pRequest->request_config, &compare_module)));

    if (!shPtr || !shPtr->get()) {
        pFilter->ctx = (void *) -1;
        lStatus =  ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return lStatus;
    }
    DupModule::RequestInfo *req = shPtr->get();

    apr_bucket *currentBucket;
    apr_status_t rv;
    for ( currentBucket = APR_BRIGADE_FIRST(pBrigade);
          currentBucket != APR_BRIGADE_SENTINEL(pBrigade);
          currentBucket = APR_BUCKET_NEXT(currentBucket) ) {
        const char *data;
        apr_size_t len;

        if (APR_BUCKET_IS_EOS(currentBucket)) {
            req->eos_seen = 1;
            continue;
        }

        rv = apr_bucket_read(currentBucket, &data, &len, APR_BLOCK_READ);

        if ((rv == APR_SUCCESS) && (data != NULL)) {
            req->mDupResponseBody.append(data, len);
        }
    }

    rv = ap_pass_brigade(pFilter->next, pBrigade);
    apr_brigade_cleanup(pBrigade);
    return rv;
}


apr_status_t
outputFilterHandler2(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade) {

    apr_status_t lStatus;
    if (pFilter->ctx == (void *)-1){
        lStatus =  ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return lStatus;
    }

    request_rec *pRequest = pFilter->r->main;

    struct CompareConf *tConf = reinterpret_cast<CompareConf *>(ap_get_module_config(pRequest->per_dir_config, &compare_module));
    if( tConf == NULL ){
        lStatus =  ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return lStatus;
    }

    boost::shared_ptr<DupModule::RequestInfo> *shPtr(reinterpret_cast<boost::shared_ptr<DupModule::RequestInfo> *>(ap_get_module_config(pRequest->request_config, &compare_module)));

    if ( !shPtr || !shPtr->get()) {
        lStatus =  ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return lStatus;
    }

    DupModule::RequestInfo *req = shPtr->get();
    if (!req->eos_seen) {
        lStatus =  ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return lStatus;
    }

    req->mRequest = std::string(pRequest->unparsed_uri);
    //write headers in Map
    apr_table_do(&iterateOverHeadersCallBack, &(req->mDupResponseHeader), pRequest->headers_out, NULL);

    std::string diffBody,diffHeader;
    if( tConf->mCompareDisabled){
        writeSerializedRequest(*req);
    }else{
        clock_t start=clock();
        if(tConf->mCompHeader.retrieveDiff(req->mResponseHeader,req->mDupResponseHeader,diffHeader)){
            if (tConf->mCompBody.retrieveDiff(req->mResponseBody,req->mDupResponseBody,diffBody)){
                if(diffHeader.length()!=0 || diffBody.length()!=0 || checkCassandraDiff(req->mId) ){
                    writeDifferences(*req,diffHeader,diffBody,double(clock() - start)/CLOCKS_PER_SEC);
                }
            }
        }
    }
    pFilter->ctx = (void *) -1;
    lStatus = ap_pass_brigade(pFilter->next, pBrigade);
    apr_brigade_cleanup(pBrigade);
    return lStatus;
}

};
