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

#include "Log.hh"

#define SYSLOG_NAMES
#include <sys/syslog.h>


#include <cstdarg>
#include <strings.h>
#include <stdio.h>
#include <map>
#include <boost/algorithm/string/case_conv.hpp>

/// Pointer to the only instance of this class
Log* Log::gInstance = 0;
int Log::gLogLevel = LOG_DEBUG;


const char *Log::STRDEBUG = "debug";
const char *Log::STRINFO = "info";
const char *Log::STRNOTICE = "notice";
const char *Log::STRWARN = "warn";
const char *Log::STRERROR = "error";
const char *Log::STRCRIT = "crit";
const char *Log::IDENT = "DRPCTL";

typedef std::map< std::string, int> tStrToFacility;

/// @brief set the mapping of string to facility at startup
static tStrToFacility setStrToFacility()
{
    tStrToFacility strToFacility;

    strToFacility["LOCAL0"] = LOG_LOCAL0;
    strToFacility["LOCAL1"] = LOG_LOCAL1;
    strToFacility["LOCAL2"] = LOG_LOCAL2;
    strToFacility["LOCAL3"] = LOG_LOCAL3;
    strToFacility["LOCAL4"] = LOG_LOCAL4;
    strToFacility["LOCAL5"] = LOG_LOCAL5;
    strToFacility["LOCAL6"] = LOG_LOCAL6;
    strToFacility["LOCAL7"] = LOG_LOCAL7;

    return strToFacility;
}


/// map between a type name and its uid
static tStrToFacility gStrToFacility = setStrToFacility();

int strToFacility(const std::string & facility) {
    tStrToFacility::const_iterator iter = gStrToFacility.find(boost::algorithm::to_upper_copy(facility));
    if ( iter == gStrToFacility.end() ) {
        return LOG_LOCAL2;
    }
    return iter->second;
}


/// @brief Init the Log. Does not call openlog as this would affect other apache modules.
/// @param pFacility the "device" which handles application messages
Log::Log(int pFacility) : mFacility(pFacility)
{
}

/// @brief Init the Log
/// @param pFacility the "device" which handles application messages
void Log::init(int pFacility)
{
    if( ! gInstance ){
        // create sole instance
        gInstance = new Log(pFacility);
    }
}

/// @brief Init the Log
/// @param pFacility the "device" which handles application messages
void Log::init(const std::string& pFacility)
{
    init(strToFacility(pFacility));
}

/// @brief Init the Log
void Log::init()
{
    init(LOG_LOCAL2);
}

/// @brief Close Log
void Log::close()
{
    // is log initialized?
    if(gInstance){
        delete gInstance;
        // Set gInstance to zero in order to be able to re-init Log instance
        gInstance = 0;
    }
}

/// @brief default destructor, clear the message map and close log
Log::~Log()
{
    // close log
    closelog();
}

const char * Log::stringLevel(int pLevel) {
    switch ( pLevel ) {
    case LOG_DEBUG:
        return STRDEBUG;
    case LOG_INFO:
        return STRINFO;
    case LOG_NOTICE:
        return STRNOTICE;
    case LOG_WARNING:
        return STRWARN;
    case LOG_ERR:
        return STRERROR;
    case LOG_CRIT:
        return STRCRIT;
    default:
        return STRDEBUG;
    }
}

#define LOG_LENGTH 16384

#define VLOG(level, code, msg) va_list l_ap;\
        char longmsg[LOG_LENGTH];\
        snprintf(longmsg, LOG_LENGTH,"code:%d - %s", code, msg); \
        va_start(l_ap, msg); \
        va_list l_ap2;\
        va_copy(l_ap2, l_ap);\
        vsyslog(level|gInstance->mFacility, msg, l_ap); \
        char longmsg2[LOG_LENGTH];\
        if ( level <= gLogLevel ) { \
        snprintf(longmsg2, LOG_LENGTH,"[%s] code:%d - %s\n", Log::stringLevel(level), code, msg); \
        vprintf(longmsg2, l_ap2); } \
        va_end(l_ap2);\
        va_end(l_ap);\

        
/**
 * logs a debug with message parameters <...>
 * pMsg: the message
 */
void Log::debug(const char* pMsg, ...)
{
#ifdef DEBUG
    VLOG((LOG_DEBUG), 0, pMsg);
#endif
}
/**
 * logs an info with message parameters <...>
 * pMsg: the message
 */
void Log::info(int pCode, const char* pMsg, ...)
{
    VLOG((LOG_INFO), pCode, pMsg);
}
/**
 * logs a notice with message parameters <...>
 * pMsg: the message
 */
void Log::notice(int pCode, const char* pMsg, ...)
{
    VLOG((LOG_NOTICE), pCode, pMsg);
}
/**
 * logs a warning with message parameters <...>
 * pMsg: the message
 */
void Log::warn(int pCode, const char* pMsg, ...)
{
    VLOG((LOG_WARNING), pCode, pMsg);
}

/**
 * logs an error with message parameters <...>
 * pMsg: the error message
 */
void Log::error(int pCode, const char* pMsg, ...)
{
    VLOG((LOG_ERR), pCode, pMsg);
}
/**
 * logs a crit with message parameters <...>
 * pMsg: the message
 */
void Log::crit(int pCode, const char* pMsg, ...)
{
    VLOG((LOG_CRIT), pCode, pMsg);
}
