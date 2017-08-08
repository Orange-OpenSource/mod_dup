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
// Work-around boost::chrono 1.53 conflict on CR typedef vs define in apache
#undef CR

#include <iostream>
#include <queue>
#include <unistd.h>
#include <list>
#include <fstream>
#include <ios>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/date_time.hpp>
#include "libws_diff/DiffPrinter/diffPrinter.hh"

#include "Log.hh"
#include "RequestInfo.hh"
#include "deserialize.hh"

#include <libws_diff/stringCompare.hh>
#include <libws_diff/mapCompare.hh>
#include <libws_diff/DiffPrinter/diffPrinter.hh>

#define LOGMAXSIZE 65536

extern module AP_DECLARE_DATA compare_module;

namespace CompareModule {

extern std::ofstream gFile;
extern const char* gFilePath;
extern bool gWriteInFile;
extern std::string gLogFacility;

/**
 * @brief Get the global mutex used to synchronize compare diffs
 * @return a pointer to the global mutex
 */
pthread_mutex_t *getGlobalMutex();

/**
 * A class that holds the configuration specific to the location
 */
class CompareConf {

public:

    CompareConf(std::string dirName = "");

    static apr_status_t cleaner(void *self);

    LibWsDiff::StringCompareBody mCompBody;
    LibWsDiff::MapCompare mCompHeader;
    LibWsDiff::diffPrinter::diffTypeAvailable mLogType;
    bool mCompareDisabled;
    bool mIsActive;
    std::string mDirName;

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

int translateHook(request_rec *pRequest);

/**
 * @brief the input filter callback
 */
apr_status_t
inputFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade, ap_input_mode_t pMode, apr_read_type_e pBlock, apr_off_t pReadbytes);

/** @brief the output filter callback
 * Plugged only in REQUEST_WITH_ANSWER mode
 */
apr_status_t
outputFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade);

apr_status_t
outputFilterHandler2(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade);

/**
 * @brief Set the list of errors to ignore in the comparison
 * @param pParams miscellaneous data
 * @param pCfg user data for the directory/location
 * @param pListType the type of list (Header or Body)
 * @param pValue the value to insert in the list
 * @return NULL if parameters are valid, otherwise a string describing the error
 */
const char* setHeaderList(cmd_parms* pParams, void* pCfg, const char* pListType, const char* pAffectedKey, const char* pValue);

/**
 * @brief Set the list of errors who stop the comparison
 * @param pParams miscellaneous data
 * @param pCfg user data for the directory/location
 * @param pListType the type of list (Header or Body)
 * @param pValue the value to insert in the list
 * @return NULL if parameters are valid, otherwise a string describing the error
 */
const char* setBodyList(cmd_parms* pParams, void* pCfg, const char* pListType, const char* pValue);

const char* setDiffLogType(cmd_parms* pParams, void* pCfg, const char* pValue);

void
printRequest(request_rec *pRequest, std::string pBody);

bool writeCassandraDiff(const std::string &pUniqueID, LibWsDiff::diffPrinter& printer);

void writeSerializedRequest(const DupModule::RequestInfo& req);

void childInit(apr_pool_t *pPool, server_rec *pServer);

void writeInFacility(const std::string& pDiffLog);

void writeDifferences(const DupModule::RequestInfo &pReqInfo,
		LibWsDiff::diffPrinter& printer,
		boost::posix_time::time_duration time);

void map2string(const std::map< std::string, std::string> &pMap, std::string &pString);

int iterateOverHeadersCallBack(void *d, const char *key, const char *value);

apr_status_t closeLogFile(void *);

apr_status_t openLogFile(const char* filepath,std::ios_base::openmode mode=std::ios_base::out);

const char* setFilePath(cmd_parms* pParams, void* pCfg, const char* pPath);

const char* setDisableLibwsdiff(cmd_parms* pParams, void* pCfg, const char* pValue);

const char* setLogFacility(cmd_parms* pParams, void* pCfg, const char* pValue);

bool checkCassandraDiff(const std::string &pUniqueID);

void changeMethod(request_rec *pRequest, const std::string& pMethod);

}

