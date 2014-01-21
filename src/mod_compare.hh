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

#pragma once

#include <apr_pools.h>
#include <apr_hooks.h>
#include <boost/lexical_cast.hpp>
#include <exception>
#include <httpd.h>
#include <http_config.h>
#include <http_request.h>
#include <http_protocol.h>
#include <iostream>
#include <queue>
#include <unistd.h>
#include <list>

#include "Log.hh"

extern module AP_DECLARE_DATA compare_module;

namespace CompareModule {

/**
 * A class that holds the configuration specific to the location
 */
class CompareConf {

public:

    CompareConf();

    static apr_status_t cleaner(void *self);

    /*
     * Returns the next random request ID
     * method is reentrant
     */
    unsigned int getNextReqId();

    std::list< unsigned int > mHeaderIgnoreList;
    std::list< unsigned int > mBodyIgnoreList;
};

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
 * @brief Clean up before the child exits
 */
apr_status_t
cleanUp(void *);

/**
 * @brief register hooks in apache
 * @param pPool the apache pool
 */
void registerHooks(apr_pool_t *pPool);

/**
 * @brief the input filter callback
 */
apr_status_t
inputFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade, ap_input_mode_t pMode, apr_read_type_e pBlock, apr_off_t pReadbytes);

/** @brief the output filter callback
 * Plugged only in REQUEST_WITH_ANSWER mode
 */
/*apr_status_t
outputFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade);*/

/**
 * @brief Set the list of errors to ignore in the comparison of headers
 * @param pParams miscellaneous data
 * @param pCfg user data for the directory/location
 * @param pList the list of errors separated by ','
 * @return NULL if parameters are valid, otherwise a string describing the error
 */
const char*
setHeaderIgnoreList(cmd_parms* pParams, void* pCfg, const char* pList);

/**
 * @brief Set the list of errors to ignore in the comparison of bodies
 * @param pParams miscellaneous data
 * @param pCfg user data for the directory/location
 * @param pList the list of errors separated by ','
 * @return NULL if parameters are valid, otherwise a string describing the error
 */
const char*
setBodyIgnoreList(cmd_parms* pParams, void* pCfg, const char* pList);

}

