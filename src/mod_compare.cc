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
#include <unixd.h>
#include <sys/mman.h>


#include "mod_compare.hh"

#define MOD_REWRITE_NAME "mod_rewrite.c"

namespace alg = boost::algorithm;


namespace CompareModule {


const char* gName = "Compare";
const char* gNameOut = "CompareOut";
const char* gNameOut2 = "CompareOut2";
const char* c_COMPONENT_VERSION = "Compare/1.0";
const char* c_named_mutex = "mod_compare_log_mutex";
std::ofstream gFile;
const char * gFilePath = "/var/opt/hosting/log/apache2/compare_diff.log";
bool gWriteInFile = true;
std::string gLogFacility;


pthread_mutex_t *getGlobalMutex() {
    static pthread_mutex_t *mutex = NULL;
    if (mutex == NULL) {
        int fd;
        fd = shm_open(c_named_mutex, O_RDWR, 0666);
        if (fd < 0) {
            Log::error(42, "[COMPARE] Cannot open global mutex named: %s.", c_named_mutex);
            return NULL;
        }
        mutex = (pthread_mutex_t *)mmap(NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);
    }
    return mutex;
}

/**
 * @brief Create the global mutex in the shared memory
 * @return 0 if successful, -1 when it fails
 */
int
createGlobalMutex(void) {
    pthread_mutex_t *mutex;
    pthread_mutexattr_t mutex_attr;
    int fd;
    char buffer[40] = "/dev/shm/";
    fd = shm_open(c_named_mutex, O_RDWR | O_CREAT | O_EXCL, 0666);
    if (fd < 0) {
        if (errno != EEXIST) {
            Log::error(42, "[COMPARE] Cannot initialize global mutex named: %s. What: %s", c_named_mutex, strerror(errno));
            return -1;
        }
        Log::debug("[COMPARE] Global mutex already initialized");
        return 0;
    }
    chmod(strncat(buffer,c_named_mutex,30),0666);
    ftruncate(fd, sizeof(pthread_mutex_t));
    mutex = (pthread_mutex_t *)mmap(NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&mutex_attr, PTHREAD_MUTEX_ROBUST);
    pthread_mutex_init(mutex, &mutex_attr);
    pthread_mutexattr_destroy(&mutex_attr);
    Log::debug("Mutex initialized");
    return 0;
}

/**
 * @brief Destroy the global mutex and unlink the shared memory
 * @return 0 if successful, -1 when it fails
 */
int
destroyGlobalMutex(void) {
    pthread_mutex_t *mutex;
    int fd;

    fd = shm_open(c_named_mutex, O_RDWR, 0666);
    if (fd < 0) {
        Log::error(42, "Cannot destroy global mutex named: %s. What: %s", c_named_mutex, strerror(errno));
        return -1;
    }
    mutex = (pthread_mutex_t *)mmap(NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    pthread_mutex_destroy(mutex);
    shm_unlink(c_named_mutex);
    Log::debug("[COMPARE] Mutex destroyed");
    return 0;
}


CompareConf::CompareConf(): mCompareDisabled(false), mIsActive(false) {
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

    if( !gWriteInFile ){
        return APR_SUCCESS;
    }

    apr_pool_cleanup_register(pPool, NULL, apr_pool_cleanup_null, closeLogFile);
    return openLogFile(gFilePath, std::ofstream::out | std::ofstream::app);

}

apr_status_t
closeLogFile(void *) {

    gFile.close();
    return APR_SUCCESS;
}

apr_status_t openLogFile(const char * filepath,std::ios_base::openmode mode) {

    gFile.open(filepath,mode);
    if (!gFile.is_open()){
        Log::error(43,"[COMPARE] Couldn't open correctly the file");
        return 400; // to modify
    }
#if AP_SERVER_MINORVERSION_NUMBER==2
    if ( chown(filepath, unixd_config.user_id, unixd_config.group_id) < 0 ) {
       Log::error(528, "[COMPARE] Failed to change ownership of shared mem file %s to child user %s, error %d (%s)", filepath, unixd_config.user_name, errno, strerror(errno) );
#elif AP_SERVER_MINORVERSION_NUMBER==4
    if ( chown(filepath, ap_unixd_config.user_id, ap_unixd_config.group_id) < 0 ) {
       Log::error(528, "[COMPARE] Failed to change ownership of shared mem file %s to child user %s, error %d (%s)", filepath, ap_unixd_config.user_name, errno, strerror(errno) );
#else
#error "Unsupported Apache Version, only 2.2 or 2.4"
#endif
    }
    gFile.close();
    return APR_SUCCESS;
}

void
childInit(apr_pool_t *pPool, server_rec *pServer)
{
    if( gWriteInFile ){
        gFile.open(gFilePath, std::ofstream::out | std::ofstream::app );
        if (!gFile.is_open()){
            Log::error(43,"[COMPARE] Couldn't open correctly the file");
        }
    }
    createGlobalMutex();
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
const char* setHeaderList(cmd_parms* pParams, void* pCfg, const char* pListType, const char* pAffectedKey,const char* pValue) {
    if (!pValue || strlen(pValue) == 0) {
        return "Missing reg_ex value for the header";
    }

    if (!pAffectedKey || strlen(pAffectedKey) == 0) {
            return "Missing header value";
        }

    if (!pListType || strlen(pListType) == 0) {
        return "Missing the type list";
    }

    CompareConf *lConf = reinterpret_cast<CompareConf *>(pCfg);
    std::string lHeader(pAffectedKey);
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
 * @brief Enable/Disable the utilization of the libws-diff tools
 * @param pParams miscellaneous data
 * @param pCfg user data for the directory/location
 * @param pValue the value
 * @return NULL if parameters are valid, otherwise a string describing the error
 */
const char*
setDisableLibwsdiff(cmd_parms* pParams, void* pCfg, const char* pValue) {
    Log::init();
	CompareConf *lConf = reinterpret_cast<CompareConf *>(pCfg);
	lConf->mCompareDisabled= strcmp(pValue, "1")==0 || strcmp(pValue, "true")==0;
    if(lConf->mCompareDisabled){
    	Log::warn(42,"[COMPARE] The use of the diffing library \nlibws-diff has been disabled!");
    }
    return NULL;
}

const char* setDiffLogType(cmd_parms* pParams,
		void* pCfg,
		const char* pValue) {
	Log::init();
	CompareConf *lConf = reinterpret_cast<CompareConf *>(pCfg);

	if(strcasecmp(pValue,"multiline")==0){
    	lConf->mLogType=LibWsDiff::diffPrinter::diffTypeAvailable::MULTILINE;
	}else if(strcasecmp(pValue,"simplejson")==0){
    	lConf->mLogType=LibWsDiff::diffPrinter::diffTypeAvailable::JSON;
    }else{
    	lConf->mLogType=LibWsDiff::diffPrinter::diffTypeAvailable::UTF8JSON;
    }
    return NULL;
}

const char*
setCompareLog(cmd_parms* pParams, void* pCfg, const char* pType, const char* pValue) {

    if (!pType || strlen(pValue) == 0) {
            return "Missing log type";
        }

    if (!pValue || strlen(pValue) == 0) {
        return "Missing file path or facility";
    }

    if (strcmp("FILE", pType) == 0)
    {
        gWriteInFile = true;
        gFilePath = pValue;
    }
    else if(strcmp("SYSLOG", pType) == 0)
    {
        gLogFacility = std::string(pValue);
        gWriteInFile = false;
        //closes the log if it was initialized
        Log::close();
        //initializes the log
        Log::init(gLogFacility);
    }
    else
    {
        return "Invalid value for the log type";
    }

    return NULL;

}

const char*
setCompare(cmd_parms* pParams, void* pCfg, const char* pValue) {
    CompareConf *lConf = reinterpret_cast<CompareConf *>(pCfg);

    lConf->mIsActive= true;

#ifndef UNIT_TESTING
        if (!ap_find_linked_module(MOD_REWRITE_NAME)) {
            return "'mod_rewrite' is not loaded, Enable mod_rewrite to use mod_compare";
        }
#endif

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
        AP_INIT_NO_ARGS("Compare",
                      reinterpret_cast<const char *(*)()>(&setCompare),
                      0,
                      ACCESS_CONF,
                      "Activate mod_compare."),
        AP_INIT_TAKE2("CompareLog",
                      reinterpret_cast<const char *(*)()>(&setCompareLog),
                      0,
                      OR_ALL,
                      "Log to a facility instead of a file."),
		AP_INIT_TAKE1("DiffLogType",
					reinterpret_cast<const char *(*)()>(&setDiffLogType),
					0,
					OR_ALL,
					"Specify the output log type for differences (<json>,multiline)"),
        AP_INIT_TAKE1("DisableLibwsdiff",
                      reinterpret_cast<const char *(*)()>(&setDisableLibwsdiff),
                      0,
                      ACCESS_CONF,
                      "Disable the use of libws-diff tools. Print raw serialization of the data in the log file."),
        {0}
    };

#ifndef UNIT_TESTING
/*
static void insertInputFilter(request_rec *pRequest) {
    CompareConf *lConf = reinterpret_cast<CompareConf *>(ap_get_module_config(pRequest->per_dir_config, &compare_module));
    assert(lConf);
    if (lConf->mIsActive){
        ap_add_input_filter(gName, NULL, pRequest, pRequest->connection);
    }
}*/

static void insertOutputFilter(request_rec *pRequest) {
    CompareConf *lConf = reinterpret_cast<CompareConf *>(ap_get_module_config(pRequest->per_dir_config, &compare_module));
    assert(lConf);
    if (lConf->mIsActive){
        ap_add_output_filter(gNameOut, NULL, pRequest, pRequest->connection);
    }
}

static void insertOutputFilter2(request_rec *pRequest) {
    CompareConf *lConf = reinterpret_cast<CompareConf *>(ap_get_module_config(pRequest->per_dir_config, &compare_module));
    assert(lConf);
    if (lConf->mIsActive){
        ap_add_output_filter(gNameOut2, NULL, pRequest, pRequest->connection);
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
    ap_register_input_filter(gName, inputFilterHandler, NULL, AP_FTYPE_RESOURCE);
    // output filter of type AP_FTYPE_RESOURCE => only the body will be read ( the headers_out not set yet)
    ap_register_output_filter(gNameOut, outputFilterHandler, NULL, AP_FTYPE_RESOURCE);
    // output filter of type AP_FTYPE_CONNECTION => only the response header will be read
    ap_register_output_filter(gNameOut2, outputFilterHandler2, NULL, AP_FTYPE_TRANSCODE);
    // ap_hook_insert_filter(&insertInputFilter, NULL, NULL, APR_HOOK_FIRST);
    ap_hook_insert_filter(&insertOutputFilter, NULL, NULL, APR_HOOK_LAST);
    ap_hook_insert_filter(&insertOutputFilter2, NULL, NULL, APR_HOOK_LAST);

    ap_hook_translate_name(&translateHook, NULL, NULL, APR_HOOK_MIDDLE);
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
