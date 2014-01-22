/*
* mod_compare - compares apache requests
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
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <exception>
#include <set>
#include <sstream>
#include <sys/syscall.h>
#include <algorithm>
#include <boost/tokenizer.hpp>

#include "mod_compare.hh"


namespace alg = boost::algorithm;


namespace CompareModule {


const char *gName = "Compare";
const char *c_COMPONENT_VERSION = "Compare/1.0";
const char* c_UNIQUE_ID = "UNIQUE_ID";


CompareConf::CompareConf() {
}


unsigned int CompareConf::getNextReqId() {
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

apr_status_t CompareConf::cleaner(void *self) {
    CompareConf *c = reinterpret_cast<CompareConf *>(self);
    c->~CompareConf();
    return 0;
}

/**
 * @brief allocate a pointer to a string which will hold the path for the dir config if mod_dup is active on it
 * @param pPool the apache pool on which to allocate data
 * @param pDirName the directory name for which to create data
 * @return a void pointer to newly allocated object
 */
void *
createDirConfig(apr_pool_t *pPool, char *pDirName)
{
    void *addr= apr_pcalloc(pPool, sizeof(class CompareConf));
    new (addr) CompareConf();
    apr_pool_cleanup_register(pPool, addr, CompareConf::cleaner,  NULL);
    return addr;
}

/**
 * @brief Initialize logging post-config
 * @param pPool the apache pool
 * @param pServer the corresponding server record
 * @return Always OK
 */
int
postConfig(apr_pool_t * pPool, apr_pool_t * pLog, apr_pool_t * pTemp, server_rec * pServer) {
    Log::init();

    ap_add_version_component(pPool, c_COMPONENT_VERSION) ;
    return OK;
}

/**
 * @brief Set the list of errors to ignore in the comparison of headers
 * @param pParams miscellaneous data
 * @param pCfg user data for the directory/location
 * @param pList the list of errors separated by ','
 * @return NULL if parameters are valid, otherwise a string describing the error
 */
const char*
setHeaderIgnoreList(cmd_parms* pParams, void* pCfg, const char* pList) {
    if (!pList || strlen(pList) == 0) {
        return "Missing header ignore list";
    }

    CompareConf *lConf = reinterpret_cast<CompareConf *>(pCfg);
    std::string lList(pList);
    boost::char_separator<char> lSep(",");
    typedef boost::tokenizer< boost::char_separator<char> > t_tokenizer;
    t_tokenizer lTok(lList, lSep);
    unsigned int lErr;

    for (t_tokenizer::iterator lIter = lTok.begin(); lIter != lTok.end(); ++lIter)
    {
        try
        {
            lErr = boost::lexical_cast<unsigned int>(*lIter);
        } catch (boost::bad_lexical_cast)
        {
            return "Invalid value for the header ignore list.";
        }
        lConf->mHeaderIgnoreList.push_back( lErr );
    }
    return NULL;
}

/**
 * @brief Set the list of errors to ignore in the comparison of bodies
 * @param pParams miscellaneous data
 * @param pCfg user data for the directory/location
 * @param pList the list of errors separated by ','
 * @return NULL if parameters are valid, otherwise a string describing the error
 */
const char*
setBodyIgnoreList(cmd_parms* pParams, void* pCfg, const char* pList) {
    if (!pList || strlen(pList) == 0) {
        return "Missing body ignore list";
    }

    CompareConf *lConf = reinterpret_cast<CompareConf *>(pCfg);
    std::string lList(pList);
    boost::char_separator<char> lSep(",");
    typedef boost::tokenizer< boost::char_separator<char> > t_tokenizer;
    t_tokenizer lTok(lList, lSep);
    unsigned int lErr;

    for (t_tokenizer::iterator lIter = lTok.begin(); lIter != lTok.end(); ++lIter)
    {
        try
        {
            lErr = boost::lexical_cast<unsigned int>(*lIter);
        } catch (boost::bad_lexical_cast)
        {
            return "Invalid value for the header ignore list.";
        }
        lConf->mBodyIgnoreList.push_back( lErr );
    }
    return NULL;
}


/** @brief Declaration of configuration commands */
command_rec gCmds[] = {
    // AP_INIT_(directive,
    //          function,
    //          void * extra data,
    //          overrides to allow in order to enable,
    //          help message),
        AP_INIT_TAKE1("HeaderIgnoreList",
                    reinterpret_cast<const char *(*)()>(&setHeaderIgnoreList),
                    0,
                    ACCESS_CONF,
                    "List of errors to ignore in the comparison of headers."),
        AP_INIT_TAKE1("BodyIgnoreList",
                    reinterpret_cast<const char *(*)()>(&setBodyIgnoreList),
                    0,
                    ACCESS_CONF,
                    "List of errors to ignore in the comparison of bodies."),
    {0}
};

/**
 * @brief register hooks in apache
 * @param pPool the apache pool
 */
void
registerHooks(apr_pool_t *pPool) {
#ifndef UNIT_TESTING
    ap_hook_post_config(postConfig, NULL, NULL, APR_HOOK_MIDDLE);
    ap_register_input_filter(gName, inputFilterHandler, NULL, AP_FTYPE_RESOURCE);
    ap_register_output_filter(gName, outputFilterHandler, NULL, AP_FTYPE_RESOURCE);
#endif
}

} // End namespace

/// Apache module declaration
module AP_MODULE_DECLARE_DATA compare_module = {
    STANDARD20_MODULE_STUFF,
    CompareModule::createDirConfig,
    0, // merge_dir_config
    0, // create_server_config
    0, // merge_server_config
    CompareModule::gCmds,
    CompareModule::registerHooks
};
