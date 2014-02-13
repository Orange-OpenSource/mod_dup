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


#include <sys/syscall.h>
#include <boost/algorithm/string.hpp>

#include "Utils.hh"
#include "Log.hh"

namespace DupModule {

extern unsigned int getNextReqId();

/*
 * Returns the next random request ID
 * method is reentrant
 */
unsigned int getNextReqId() {
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

}
