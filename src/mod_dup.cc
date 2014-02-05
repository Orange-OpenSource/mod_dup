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

#include <httpd.h>
#include <http_config.h>
#include <http_request.h>
#include <http_protocol.h>
#include <http_connection.h>
#include <apr_pools.h>
#include <apr_hooks.h>
#include "apr_strings.h"
#include <unistd.h>
#include <curl/curl.h>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <exception>
#include <set>
#include <sstream>
#include <sys/syscall.h>


#include "mod_dup.hh"

namespace alg = boost::algorithm;


namespace DupModule {

RequestProcessor *gProcessor;
ThreadPool<RequestInfo*> *gThreadPool;

const char *gName = "Dup";
const char *gNameBody2Brigade = "DupBody2Brigade";
const char *gNameOut = "DupOut";

const char *c_COMPONENT_VERSION = "Dup/1.0";
const char* c_UNIQUE_ID = "UNIQUE_ID";

namespace DuplicationType {
    extern const char* c_ERROR_ON_STRING_VALUE;
};

DupConf::DupConf()
    : currentApplicationScope(ApplicationScope::HEADER)
    , dirName(NULL)
    , currentDupDestination()
    , mCurrentDuplicationType(DuplicationType::NONE)
    , mHighestDuplicationType(DuplicationType::NONE) {
    srand(time(NULL));
}

unsigned int DupConf::getNextReqId() {
    // Thread-local static variables
    // Makes sure the random pattern/sequence is different for each thread
    static __thread bool lInitialized = false;
    static __thread struct random_data lRD = { 0, 0, 0, 0, 0, 0, 0} ;
    static __thread char lRSB[8];

    // Initialized per thread
    int lRet = 0;
    if (!lInitialized) {
        memset(lRSB,0, 8);
        struct timespec lTimeSpec;
        clock_gettime(CLOCK_MONOTONIC, &lTimeSpec);
        // The seed is randomized using thread ID and nanoseconds
        unsigned int lSeed = lTimeSpec.tv_nsec + (pid_t) syscall(SYS_gettid);

        // init State must be different for all threads or each will answer the same sequence
        lRet |= initstate_r(lSeed, lRSB, 8, &lRD);
        lInitialized = true;
    }
    // Thread-safe calls with thread local initialization
    int lRandNum = 1;
    lRet |= random_r(&lRD, &lRandNum);
    if (lRet)
        Log::error(5, "Error on number randomisation");
    return lRandNum;
}

void
DupConf::setCurrentDuplicationType(DuplicationType::eDuplicationType dt) {
    mCurrentDuplicationType = dt;
    if ( dt > mHighestDuplicationType ) {
        mHighestDuplicationType = dt;
    }
}

DuplicationType::eDuplicationType
DupConf::getCurrentDuplicationType() const {
    return mCurrentDuplicationType;
}

DuplicationType::eDuplicationType
DupConf::getHighestDuplicationType() const {
    return mHighestDuplicationType;
}

void *
createDirConfig(apr_pool_t *pPool, char *pDirName)
{
    void *addr= apr_palloc(pPool, sizeof(class DupConf));
    new (addr) DupConf();
    apr_pool_cleanup_register(pPool, addr, cleaner<DupConf>, apr_pool_cleanup_null);
    return addr;
}

int
preConfig(apr_pool_t * pPool, apr_pool_t * pLog, apr_pool_t * pTemp) {
    gProcessor = new RequestProcessor();
    gThreadPool = new ThreadPool<RequestInfo *>(boost::bind(&RequestProcessor::run, gProcessor, _1), &POISON_REQUEST);
    // Add the request timeout stat provider. Compose the lexical_cast with getTimeoutCount so that the resulting stat provider returns a string
    gThreadPool->addStat("#TmOut", boost::bind(boost::lexical_cast<std::string, unsigned int>,
                                               boost::bind(&RequestProcessor::getTimeoutCount, gProcessor)));
    gThreadPool->addStat("#DupReq", boost::bind(boost::lexical_cast<std::string, unsigned int>,
                                                boost::bind(&RequestProcessor::getDuplicatedCount, gProcessor)));
    return OK;
}

int
postConfig(apr_pool_t * pPool, apr_pool_t * pLog, apr_pool_t * pTemp, server_rec * pServer) {
    Log::init();
    ap_add_version_component(pPool, c_COMPONENT_VERSION) ;
    return OK;
}

const char*
setName(cmd_parms* pParams, void* pCfg, const char* pName) {
    if (!pName || strlen(pName) == 0) {
        return "Missing program name";
    }
    gThreadPool->setProgramName(pName);
    return NULL;
}

const char*
setUrlCodec(cmd_parms* pParams, void* pCfg, const char* pUrlCodec) {
    if (!pUrlCodec || strlen(pUrlCodec) == 0) {
        return "Missing url codec style";
    }
    gProcessor->setUrlCodec(pUrlCodec);
    return NULL;
}

const char*
setDestination(cmd_parms* pParams, void* pCfg, const char* pDestination) {
    const char *lErrorMsg = setActive(pParams, pCfg);
    if (lErrorMsg) {
        return lErrorMsg;
    }
    struct DupConf *tC = reinterpret_cast<DupConf *>(pCfg);
    assert(tC);
    if (!pDestination || strlen(pDestination) == 0) {
        return "Missing destination argument";
    }
    tC->currentDupDestination = pDestination;
    return NULL;
}

const char*
setApplicationScope(cmd_parms* pParams, void* pCfg, const char* pAppScope) {
    const char *lErrorMsg = setActive(pParams, pCfg);
    if (lErrorMsg) {
        return lErrorMsg;
    }
    struct DupConf *tC = reinterpret_cast<DupConf *>(pCfg);
    try {
        tC->currentApplicationScope = ApplicationScope::stringToEnum(pAppScope);
    } catch (std::exception e) {
        return ApplicationScope::c_ERROR_ON_STRING_VALUE;
    }
    return NULL;
}

const char*
setRawSubstitute(cmd_parms* pParams, void* pCfg,
                 const char* pMatch, const char* pReplace){
    const char *lErrorMsg = setActive(pParams, pCfg);
    if (lErrorMsg) {
        return lErrorMsg;
    }
    struct DupConf *conf = reinterpret_cast<DupConf *>(pCfg);
    assert(conf);

    try {
        gProcessor->addRawSubstitution(pParams->path, pMatch, pReplace,
                                       *conf);
    } catch (boost::bad_expression) {
        return "Invalid regular expression in substitution definition.";
    }
    return NULL;
}

const char*
setThreads(cmd_parms* pParams, void* pCfg, const char* pMin, const char* pMax) {
	size_t lMin, lMax;
	try {
		lMin = boost::lexical_cast<size_t>(pMin);
		lMax = boost::lexical_cast<size_t>(pMax);
	} catch (boost::bad_lexical_cast) {
		return "Invalid value(s) for minimum and maximum number of threads.";
	}

	if (lMax < lMin) {
		return "Invalid value(s) for minimum and maximum number of threads.";
	}
	gThreadPool->setThreads(lMin, lMax);
	return NULL;
}

const char*
setTimeout(cmd_parms* pParams, void* pCfg, const char* pTimeout) {
    size_t lTimeout;
    try {
        lTimeout = boost::lexical_cast<unsigned int>(pTimeout);
    } catch (boost::bad_lexical_cast) {
        return "Invalid value(s) for timeout.";
    }

    gProcessor->setTimeout(lTimeout);
    return NULL;
}

const char*
setQueue(cmd_parms* pParams, void* pCfg, const char* pMin, const char* pMax) {
    size_t lMin, lMax;
    try {
        lMin = boost::lexical_cast<size_t>(pMin);
        lMax = boost::lexical_cast<size_t>(pMax);
    } catch (boost::bad_lexical_cast) {
        return "Invalid value(s) for minimum and maximum queue size.";
    }

    if (lMax < lMin) {
        return "Invalid value(s) for minimum and maximum queue size.";
    }

    gThreadPool->setQueue(lMin, lMax);
    return NULL;
}

const char*
setSubstitute(cmd_parms* pParams, void* pCfg, const char *pField, const char* pMatch, const char* pReplace) {
    const char *lErrorMsg = setActive(pParams, pCfg);
    if (lErrorMsg) {
        return lErrorMsg;
    }

    struct DupConf *conf = reinterpret_cast<DupConf *>(pCfg);
    assert(conf);

    try {
        gProcessor->addSubstitution(pParams->path, pField, pMatch, pReplace, *conf);
    } catch (boost::bad_expression) {
        return "Invalid regular expression in substitution definition.";
    }
    return NULL;
}

const char*
setEnrichContext(cmd_parms* pParams, void* pCfg, const char *pVarName, const char* pMatchRegex, const char* pSetValue) {
    const char *lErrorMsg = setActive(pParams, pCfg);
    if (lErrorMsg) {
        return lErrorMsg;
    }

    struct DupConf *conf = reinterpret_cast<DupConf *>(pCfg);
    assert(conf);

    try {
        gProcessor->addEnrichContext(pParams->path, pVarName, pMatchRegex, pSetValue, *conf);
    } catch (boost::bad_expression) {
        return "Invalid regular expression in EnrichContext definition.";
    }
    return NULL;
}

const char*
setActive(cmd_parms* pParams, void* pCfg) {
    struct DupConf *lConf = reinterpret_cast<DupConf *>(pCfg);
    if (!lConf) {
        return "No per_dir conf defined. This should never happen!";
    }
    // No dir name initialized
    if (!(lConf->dirName)) {
        lConf->dirName = (char *) apr_pcalloc(pParams->pool, sizeof(char) * (strlen(pParams->path) + 1));
        strcpy(lConf->dirName, pParams->path);
    }
    return NULL;
}

const char*
setDuplicationType(cmd_parms* pParams, void* pCfg, const char* pDupType) {
    const char *lErrorMsg = setActive(pParams, pCfg);
    if (lErrorMsg) {
        return lErrorMsg;
    }

    struct DupConf *conf = reinterpret_cast<DupConf *>(pCfg);
    assert(conf);

    try {
        conf->setCurrentDuplicationType(DuplicationType::stringToEnum(pDupType));
    } catch (std::exception e) {
        return DuplicationType::c_ERROR_ON_STRING_VALUE;
    }
    return NULL;
}

const char*
setFilter(cmd_parms* pParams, void* pCfg, const char *pField, const char* pFilter) {
    const char *lErrorMsg = setActive(pParams, pCfg);
    if (lErrorMsg) {
        return lErrorMsg;
    }

    struct DupConf *conf = reinterpret_cast<DupConf *>(pCfg);
    assert(conf);

    try {
        gProcessor->addFilter(pParams->path, pField, pFilter, *conf);
    } catch (boost::bad_expression) {
        return "Invalid regular expression in filter definition.";
    }
    return NULL;
}

const char*
setRawFilter(cmd_parms* pParams, void* pCfg, const char* pExpression) {
    const char *lErrorMsg = setActive(pParams, pCfg);
    if (lErrorMsg) {
        return lErrorMsg;
    }
    struct DupConf *conf = reinterpret_cast<DupConf *>(pCfg);
    assert(conf);

    try {
        gProcessor->addRawFilter(pParams->path, pExpression, *conf);
    } catch (boost::bad_expression) {
        return "Invalid regular expression in filter definition.";
    }
    return NULL;
}

apr_status_t
cleanUp(void *) {
    gThreadPool->stop();
    delete gThreadPool;
    gThreadPool = NULL;

    delete gProcessor;
    gProcessor = NULL;
    return APR_SUCCESS;
}

void
childInit(apr_pool_t *pPool, server_rec *pServer) {
	curl_global_init(CURL_GLOBAL_ALL);
	gThreadPool->start();

	apr_pool_cleanup_register(pPool, NULL, cleanUp, cleanUp);
}


/** @brief Declaration of configuration commands */
command_rec gCmds[] = {
    // AP_INIT_(directive,
    //          function,
    //          void * extra data,
    //          overrides to allow in order to enable,
    //          help message),
    AP_INIT_TAKE1("DupName",
                  reinterpret_cast<const char *(*)()>(&setName),
                  0,
                  OR_ALL,
                  "Set the program name for the stats log messages"),
    AP_INIT_TAKE1("DupUrlCodec",
                  reinterpret_cast<const char *(*)()>(&setUrlCodec),
                  0,
                  OR_ALL,
                  "Set the url enc/decoding style for url arguments (default or apache)"),
    AP_INIT_TAKE1("DupTimeout",
                  reinterpret_cast<const char *(*)()>(&setTimeout),
                  0,
                  OR_ALL,
                  "Set the timeout for outgoing requests in milliseconds."),
    AP_INIT_TAKE2("DupThreads",
                  reinterpret_cast<const char *(*)()>(&setThreads),
                  0,
                  OR_ALL,
                  "Set the minimum and maximum number of threads per pool."),
    AP_INIT_TAKE2("DupQueue",
                  reinterpret_cast<const char *(*)()>(&setQueue),
                  0,
                  OR_ALL,
                  "Set the minimum and maximum queue size for each thread pool."),
    AP_INIT_TAKE1("DupDuplicationType",
                  reinterpret_cast<const char *(*)()>(&setDuplicationType),
                  0,
                  ACCESS_CONF,
                  "Sets the duplication type that will used for all the following filters declarations"),
    AP_INIT_TAKE1("DupDestination",
                  reinterpret_cast<const char *(*)()>(&setDestination),
                  0,
                  ACCESS_CONF,
                  "Set the destination for the duplicated requests. Format: host[:port]"),
    AP_INIT_TAKE1("DupApplicationScope",
                  reinterpret_cast<const char *(*)()>(&setApplicationScope),
                  0,
                  ACCESS_CONF,
                  "Sets the application scope of the filters and subsitution rules that follow this declaration"),
    AP_INIT_TAKE2("DupFilter",
                  reinterpret_cast<const char *(*)()>(&setFilter),
                  0,
                  ACCESS_CONF,
                  "Filter incoming request fields before duplicating them. "
                  "If one or more filters are specified, at least one of them has to match."),
    AP_INIT_TAKE1("DupRawFilter",
                  reinterpret_cast<const char *(*)()>(&setRawFilter),
                  0,
                  ACCESS_CONF,
                  "Filter incoming request fields before duplicating them."
                  "1st Arg: BODY HEAD ALL, data to match with the regex"
                  "Simply performs a match with the specified REGEX."),
    AP_INIT_TAKE2("DupRawSubstitute",
                  reinterpret_cast<const char *(*)()>(&setRawSubstitute),
                  0,
                  ACCESS_CONF,
                  "Filter incoming request fields before duplicating them."
                  "1st Arg: BODY HEAD ALL, data to match with the regex"
                  "Simply performs a match with the specified REGEX."),
    AP_INIT_TAKE3("DupSubstitute",
                  reinterpret_cast<const char *(*)()>(&setSubstitute),
                  0,
                  ACCESS_CONF,
                  ""),
    AP_INIT_TAKE3("DupEnrichContext",
                  reinterpret_cast<const char *(*)()>(&setEnrichContext),
                  0,
                  ACCESS_CONF,
                  "Enrich apache context with some variable."
                  "Usage: DupEnrichContext VarName MatchRegex SetRegex"
                  "VarName: The name of the variable to define"
                  "MatchRegex: The regex that must match to define the variable"
                  "SetRegex: The value to set if MatchRegex matches"),
    AP_INIT_NO_ARGS("Dup",
                    reinterpret_cast<const char *(*)()>(&setActive),
                    0,
                    ACCESS_CONF,
                    "Duplicating requests on this location using the dup module. "
                    "This is only needed if no filter or substitution is defined."),
    {0}
};

#ifndef UNIT_TESTING
// Register the dup filters
static void insertInputFilter(request_rec *pRequest) {
    struct DupConf *tConf = reinterpret_cast<DupConf *>(ap_get_module_config(pRequest->per_dir_config, &dup_module));
    assert(tConf);
    if (tConf->dirName) {
        ap_add_input_filter(gNameBody2Brigade, NULL, pRequest, pRequest->connection);
    }
}

static void insertOutputFilter(request_rec *pRequest) {
    struct DupConf *tConf = reinterpret_cast<DupConf *>(ap_get_module_config(pRequest->per_dir_config, &dup_module));
    assert(tConf);
    if (tConf->dirName) {
        ap_add_output_filter(gNameOut, NULL, pRequest, pRequest->connection);
    }
}
#endif

/**
 * @brief register hooks in apache
 * @param pPool the apache pool
 */
void
registerHooks(apr_pool_t *pPool) {
#ifndef UNIT_TESTING
    ap_hook_pre_config(preConfig, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_post_config(postConfig, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_child_init(&childInit, NULL, NULL, APR_HOOK_MIDDLE);

    // Here we want to be almost the last filter
    ap_register_input_filter(gNameBody2Brigade, inputFilterBody2Brigade, NULL, AP_FTYPE_CONTENT_SET);

    // And now one of the first
    ap_register_output_filter(gNameOut, outputFilterHandler, NULL, AP_FTYPE_RESOURCE);
    static const char * const beforeRewrite[] = {"mod_rewrite.c", NULL};
    ap_hook_translate_name(&translateHook, NULL, beforeRewrite, APR_HOOK_MIDDLE);
    ap_hook_insert_filter(&insertInputFilter, NULL, NULL, APR_HOOK_FIRST);
    ap_hook_insert_filter(&insertOutputFilter, NULL, NULL, APR_HOOK_MIDDLE);
#endif
}

} // End namespace

/// Apache module declaration
module AP_MODULE_DECLARE_DATA dup_module = {
    STANDARD20_MODULE_STUFF,
    DupModule::createDirConfig,
    0, // merge_dir_config
    0, // create_server_config
    0, // merge_server_config
    DupModule::gCmds,
    DupModule::registerHooks
};
