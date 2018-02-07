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

#include "RequestInfo.hh"
#include <assert.h>
#include <iomanip>
#include <string.h>

namespace DupModule {

    namespace DuplicationType {
            // String representation of the Duplicationtype values
        const char* c_NONE =                        "NONE";
        const char* c_HEADER_ONLY =                 "HEADER_ONLY";
        const char* c_COMPLETE_REQUEST =            "COMPLETE_REQUEST";
        const char* c_REQUEST_WITH_ANSWER =         "REQUEST_WITH_ANSWER";
        /// Duplication type mismatch value error
        const char* c_ERROR_ON_STRING_VALUE =       "Invalid Duplication Type Value. Supported Values: HEADER_ONLY | COMPLETE_REQUEST | REQUEST_WITH_ANSWER" ;

        eDuplicationType stringToEnum(const char *value) throw (std::exception){
            if (!strcmp(value, c_NONE)) {
                return NONE;
            }
            if (!strcmp(value, c_HEADER_ONLY)) {
                return HEADER_ONLY;
            }
            if (!strcmp(value, c_COMPLETE_REQUEST)) {
                return COMPLETE_REQUEST;
            }
            if (!strcmp(value, c_REQUEST_WITH_ANSWER)) {
                return REQUEST_WITH_ANSWER;
            }
            throw std::exception();
        }

    };



void RequestInfo::eos_seen(bool valToSet) {
    // Compute time elapsed to first eos set
    if (!mEOS && valToSet) {
        mElapsedTime = boost::posix_time::microsec_clock::universal_time() - mStartTime;
    }
    mEOS = valToSet;
}

RequestInfo::RequestInfo(std::string id, const std::string &pConfPath, const std::string &pPath,
                         const std::string &pArgs, const std::string *pBody)
    : mPoison(false),
      mId(id),
      mPath(pPath),
      mArgs(pArgs),
      mCurlCompResponseStatus(-1),
      mValidationHeaderDup(false),
      mValidationHeaderComp(false),
      mConf(nullptr),
      mEOS(false),
      mStartTime(boost::posix_time::microsec_clock::universal_time()),
      mElapsedTime()
      {
    if (pBody)
        mBody = *pBody;
}

RequestInfo::RequestInfo(const mapStr &reqHeader, const std::string &reqBody, const mapStr &respHeader,
                         const std::string &respBody, const mapStr &dupHeader, const std::string &dupBody):
	mPoison(false),
	mReqHeader(reqHeader),
	mReqBody(reqBody),
	mResponseHeader(respHeader),
	mResponseBody(respBody),
	mDupResponseHeader(dupHeader),
	mDupResponseBody(dupBody),
	mCurlCompResponseStatus(-1),
	mValidationHeaderDup(false),
	mValidationHeaderComp(false),
	mConf(nullptr),
    mEOS(false),
    mStartTime(boost::posix_time::microsec_clock::universal_time()),
    mElapsedTime()
{
}

RequestInfo::RequestInfo(const std::string &id, int64_t startTime)
    : mPoison(false),
      mId(id),
      mCurlCompResponseStatus(-1),
      mValidationHeaderDup(false),
      mValidationHeaderComp(false),
      mConf(nullptr),
      mEOS(false)
{
          namespace pt = boost::posix_time;
          mStartTime = pt::from_time_t(time_t(startTime / 1000000)) + pt::microseconds(startTime % 1000000);
}

RequestInfo::RequestInfo() :
    mPoison(true),
    mCurlCompResponseStatus(-1),
    mValidationHeaderDup(false),
    mValidationHeaderComp(false),
    mConf(nullptr),
    mEOS(false),
    mStartTime(boost::posix_time::microsec_clock::universal_time()),
    mElapsedTime()
    {
}

std::string RequestInfo::flatten(const tKeyValList &kvl, std::string sep)
{
    std::string out;
    for( const auto &item : kvl ) {
        out += item.first;
        out += sep;
        out += item.second;
    }
    return out;
}


bool
RequestInfo::isPoison() const {
    return mPoison;
}

bool
RequestInfo::hasBody() const {
    return !mBody.empty();
}

void
RequestInfo::Serialize(const std::string &toSerialize, std::stringstream &ss) {
    ss << std::setfill('0') << std::setw(8) << toSerialize.length() << toSerialize;
}

int
RequestInfo::getElapsedTimeMS() const {
    return mElapsedTime.total_milliseconds();
}


}
