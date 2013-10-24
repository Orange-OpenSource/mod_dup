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

#pragma once

#include <string>

/// @brief logging utility.
/// syslog calls are wrapped
/// @see man syslog
class Log
{
public:
    /***
    *  Init the Log
    *  pFacility: the "device" which handles application messages
    */
    static void init(int pFacility);

    /// @brief Init the Log
    static void init();

    // Close Log
    static void close();

    /***
     * logs a debug with message parameters <...>
     * pMsg: the message
     */
    static void debug(const char* pMsg, ...) __attribute__ ((format (printf, 1, 2)));
    /***
     * logs an info with message parameters <...>
     * pMsg: the message
     */
    static void info(int pCode, const char* pMsg, ...) __attribute__ ((format (printf, 2, 3)));
    /***
     * logs a notice with message parameters <...>
     * pMsg: the message
     */
    static void notice(int pCode, const char* pMsg, ...) __attribute__ ((format (printf, 2, 3)));
    /***
     * logs a warning with message parameters <...>
     * pMsg: the message
     */
    static void warn(int pCode, const char* pMsg, ...) __attribute__ ((format (printf, 2, 3)));
    /***
     * logs an error with message parameters <...>
     * pMsg: the message
     */
    static void error(int pCode, const char* pMsg, ...) __attribute__ ((format (printf, 2, 3)));
    /***
     * logs a crit with message parameters <...>
     * pMsg: the message
     */
    static void crit(int pCode, const char* pMsg, ...) __attribute__ ((format (printf, 2, 3)));

    static const char * stringLevel(int pLevel);

protected:
    /// @brief pointer to the only instance of this class
    static Log* gInstance;

    static int gLogLevel;

    static const char *STRDEBUG;
    static const char *STRINFO;
    static const char *STRNOTICE;
    static const char *STRWARN;
    static const char *STRERROR;
    static const char *STRCRIT;
    static const char *IDENT;

	const int mFacility;

    /***
     * Log's constructor, assignment operator, and copy constructor are declared protected
     * to ensure that users can't create local instances of the class
     */
    Log(int pFacility);
    Log(const Log&);
    Log& operator= (const Log&);

    virtual ~Log();


public:


};
