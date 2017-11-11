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

#pragma once

#include <apr_pools.h>
#include <apr_hooks.h>
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>
#include <curl/curl.h>
#include <exception>
#include <httpd.h>
#include <http_config.h>
#include <http_request.h>
#include <http_protocol.h>
// Work-around boost::chrono 1.53 conflict on CR typedef vs define in apache
#undef CR

#include <iostream>
#include <queue>
#include <unistd.h>

#include "Log.hh"
#include "RequestCommon.hh"
#include "RequestProcessor.hh"
#include "ThreadPool.hh"

extern module AP_DECLARE_DATA dup_module;

namespace DupModule {
    extern const char *gName;
    extern const char *gNameBody2Brigade;
    extern const char *gNameOut;
    extern const char *gNameOutBody;
    extern const char *gNameOutHeaders;

    extern RequestProcessor                             *gProcessor;
    extern ThreadPool<boost::shared_ptr<RequestInfo> >  *gThreadPool;

/**
 * A structure that holds the configuration specific to the location
 */
class DupConf {

public:

    DupConf();

    /** @brief the current Filter and Subs application scope set by the DupApplicationScope directive */
    ApplicationScope::eApplicationScope         currentApplicationScope;

    char                                        *dirName;

    /** @brief the current duplication destination set by the DupDestination directive */
    std::string                                 currentDupDestination;

    bool                                        synchronous;

    void setCurrentDuplicationType(DuplicationType::eDuplicationType dt);

    DuplicationType::eDuplicationType getCurrentDuplicationType() const;
    DuplicationType::eDuplicationType getHighestDuplicationType() const;

private:

    /** @brief the current duplication type*/
    DuplicationType::eDuplicationType          mCurrentDuplicationType;

    /** @brief the highest duplication type based on successive current ones*/
    DuplicationType::eDuplicationType          mHighestDuplicationType;

};

/**
 * @brief Initialize our the processor and thread pool on first config call
 */
void
init();

/**
 * @brief allocate a pointer to a string which will hold the path for the dir config if mod_dup is active on it
 * @param pPool the apache pool on which to allocate data
 * @param pDirName the directory name for which to create data
 * @return a void pointer to newly allocated object
 */
void *
createDirConfig(apr_pool_t *pPool, char *pDirName);

/**
 * @brief Initialize logging post-config
 * @param pPool the apache pool
 * @param pServer the corresponding server record
 * @return Always OK
 */
int
postConfig(apr_pool_t * pPool, apr_pool_t * pLog, apr_pool_t * pTemp, server_rec * pServer);

/**
 * @brief Set the destination host and port
 * @param pParams miscellaneous data
 * @param pCfg user data for the directory/location
 * @param pDestionation the destination in <host>[:<port>] format
 * @param percentageToDuplicate percentage of requests that matches to duplicate to this destination
 * @return Always DECLINED to let other modules handle the request
 */
const char*
setDestination(cmd_parms* pParams, void* pCfg, const char* pDestination, const char* percentageToDuplicate);

/**
 * @brief Set the minimum and maximum number of threads
 * @param pParams miscellaneous data
 * @param pCfg user data for the directory/location
 * @param pMin the minimum number of threads
 * @param pMax the maximum number of threads
 * @return NULL if parameters are valid, otherwise a string describing the error
 */
const char*
setThreads(cmd_parms* pParams, void* pCfg, const char* pMin, const char* pMax);

/**
 * @brief Set the timeout for outgoing requests
 * @param pParams miscellaneous data
 * @param pCfg user data for the directory/location
 * @param pTimeout the timeout for outgoing requests in ms
 * @return NULL if parameters are valid, otherwise a string describing the error
 */
const char*
setTimeout(cmd_parms* pParams, void* pCfg, const char* pTimeout);

/**
 * @brief Set the minimum and maximum queue size
 * @param pParams miscellaneous data
 * @param pCfg user data for the directory/location
 * @param pMin the minimum queue size
 * @param pMax the maximum queue size
 * @return NULL if parameters are valid, otherwise a string describing the error
 */
const char*
setQueue(cmd_parms* pParams, void* pCfg, const char* pMin, const char* pMax);

/**
 * @brief Add a substitution definition
 * @param pParams miscellaneous data
 * @param pCfg user data for the directory/location
 * @param pField the field on which to do the substitution
 * @param pMatch the regexp matching what should be replaced
 * @param pReplace the value which the match should be replaced with
 * @return NULL if parameters are valid, otherwise a string describing the error
 */
const char*
setSubstitute(cmd_parms* pParams, void* pCfg, const char *pField, const char* pMatch, const char* pReplace);

/**
 * @brief Add a raw substitution definition
 * @param pParams miscellaneous data
 * @param pCfg user data for the directory/location
 * @param pMatch the regexp matching what should be replaced
 * @param pReplace the value which the match should be replaced with
 * @return NULL if parameters are valid, otherwise a string describing the error
 */
const char*
setRawSubstitute(cmd_parms* pParams, void* pCfg,
                 const char* pMatch, const char* pReplace);


/**
 * @brief Add a filter definition
 * @param pParams miscellaneous data
 * @param pCfg user data for the directory/location
 * @param pField the field on which to do the substitution
 * @param pFilter a reg exp which has to match for this request to be duplicated
 * @return NULL if parameters are valid, otherwise a string describing the error
 */
const char*
setFilter(cmd_parms* pParams, void* pCfg, const char *pField, const char* pFilter);

/**
 * @brief Add a Raw filter definition
 * @param pParams miscellaneous data
 * @param pCfg user data for the directory/location
 * @param pFilter a reg exp which has to match for this request to be duplicated
 * @return NULL if parameters are valid, otherwise a string describing the error
 */
const char*
setRawFilter(cmd_parms* pParams, void* pCfg, const char* pFilter);

/**
 * @brief Activate duplication
 * @param pParams miscellaneous data
 * @param pCfg user data for the directory/location
 * @return NULL
 */
const char*
setActive(cmd_parms* pParams, void* pCfg);

/**
 * @brief Sets the program name to use in the logs
 * @return NULL
 */
const char*
setName(cmd_parms* pParams, void* pCfg, const char* pName);

/*
 * @brief Sets the URL codec to use during the request duplication process
 */
const char*
setUrlCodec(cmd_parms* pParams, void* pCfg, const char* pUrlCodec);

/**
 * @brief Clean up before the child exits
 */
apr_status_t
cleanUp(void *);

/**
 * @brief init curl and our own thread pool on child init
 * @param pPool the apache pool
 * @param pServer the apache server record
 */
void
childInit(apr_pool_t *pPool, server_rec *pServer);

/*
 * Defines the application scope for the elts defined after this statement
 * @param pParams miscellaneous data
 * @param pCfg user data for the directory/location
 * @param pDupType the string representing the application scope
 */
const char*
setApplicationScope(cmd_parms* pParams, void* pCfg, const char* pAppScope);

/**
 * @brief register hooks in apache
 * @param pPool the apache pool
 */
void registerHooks(apr_pool_t *pPool);

/**
 * @brief Sets the duplication type
 * @param pDupType duplication type as a string
 */
const char*
setDuplicationType(cmd_parms* pParams, void* pCfg, const char* pDupType);

/*
 * Read the request body and stores it in a RequestInfo object in the request context
 * Enrich the request context for mod_rewrite
 */
int
translateHook(request_rec *r);

/**
 * @brief the source input filter callback
 * This filter is placed first in the chain and serves the body stored in a RequestInfo object in the request context
 * to the other filters
 */
apr_status_t
inputFilterBody2Brigade(ap_filter_t *pF, apr_bucket_brigade *pB, ap_input_mode_t pMode, apr_read_type_e pBlock, apr_off_t pReadbytes);

/**
 * @brief the output filter callback
 */
apr_status_t
outputBodyFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade);

/**
 * @brief the output filter callback
 */
apr_status_t
outputHeadersFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade);

/**
 * @brief The input filter handler. Reads the request, stores it in a shared pointer allocated on the pool
 */
apr_status_t
inputFilterHandler(ap_filter_t *pF, apr_bucket_brigade *pB, ap_input_mode_t pMode, apr_read_type_e pBlock, apr_off_t pReadbytes);

}
