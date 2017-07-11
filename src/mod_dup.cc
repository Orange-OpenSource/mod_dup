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
#include "Utils.hh"

#define MOD_REWRITE_NAME "mod_rewrite.c"

namespace alg = boost::algorithm;

namespace DupModule {

RequestProcessor                                *gProcessor;
ThreadPool<boost::shared_ptr<RequestInfo> >    *gThreadPool;

const char *gName = "Dup";
const char *gNameOutBody = "DupOutBody";
const char *gNameOutHeaders = "DupOutHeaders";

const char *c_COMPONENT_VERSION = "Dup/1.0";

namespace DuplicationType {
    extern const char* c_ERROR_ON_STRING_VALUE;
};

DupConf::DupConf()
    : currentApplicationScope(ApplicationScope::HEADER)
    , dirName(NULL)
    , currentDupDestination()
    , synchronous(false)
    , mCurrentDuplicationType(DuplicationType::NONE)
    , mHighestDuplicationType(DuplicationType::NONE) {
    srand(time(NULL));
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
    apr_pool_cleanup_register(pPool, addr, CommonModule::cleaner<DupConf>, apr_pool_cleanup_null);
    return addr;
}

    //TODO move in preconfig function
static boost::shared_ptr<RequestInfo> POISON_REQUEST(new RequestInfo());

void
init() {
    gProcessor = new RequestProcessor();
    gThreadPool = new ThreadPool<boost::shared_ptr<RequestInfo> >(boost::bind(&RequestProcessor::run, gProcessor, _1),
                                                                  POISON_REQUEST);
    // Add the request timeout stat provider. Compose the lexical_cast with getTimeoutCount so that the resulting stat provider returns a string
    gThreadPool->addStat("#TmOut", boost::bind(boost::lexical_cast<std::string, unsigned int>,
                                               boost::bind(&RequestProcessor::getTimeoutCount, gProcessor)));
    gThreadPool->addStat("#DupReq", boost::bind(boost::lexical_cast<std::string, unsigned int>,
                                                boost::bind(&RequestProcessor::getDuplicatedCount, gProcessor)));
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
    if ( ! gThreadPool ) init();
    gThreadPool->setProgramName(pName);
    return NULL;
}

const char*
setUrlCodec(cmd_parms* pParams, void* pCfg, const char* pUrlCodec) {
    if (!pUrlCodec || strlen(pUrlCodec) == 0) {
        return "Missing url codec style";
    }
    if ( ! gProcessor ) init();
    gProcessor->setUrlCodec(pUrlCodec);
    return NULL;
}

const char*
setDestination(cmd_parms* pParams, void* pCfg, const char* pDestination, const char* duplicationPercentage) {
    const char *lErrorMsg = setActive(pParams, pCfg);
    if (lErrorMsg) {
        return lErrorMsg;
    }
    if ( ! gProcessor ) init();
    struct DupConf *tC = reinterpret_cast<DupConf *>(pCfg);
    assert(tC);
    if (!pDestination || strlen(pDestination) == 0) {
        return "Missing destination argument";
    }
    tC->currentDupDestination = pDestination;
    if (!duplicationPercentage) {
        tC->currentDuplicationPercentage = 100;
    } else {
        try {
            unsigned int perc = boost::lexical_cast<unsigned int>(duplicationPercentage);
            if (perc > 100) {
                return "Duplication percentage value not valid: must be an integer between 0 and 100";
            }
            gProcessor->setDestinationDuplicationPercentage(*tC, pDestination, perc);
        } catch (boost::bad_lexical_cast&) {
            std::string msg = "Duplication percentage value not valid: ";
            msg += duplicationPercentage;
            return strdup(msg.c_str());
        }
    }
    return NULL;
}

const char*
setApplicationScope(cmd_parms* pParams, void* pCfg, const char* pAppScope) {
    const char *lErrorMsg = setActive(pParams, pCfg);
    if (lErrorMsg) {
        return lErrorMsg;
    }
    if ( ! gProcessor ) init();
    struct DupConf *tC = reinterpret_cast<DupConf *>(pCfg);
    try {
        tC->currentApplicationScope = ApplicationScope::stringToEnum(pAppScope);
    } catch (std::exception& e) {
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
    if ( ! gProcessor ) init();
  
    struct DupConf *conf = reinterpret_cast<DupConf *>(pCfg);
    assert(conf);

    try {
        gProcessor->addRawSubstitution(pMatch, pReplace,
                                       *conf);
    } catch (boost::bad_expression&) {
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
	} catch (boost::bad_lexical_cast&) {
		return "Invalid value(s) for minimum and maximum number of threads.";
	}

	if (lMax < lMin) {
		return "Invalid value(s) for minimum and maximum number of threads.";
	}
	if ( ! gThreadPool ) init();
    gThreadPool->setThreads(lMin, lMax);
	return NULL;
}

const char*
setTimeout(cmd_parms* pParams, void* pCfg, const char* pTimeout) {
    size_t lTimeout;
    try {
        lTimeout = boost::lexical_cast<unsigned int>(pTimeout);
    } catch (boost::bad_lexical_cast&) {
        return "Invalid value(s) for timeout.";
    }

    if ( ! gProcessor ) init();
    gProcessor->setTimeout(lTimeout);
    return NULL;
}

const char*
setQueue(cmd_parms* pParams, void* pCfg, const char* pMin, const char* pMax) {
    size_t lMin, lMax;
    try {
        lMin = boost::lexical_cast<size_t>(pMin);
        lMax = boost::lexical_cast<size_t>(pMax);
    } catch (boost::bad_lexical_cast&) {
        return "Invalid value(s) for minimum and maximum queue size.";
    }

    if (lMax < lMin) {
        return "Invalid value(s) for minimum and maximum queue size.";
    }

    if ( ! gThreadPool ) init();
    gThreadPool->setQueue(lMin, lMax);
    return NULL;
}

const char*
setSubstitute(cmd_parms* pParams, void* pCfg, const char *pField, const char* pMatch, const char* pReplace) {
    const char *lErrorMsg = setActive(pParams, pCfg);
    if (lErrorMsg) {
        return lErrorMsg;
    }
    if ( ! gProcessor ) init();
    
    struct DupConf *conf = reinterpret_cast<DupConf *>(pCfg);
    assert(conf);

    try {
        gProcessor->addSubstitution(pField, pMatch, pReplace, *conf);
    } catch (boost::bad_expression&) {
        return "Invalid regular expression in substitution definition.";
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

#ifndef UNIT_TESTING
        if (!ap_find_linked_module(MOD_REWRITE_NAME)) {
            return "'mod_rewrite' is not loaded, Enable mod_rewrite to use mod_dup";
        }
#endif
    return NULL;
}

const char*
setSynchronous(cmd_parms* pParams, void* pCfg) {
    struct DupConf *lConf = reinterpret_cast<DupConf *>(pCfg);
    if (!lConf) {
        return "No per_dir conf defined. This should never happen!";
    }

    lConf->synchronous = true;

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
    } catch (std::exception& e) {
        return DuplicationType::c_ERROR_ON_STRING_VALUE;
    }
    return NULL;
}

static const char*
_setFilter(cmd_parms* pParams, void* pCfg, const char *pField, const char* pFilter, tFilter::eFilterTypes fType) {
    const char *lErrorMsg = setActive(pParams, pCfg);
    if (lErrorMsg) {
        return lErrorMsg;
    }
    if ( ! gProcessor ) init();
    
    struct DupConf *conf = reinterpret_cast<DupConf *>(pCfg);
    assert(conf);

    try {
        gProcessor->addFilter(pField, pFilter, *conf, fType);
    } catch (boost::bad_expression&) {
        return "Invalid regular expression in filter definition.";
    }
    return NULL;
}


const char*
setFilter(cmd_parms* pParams, void* pCfg, const char *pField, const char* pFilter) {
    return _setFilter(pParams, pCfg, pField, pFilter, tFilter::eFilterTypes::REGULAR);
}

const char*
setPreventFilter(cmd_parms* pParams, void* pCfg, const char *pField, const char* pFilter) {
    return _setFilter(pParams, pCfg, pField, pFilter, tFilter::eFilterTypes::PREVENT_DUPLICATION);
}

const char*
_setRawFilter(cmd_parms* pParams, void* pCfg, const char* pExpression, tFilter::eFilterTypes fType) {
    const char *lErrorMsg = setActive(pParams, pCfg);
    if (lErrorMsg) {
        return lErrorMsg;
    }
    if ( ! gProcessor ) init();
    struct DupConf *conf = reinterpret_cast<DupConf *>(pCfg);
    assert(conf);

    try {
        gProcessor->addRawFilter(pExpression, *conf, fType);
    } catch (boost::bad_expression&) {
        return "Invalid regular expression in filter definition.";
    }
    return NULL;
}

const char*
setRawFilter(cmd_parms* pParams, void* pCfg, const char* pExpression) {
    return _setRawFilter(pParams, pCfg, pExpression, tFilter::eFilterTypes::REGULAR);
}

const char*
setRawPreventFilter(cmd_parms* pParams, void* pCfg, const char* pExpression) {
    return _setRawFilter(pParams, pCfg, pExpression, tFilter::eFilterTypes::PREVENT_DUPLICATION);
}

apr_status_t
cleanUp(void *) {
    if ( gThreadPool ) {
        gThreadPool->stop();
        delete gThreadPool;
        gThreadPool = NULL;
    }

    if ( gProcessor ) {
        delete gProcessor;
        gProcessor = NULL;
    }
    return APR_SUCCESS;
}

void
childInit(apr_pool_t *pPool, server_rec *pServer) {
    curl_global_init(CURL_GLOBAL_ALL);
    if ( gThreadPool ) {
        gThreadPool->start();
    }
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
    AP_INIT_TAKE12("DupDestination",
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
    AP_INIT_TAKE2("DupPreventFilter",
                  reinterpret_cast<const char *(*)()>(&setPreventFilter),
                  0,
                  ACCESS_CONF,
                  "Cancels request duplication if matches"),
    AP_INIT_TAKE1("DupRawFilter",
                  reinterpret_cast<const char *(*)()>(&setRawFilter),
                  0,
                  ACCESS_CONF,
                  "Filter incoming request fields before duplicating them."
                  "1st Arg: BODY HEAD ALL, data to match with the regex"
                  "Simply performs a match with the specified REGEX."),
    AP_INIT_TAKE1("DupRawPreventFilter",
                  reinterpret_cast<const char *(*)()>(&setRawPreventFilter),
                  0,
                  ACCESS_CONF,
                  "Cancels request duplication if matches"),
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
    AP_INIT_NO_ARGS("DupSync",
                    reinterpret_cast<const char *(*)()>(&setSynchronous),
                    0,
                    ACCESS_CONF,
                    "Duplicating Synchronously. "
                    "This is only needed if no filter or substitution is defined."),
    AP_INIT_NO_ARGS("Dup",
                    reinterpret_cast<const char *(*)()>(&setActive),
                    0,
                    ACCESS_CONF,
                    "Duplicating requests on this location using the dup module. "
                    "This is only needed if no filter or substitution is defined."),
    {0}
};

#ifndef UNIT_TESTING

static void insertInputFilter(request_rec *pRequest) {
    struct DupConf *tConf = reinterpret_cast<DupConf *>(ap_get_module_config(pRequest->per_dir_config, &dup_module));
    assert(tConf);
    if (tConf->dirName) {
        ap_add_input_filter(gName, NULL, pRequest, pRequest->connection);
    }
}

static void insertOutputBodyFilter(request_rec *pRequest) {
    struct DupConf *tConf = reinterpret_cast<DupConf *>(ap_get_module_config(pRequest->per_dir_config, &dup_module));
    assert(tConf);
    if (tConf->dirName) {
        ap_add_output_filter(gNameOutBody, NULL, pRequest, pRequest->connection);
    }
}

static void insertOutputHeadersFilter(request_rec *pRequest) {
    struct DupConf *tConf = reinterpret_cast<DupConf *>(ap_get_module_config(pRequest->per_dir_config, &dup_module));
    assert(tConf);
    if (tConf->dirName) {
        ap_add_output_filter(gNameOutHeaders, NULL, pRequest, pRequest->connection);
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
    ap_hook_post_config(postConfig, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_child_init(&childInit, NULL, NULL, APR_HOOK_MIDDLE);

    ap_register_input_filter(gName, inputFilterHandler, NULL, AP_FTYPE_CONTENT_SET);

    // One of the first to get the body of the response
    ap_register_output_filter(gNameOutBody, outputBodyFilterHandler, NULL, AP_FTYPE_RESOURCE);
    // Just after protocol to be sure that headers out is filled but request_rec is still valid
    // using type connection produced calls to the filter with invalid brigades
    ap_register_output_filter(gNameOutHeaders, outputHeadersFilterHandler, NULL, AP_FTYPE_TRANSCODE);

    ap_hook_insert_filter(&insertInputFilter, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_insert_filter(&insertOutputBodyFilter, NULL, NULL, APR_HOOK_LAST);
    ap_hook_insert_filter(&insertOutputHeadersFilter, NULL, NULL, APR_HOOK_LAST);

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
