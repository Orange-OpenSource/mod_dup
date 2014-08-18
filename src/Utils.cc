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
#include <boost/lexical_cast.hpp>

#include "Utils.hh"
#include "Log.hh"

#include <http_request.h>

namespace CommonModule {

const char* c_UNIQUE_ID = "UNIQUE_ID";
const unsigned int CMaxBytes = 8192;

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

bool
extractBrigadeContent(apr_bucket_brigade *bb, ap_filter_t *pF, std::string &content) {
    if (ap_get_brigade(pF,
                       bb, AP_MODE_READBYTES, APR_BLOCK_READ, CMaxBytes) != APR_SUCCESS) {
      Log::error(42, "Get brigade failed, skipping the rest of the body");
      return true;
    }
    // Read brigade content
    for (apr_bucket *b = APR_BRIGADE_FIRST(bb);
     b != APR_BRIGADE_SENTINEL(bb);
     b = APR_BUCKET_NEXT(b) ) {
      // Metadata end of stream
      if (APR_BUCKET_IS_EOS(b)) {
          return true;
      }
      if (APR_BUCKET_IS_METADATA(b))
          continue;
      const char *data = 0;
      apr_size_t len = 0;
      apr_status_t rv = apr_bucket_read(b, &data, &len, APR_BLOCK_READ);
      if (rv != APR_SUCCESS) {
    Log::error(42, "Bucket read failed, skipping the rest of the body");
    return true;
      }
      if (len) {
          content.append(data, len);
      }
    }
    return false;
}

std::string getOrSetUniqueID(request_rec *pRequest) {
    // If there is no UNIQUE_ID in the request header copy thr Request ID generated in both headers
    const char* lID = apr_table_get(pRequest->headers_in, CommonModule::c_UNIQUE_ID);
    unsigned int lReqID;
    if( lID == NULL){
        // Not defined in a header
        lReqID = CommonModule::getNextReqId();
        std::string reqId = boost::lexical_cast<std::string>(lReqID);
        if (pRequest->headers_in)
            apr_table_set(pRequest->headers_in, CommonModule::c_UNIQUE_ID, reqId.c_str());
        if (pRequest->headers_out)
            apr_table_set(pRequest->headers_out, CommonModule::c_UNIQUE_ID, reqId.c_str());
        return reqId;
    } else {
        // Already defined, we redefine it in headers_out
        if (pRequest->headers_out)
            apr_table_set(pRequest->headers_out, CommonModule::c_UNIQUE_ID, lID);
    }
    return std::string(lID);
}

}

