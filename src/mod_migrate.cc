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

#include "mod_migrate.hh"

#define MOD_REWRITE_NAME "mod_rewrite.c"
#define MOD_PROXY_NAME "mod_proxy.c"
#define MOD_PROXY_HTTP_NAME "mod_proxy_http.c"

namespace alg = boost::algorithm;


namespace MigrateModule {


const char* gName = "Migrate";
const char *gNameBody2Brigade = "MigrateBody2Brigade";
const char* c_COMPONENT_VERSION = "Migrate/1.0";

const char* setActive(cmd_parms* pParams, void* pCfg) {
    struct MigrateConf *lConf = reinterpret_cast<MigrateConf *>(pCfg);
    if (!lConf) {
        return "No per_dir conf defined. This should never happen!";
    }
    // No dir name initialized
    if (!(lConf->mDirName)) {
        lConf->mDirName = (char *) apr_pcalloc(pParams->pool, sizeof(char) * (strlen(pParams->path) + 1));
        strcpy(lConf->mDirName, pParams->path);
    }

#ifndef UNIT_TESTING
        if (!ap_find_linked_module(MOD_REWRITE_NAME)) {
            return "'mod_rewrite' is not loaded, Enable mod_rewrite to use mod_migrate";
        }
        if (!ap_find_linked_module(MOD_PROXY_NAME)) {
            return "'mod_proxy' is not loaded, Enable mod_proxy to use mod_migrate";
        }
        if (!ap_find_linked_module(MOD_PROXY_HTTP_NAME)) {
            return "'mod_proxy_http' is not loaded, Enable mod_proxy_http to use mod_migrate";
        }
#endif
    return NULL;
}

const char* setApplicationScope(cmd_parms* pParams, void* pCfg, const char* pAppScope) {
    const char *lErrorMsg = setActive(pParams, pCfg);
    if (lErrorMsg) {
        return lErrorMsg;
    }
    struct MigrateConf *tC = reinterpret_cast<MigrateConf *>(pCfg);
    try {
        tC->mCurrentApplicationScope = ApplicationScope::stringToEnum(pAppScope);
    } catch (std::exception& e) {
        return ApplicationScope::c_ERROR_ON_STRING_VALUE;
    }

    return NULL;
}

const char* setMigrateEnv(cmd_parms* pParams, void* pCfg, const char *pVarName, const char* pMatchRegex, const char* pSetValue) {
    const char *lErrorMsg = setActive(pParams, pCfg);
    if (lErrorMsg) {
        return lErrorMsg;
    }

    struct MigrateConf *conf = reinterpret_cast<MigrateConf *>(pCfg);
    assert(conf);

    MigrateConf::MigrateEnv env{pVarName,boost::regex(pMatchRegex),pSetValue,conf->mCurrentApplicationScope};
    conf->mEnvLists[pParams->path].push_back(env);

    return NULL;
}

/**
 * @brief allocate a pointer to a string which will hold the path for the dir config if mod_compare is active on it
 * @param pPool the apache pool on which to allocate data
 * @param pDirName the directory name for which to create data
 * @return a void pointer to newly allocated object
 */
void* createDirConfig(apr_pool_t *pPool, char *pDirName)
{
    void *addr= apr_pcalloc(pPool, sizeof(class MigrateConf));
    new (addr) MigrateConf();
    apr_pool_cleanup_register(pPool, addr, cleaner<MigrateConf>,  apr_pool_cleanup_null);
    return addr;
}

/**
 * @brief Initialize logging post-config
 * @param pPool the apache pool
 * @param pServer the corresponding server record
 * @return Always OK
 */
int postConfig(apr_pool_t * pPool, apr_pool_t * pLog, apr_pool_t * pTemp, server_rec * pServer) {

    Log::init();

    ap_add_version_component(pPool, c_COMPONENT_VERSION) ;

    return APR_SUCCESS;

}

static const char* _setFilter(cmd_parms* pParams, void* pCfg, const char *pField, const char* pFilter) {
    const char *lErrorMsg = setActive(pParams, pCfg);
    if (lErrorMsg) {
        return lErrorMsg;
    }

    struct MigrateConf *conf = reinterpret_cast<MigrateConf *>(pCfg);
    assert(conf);

    conf->mInputFilters[pParams->path].push_back(std::make_tuple(pField,pFilter,conf->mCurrentApplicationScope));
    return NULL;
}


const char* setFilter(cmd_parms* pParams, void* pCfg, const char *pField, const char* pFilter) {
    return _setFilter(pParams, pCfg, pField, pFilter);
}


void childInit(apr_pool_t *pPool, server_rec *pServer) {
    Log::debug("CHILD INIT");
}



/** @brief Declaration of configuration commands */
command_rec gCmds[] = {
        // AP_INIT_(directive,
        //          function,
        //          void * extra data,
        //          overrides to allow in order to enable,
        //          help message),
        AP_INIT_TAKE1("MigrateApplicationScope",
                reinterpret_cast<const char *(*)()>(&setApplicationScope),
                0,
                ACCESS_CONF,
                "Sets the application scope of the filters and subsitution rules that follow this declaration"),
        AP_INIT_TAKE3("MigrateEnv",
                reinterpret_cast<const char *(*)()>(&setMigrateEnv),
                0,
                ACCESS_CONF,
                "Enrich apache context with some variable."
                "Usage: DupEnrichContext VarName MatchRegex SetRegex"
                "VarName: The name of the variable to define"
                "MatchRegex: The regex that must match to define the variable"
                "SetRegex: The value to set if MatchRegex matches"),
        AP_INIT_TAKE2("MigrateFilter",
                reinterpret_cast<const char *(*)()>(&setFilter),
                0,
                ACCESS_CONF,
                "Filter incoming request fields before duplicating them. "
                "If one or more filters are specified, at least one of them has to match."),
        AP_INIT_NO_ARGS("Migrate",
                reinterpret_cast<const char *(*)()>(&setActive),
                0,
                ACCESS_CONF,
                "Duplicating requests on this location using the dup module. "
                "This is only needed if no filter or substitution is defined."),
        {0}
};

#ifndef UNIT_TESTING

static void insertInputFilter(request_rec *pRequest) {
    MigrateConf *lConf = reinterpret_cast<MigrateConf *>(ap_get_module_config(pRequest->per_dir_config, &migrate_module));
    assert(lConf);
    if (lConf->mDirName){
        ap_add_input_filter(gNameBody2Brigade, NULL, pRequest, pRequest->connection);
        Log::debug("Inserted filter");
    }
}

#endif

/**
 * @brief register hooks in apache
 * @param pPool the apache pool
 */
void registerHooks(apr_pool_t *pPool) {
#ifndef UNIT_TESTING
    ap_hook_post_config(postConfig, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_child_init(&childInit, NULL, NULL, APR_HOOK_MIDDLE);

    // Here we want to be almost the last filter
    ap_register_input_filter(gNameBody2Brigade, inputFilterBody2Brigade, NULL, AP_FTYPE_CONTENT_SET);

    static const char * const beforeRewrite[] = {MOD_REWRITE_NAME, NULL};
    ap_hook_translate_name(&translateHook, NULL, beforeRewrite, APR_HOOK_MIDDLE);

    ap_hook_insert_filter(&insertInputFilter, NULL, NULL, APR_HOOK_FIRST);
#endif
}

} // End namespace

/// Apache module declaration
module AP_MODULE_DECLARE_DATA migrate_module = {
        STANDARD20_MODULE_STUFF,
        MigrateModule::createDirConfig,
        0, // merge_dir_config
        0, // create_server_config
        0, // merge_server_config
        MigrateModule::gCmds,
        MigrateModule::registerHooks
};
