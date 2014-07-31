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

namespace alg = boost::algorithm;


namespace MigrateModule {


const char* gName = "Migrate";
const char* gNameOut = "MigrateOut";
const char* gNameOut2 = "MigrateOut2";
const char* c_COMPONENT_VERSION = "Migrate/1.0";
const char* c_named_mutex = "mod_migrate_log_mutex";
//bool gRem = boost::interprocess::named_mutex::remove(c_named_mutex);
//std::ofstream gFile;
const char * gFilePath = "/var/opt/hosting/log/apache2/compare_diff.log";
bool gWriteInFile = true;
std::string gLogFacility;

const char*
setActive(cmd_parms* pParams, void* pCfg) {
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
            return "'mod_rewrite' is not loaded, Enable mod_rewrite to use mod_dup";
        }
#endif
    return NULL;
}

const char*
setApplicationScope(cmd_parms* pParams, void* pCfg, const char* pAppScope) {
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

    conf->mEnvList.push_back(MigrateConf::MigrateEnv{pVarName,pMatchRegex,pSetValue,conf->mCurrentApplicationScope});

    return NULL;
}

/**
 * @brief allocate a pointer to a string which will hold the path for the dir config if mod_compare is active on it
 * @param pPool the apache pool on which to allocate data
 * @param pDirName the directory name for which to create data
 * @return a void pointer to newly allocated object
 */
void *
createDirConfig(apr_pool_t *pPool, char *pDirName)
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
int
postConfig(apr_pool_t * pPool, apr_pool_t * pLog, apr_pool_t * pTemp, server_rec * pServer) {

    Log::init();

    ap_add_version_component(pPool, c_COMPONENT_VERSION) ;

    return APR_SUCCESS;

}


void
childInit(apr_pool_t *pPool, server_rec *pServer)
{
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
        {0}
};

#ifndef UNIT_TESTING

static void insertInputFilter(request_rec *pRequest) {
    MigrateConf *lConf = reinterpret_cast<MigrateConf *>(ap_get_module_config(pRequest->per_dir_config, &migrate_module));
    assert(lConf);
    if (lConf->mDirName){
        ap_add_input_filter(gName, NULL, pRequest, pRequest->connection);
    }
}

//static void insertOutputFilter(request_rec *pRequest) {
////    CompareConf *lConf = reinterpret_cast<CompareConf *>(ap_get_module_config(pRequest->per_dir_config, &compare_module));
////    assert(lConf);
////    if (lConf->mIsActive){
//        ap_add_output_filter(gNameOut, NULL, pRequest, pRequest->connection);
////    }
//}

//static void insertOutputFilter2(request_rec *pRequest) {
////    CompareConf *lConf = reinterpret_cast<CompareConf *>(ap_get_module_config(pRequest->per_dir_config, &compare_module));
////    assert(lConf);
////    if (lConf->mIsActive){
//        ap_add_output_filter(gNameOut2, NULL, pRequest, pRequest->connection);
////    }
//}
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

    static const char * const beforeRewrite[] = {MOD_REWRITE_NAME, NULL};
    ap_hook_translate_name(&translateHook, NULL, beforeRewrite, APR_HOOK_MIDDLE);
//    ap_register_input_filter(gName, inputFilterHandler, NULL, AP_FTYPE_RESOURCE);
    // output filter of type AP_FTYPE_RESOURCE => only the body will be read ( the headers_out not set yet)
    //ap_register_output_filter(gNameOut, outputFilterHandler, NULL, AP_FTYPE_RESOURCE);
    // output filter of type AP_FTYPE_CONNECTION => only the response header will be read
    //ap_register_output_filter(gNameOut2, outputFilterHandler2, NULL, AP_FTYPE_TRANSCODE);
//    ap_hook_insert_filter(&insertInputFilter, NULL, NULL, APR_HOOK_FIRST);
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
