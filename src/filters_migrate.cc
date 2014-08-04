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

#include "mod_migrate.hh"
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
#include <boost/regex.hpp>



namespace MigrateModule {

//void
//printRequest(request_rec *pRequest, std::string pBody)
//{
//    const char *reqId = apr_table_get(pRequest->headers_in, DupModule::c_UNIQUE_ID);
//    Log::debug("### Filtering a request with ID: %s, body size:%ld", reqId, pBody.size());
//    Log::debug("### Uri:%s", pRequest->uri);
//    Log::debug("### Request args: %s", pRequest->args);
//}


int enrichContext(request_rec *pRequest, const RequestInfo &rInfo) {
    MigrateConf *conf = reinterpret_cast<MigrateConf *>(ap_get_module_config(pRequest->per_dir_config, &migrate_module));
    if (!conf || !conf->mDirName) {
        // Not a location that we treat, we decline the request
        return DECLINED;
    }
    auto it = conf->mEnvLists.find(rInfo.mConfPath);

    // No filters for this path
    if (it == conf->mEnvLists.end() || it->second.empty())
        return 0;
    int count = 0;
    auto &envList = it->second;

    // Iteration through context enrichment
    for(const auto &ctx : envList) {
        if (ctx.mApplicationScope & ApplicationScope::HEADER) {
            std::string toSet = boost::regex_replace(rInfo.mArgs, ctx.mMatchRegex, ctx.mSetValue, boost::match_default | boost::format_no_copy);
            if (!toSet.empty()) {
                Log::debug("CE: header match: Value to set: %s, varName: %s", toSet.c_str(), ctx.mVarName.c_str());
#ifndef UNIT_TESTING
                apr_table_set(pRequest->subprocess_env, ctx.mVarName.c_str(), toSet.c_str());
#endif
                ++count;
            }
        }
        if ((ctx.mApplicationScope & ApplicationScope::BODY) && !rInfo.mBody.empty()) {
            std::string toSet = regex_replace(rInfo.mBody, ctx.mMatchRegex, ctx.mSetValue, boost::match_default | boost::format_no_copy);
            if (!toSet.empty()) {
                Log::debug("CE: Body match: Value to set: %s, varName: %s", toSet.c_str(), ctx.mVarName.c_str());
#ifndef UNIT_TESTING
                apr_table_set(pRequest->subprocess_env, ctx.mVarName.c_str(), toSet.c_str());
#endif
                ++count;
            }
        }
    }
    return count;
}

/*
 * Translate_name level HOOK
 *   - Read the body of a request.
 *   - Analyse request (body and header) with a synchroneous call on RequestProcessor::enrichContext method
 *   - If enrichment criteria are satisfied, the request context is enriched
 *   - This context can be used by mod_rewrite to redirect/modify the request
 *   - A RequestInfo object (containing the body) is stored in the request_config array. This object
 *      will be used by the inputfilter named 'inputFilterBody2Brigade' that reinjects the body to the input filters that follow him
 *   - A shared pointer manages the requestcontext object lifespan.
 */
int translateHook(request_rec *pRequest) {
    Log::debug("In translateHook");
    if (!pRequest->per_dir_config)
        return DECLINED;
    MigrateConf *conf = reinterpret_cast<MigrateConf *>(ap_get_module_config(pRequest->per_dir_config, &migrate_module));
    if (!conf || !conf->mDirName) {
        // Not a location that we treat, we decline the request
        return DECLINED;
    }

    unsigned int lReqID = MigrateModule::getNextReqId();
    std::string reqId = boost::lexical_cast<std::string>(lReqID);
    RequestInfo *info = new RequestInfo(reqId);
    // Allocation on a shared pointer on the request pool
    // We guarantee that whatever happens, the RequestInfo will be deleted
    void *space = apr_palloc(pRequest->pool, sizeof(boost::shared_ptr<RequestInfo>));
    new (space) boost::shared_ptr<RequestInfo>(info);
    // Registering of the shared pointer destructor on the pool
    apr_pool_cleanup_register(pRequest->pool, space, cleaner<boost::shared_ptr<RequestInfo> >,
            apr_pool_cleanup_null);
    // Backup in request context
    ap_set_module_config(pRequest->request_config, &migrate_module, (void *)space);

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
    std::string body;
    while (!MigrateModule::extractBrigadeContent(bb, pRequest->input_filters, info->mBody)){
        apr_brigade_cleanup(bb);
    }
    apr_brigade_cleanup(bb);
    // Body read :)
    Log::debug("BODY: %s",info->mBody.c_str());
    Log::debug("id: %s",info->mId.c_str());
    const char* lID = apr_table_get(pRequest->headers_in, c_UNIQUE_ID);
    // Copy Request ID in both headers
    if(lID == NULL) {
        apr_table_set(pRequest->headers_in, c_UNIQUE_ID, info->mId.c_str());
        apr_table_set(pRequest->headers_out, c_UNIQUE_ID, info->mId.c_str());
    }
    else {
        apr_table_set(pRequest->headers_out, c_UNIQUE_ID, lID);
    }

    // Synchronous context enrichment
    info->mConfPath = conf->mDirName;
    info->mArgs = pRequest->args ? pRequest->args : "";
    enrichContext(pRequest, *info);
    pRequest->read_length = 0;
    return DECLINED;
}


/**
 * InputFilter which target is to fill the brigade passed as an argument with
 * the body read in the previous hook: 'translateName'
 */
apr_status_t inputFilterBody2Brigade(ap_filter_t *pF, apr_bucket_brigade *pB, ap_input_mode_t pMode, apr_read_type_e pBlock, apr_off_t pReadbytes)
{
    Log::debug("In inputFilterBody2Brigade");
    request_rec *pRequest = pF->r;
    if (!pRequest || !pRequest->per_dir_config) {
        return ap_get_brigade(pF->next, pB, pMode, pBlock, pReadbytes);
    }

    // Retrieve request info from context
    boost::shared_ptr<RequestInfo> *shPtr = reinterpret_cast<boost::shared_ptr<RequestInfo> *>(ap_get_module_config(pRequest->request_config, &migrate_module));
    if (!shPtr) {
        // Happens after a rewrite on a location that we do not treat
        return ap_get_brigade(pF->next, pB, pMode, pBlock, pReadbytes);

    }
    RequestInfo *info = shPtr->get();

    if (pF->ctx != (void *) -1) {
        if (!pF->ctx) {
            pRequest->remaining = info->mBody.size();
        }
        long int read = (long int) pF->ctx;
        int bSize = info->mBody.size();
        int toRead = std::min((bSize - read), pReadbytes);
        if (toRead > 0) {
            apr_status_t st;
            if ((st = apr_brigade_write(pB, NULL, NULL, info->mBody.c_str() + read, toRead)) != APR_SUCCESS ) {
                Log::warn(1, "Failed to write request body in a brigade: %s",  info->mBody.c_str());
                return st;
            }

            read += toRead;
            pRequest->remaining -= toRead;
        }
        // Request context update
        if (read >= bSize) {
            pF->ctx = (void *) -1;
        } else {
            pF->ctx = (void*)(read);
        }

    }  else {
        return ap_get_brigade(pF->next, pB, pMode, pBlock, pReadbytes);
    }
    return APR_SUCCESS;
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

};
