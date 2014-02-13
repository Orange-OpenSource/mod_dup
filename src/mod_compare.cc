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
#include <boost/interprocess/sync/named_mutex.hpp>
#include <exception>
#include <set>
#include <sstream>
#include <sys/syscall.h>
#include <algorithm>
#include <boost/tokenizer.hpp>
#include <fstream>
#include <stringCompare.hh>
#include <mapCompare.hh>

#include "mod_compare.hh"


namespace alg = boost::algorithm;


namespace CompareModule {


const char *gName = "Compare";
const char *c_COMPONENT_VERSION = "Compare/1.0";
const char* c_UNIQUE_ID = "UNIQUE_ID";

std::ofstream gFile;
const char * gFilePath;
boost::interprocess::named_mutex gMutex(boost::interprocess::open_or_create, "global_mutex");


CompareConf::CompareConf() {
}


apr_status_t CompareConf::cleaner(void *self) {
    if(self){
        CompareConf *c = reinterpret_cast<CompareConf *>(self);

        c->~CompareConf();
    }
    return 0;
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
    void *addr= apr_pcalloc(pPool, sizeof(class CompareConf));
    new (addr) CompareConf();
    apr_pool_cleanup_register(pPool, addr, CompareConf::cleaner,  apr_pool_cleanup_null);
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

apr_status_t
closeFile(void *) {

    gFile.close();
    return APR_SUCCESS;
}

void
childInit(apr_pool_t *pPool, server_rec *pServer)
{
    gFile.open(gFilePath, std::ofstream::out | std::ofstream::app );
    apr_pool_cleanup_register(pPool, NULL, apr_pool_cleanup_null, closeFile);
}

/**
 * @brief Set the list of errors which stop the comparison
 * @param pParams miscellaneous data
 * @param pCfg user data for the directory/location
 * @param pListType the type of list (STOP or IGNORE)
 * @param pValue the reg_ex to insert in the list
 * @return NULL if parameters are valid, otherwise a string describing the error
 */
const char*
setBodyList(cmd_parms* pParams, void* pCfg, const char* pListType, const char* pValue) {
    if (!pValue || strlen(pValue) == 0) {
        return "Missing reg_ex value for the body";
    }

    if (!pListType || strlen(pListType) == 0) {
        return "Missing the type of list";
    }

    CompareConf *lConf = reinterpret_cast<CompareConf *>(pCfg);
    std::string lListType(pListType);
    std::string lValue(pValue);

    if (strcmp("STOP", pListType) == 0)
    {
        lConf->mCompBody.addStopRegex(lValue);
    }
    else if(strcmp("IGNORE", pListType) == 0)
    {
        lConf->mCompBody.addIgnoreRegex(lValue);
    }
    else
    {
        return "Invalid value for the list type";
    }

    return NULL;
}

/**
 * @brief Set the list of errors to ignore in the comparison
 * @param pParams miscellaneous data
 * @param pCfg user data for the directory/location
 * @param pListType the type of list (STOP or IGNORE)
 * @param pHeader the header for which to apply the regex
 * @param pValue the reg_ex to insert in the list
 * @return NULL if parameters are valid, otherwise a string describing the error
 */
const char*
setHeaderList(cmd_parms* pParams, void* pCfg, const char* pListType, const char* pHeader, const char* pValue) {
    if (!pValue || strlen(pValue) == 0) {
        return "Missing reg_ex value for the header";
    }

    if (!pHeader || strlen(pHeader) == 0) {
            return "Missing header value";
        }

    if (!pListType || strlen(pListType) == 0) {
        return "Missing the type list";
    }

    CompareConf *lConf = reinterpret_cast<CompareConf *>(pCfg);
    std::string lListType(pListType);
    std::string lHeader(pHeader);
    std::string lValue(pValue);

    if (strcmp("STOP", pListType) == 0)
    {
        lConf->mCompHeader.addStopRegex(lHeader, lValue);
    }
    else if(strcmp("IGNORE", pListType) == 0)
    {
        lConf->mCompHeader.addIgnoreRegex(lHeader, lValue);
    }
    else
    {
        return "Invalid value for the list type";
    }

    return NULL;
}

/**
 * @brief Set the list of errors to ignore in the comparison
 * @param pParams miscellaneous data
 * @param pCfg user data for the directory/location
 * @param pListType the type of list (Header or Body)
 * @param pValue the value to insert in the list
 * @return NULL if parameters are valid, otherwise a string describing the error
 */
const char*
setFilePath(cmd_parms* pParams, void* pCfg, const char* pPath) {
    if (!pPath || strlen(pPath) == 0) {
        return "Missing path value";
    }
    Log::init();
    gFilePath = pPath;
    std::string lPath(gFilePath);
    Log::error(12, "ecco qui il percorso del file %s", lPath.c_str());

    return NULL;
}

/** @brief Declaration of configuration commands */
command_rec gCmds[] = {
    // AP_INIT_(directive,
    //          function,
    //          void * extra data,
    //          overrides to allow in order to enable,
    //          help message),
        AP_INIT_TAKE2("BodyList",
                    reinterpret_cast<const char *(*)()>(&setBodyList),
                    0,
                    ACCESS_CONF,
                    "List of reg_ex to apply to the body for the comparison."),
        AP_INIT_TAKE3("HeaderList",
                    reinterpret_cast<const char *(*)()>(&setHeaderList),
                    0,
                    ACCESS_CONF,
                    "List of reg_ex to apply to the Header for the comparison."),
        AP_INIT_TAKE1("FilePath",
                    reinterpret_cast<const char *(*)()>(&setFilePath),
                    0,
                    OR_ALL,
                    "Path of file where the differences will be logged."),
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
    ap_hook_child_init(&childInit, NULL, NULL, APR_HOOK_MIDDLE);
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
