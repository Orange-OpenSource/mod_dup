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
#include <list>
#include <ios>
#include <boost/regex.hpp>
#include <unordered_map>

#include "Log.hh"
#include "RequestInfo.hh"
#include "RequestProcessor.hh"

extern module AP_DECLARE_DATA migrate_module;

namespace MigrateModule {

/**
 * A class that holds the configuration specific to the location
 */
class MigrateConf {
public:
    struct MigrateEnv {
        std::string mVarName;
        boost::regex mMatchRegex;
        std::string mSetValue;
        ApplicationScope::eApplicationScope mApplicationScope;
    };

    char* mDirName;

    ApplicationScope::eApplicationScope mCurrentApplicationScope;

    /// Map with Location as key and a list of MigrateEnv structure as value
    std::unordered_map<std::string, std::list<MigrateEnv>> mEnvLists;

    MigrateConf() : mDirName(NULL),mCurrentApplicationScope(ApplicationScope::ALL) {}
};

/**
 * @brief allocate a pointer to a string which will hold the path for the dir config if mod_dup is active on it
 * @param pPool the apache pool on which to allocate data
 * @param pDirName the directory name for which to create data
 * @return a void pointer to newly allocated object
 */
void *
createDirConfigcreateDirConfig(apr_pool_t *pPool, char *pDirName);

/**
 * @brief Initialize logging post-config
 * @param pPool the apache pool
 * @param pServer the corresponding server record
 * @return Always OK
 */
int
postConfig(apr_pool_t * pPool, apr_pool_t * pLog, apr_pool_t * pTemp, server_rec * pServer);

///**
// * @brief Clean up before the child exits
// */
//apr_status_t
//cleanUp(void *);

/*
 * Read the request body ans stores it in a RequestInfo object in the request context
 * Enrich the request context for mod_rewrite
 */
int translateHook(request_rec *r);

/**
 * @brief register hooks in apache
 * @param pPool the apache pool
 */
void registerHooks(apr_pool_t *pPool);

int enrichContext(request_rec *pRequest, const RequestInfo &rInfo);

/**
 * @brief the input filter callback
 */
apr_status_t inputFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade, ap_input_mode_t pMode, apr_read_type_e pBlock, apr_off_t pReadbytes);

/**
 * @brief the source input filter callback
 * This filter is placed first in the chain and serves the body stored in a RequestInfo object in the request context
 * to the other filters
 */
apr_status_t inputFilterBody2Brigade(ap_filter_t *pF, apr_bucket_brigade *pB, ap_input_mode_t pMode, apr_read_type_e pBlock, apr_off_t pReadbytes);

/*
 * Defines the application scope for the elts defined after this statement
 * @param pParams miscellaneous data
 * @param pCfg user data for the directory/location
 * @param pDupType the string representing the application scope
 */
const char* setApplicationScope(cmd_parms* pParams, void* pCfg, const char* pAppScope);

const char* setMigrateEnv(cmd_parms* pParams, void* pCfg, const char *pVarName, const char* pMatchRegex, const char* pSetValue);

/**
 * @brief Activate duplication
 * @param pParams miscellaneous data
 * @param pCfg user data for the directory/location
 * @return NULL
 */
const char* setActive(cmd_parms* pParams, void* pCfg);

/*
 * Method that calls the destructor of an object which type is templated
 */
template <class T>
apr_status_t
cleaner(void *self) {
    if (self) {
        T *elt = reinterpret_cast<T *>(self);
        assert(elt);
        elt->~T();
    }
    return 0;
}

}

