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

RequestInfo::RequestInfo(std::string id, const std::string &pConfPath, const std::string &pPath, const std::string &pArgs, const std::string *pBody)
    : mPoison(false),
      mId(id),
      mConfPath(pConfPath),
      mPath(pPath),
      mArgs(pArgs),
      eos_seen(false){
    if (pBody)
        mBody = *pBody;
}

RequestInfo::RequestInfo(mapStr reqHeader,std::string reqBody,mapStr respHeader,std::string respBody,mapStr dupHeader,std::string dupBody):
	mReqHeader(reqHeader),
	mReqBody(reqBody),
	mResponseHeader(respHeader),
	mResponseBody(respBody),
	mDupResponseHeader(dupHeader),
	mDupResponseBody(dupBody),
        eos_seen(false){
}

RequestInfo::RequestInfo(std::string id)
    : mPoison(false),
      mId(id),
      eos_seen(false) {
}

RequestInfo::RequestInfo() :
    mPoison(true),
    eos_seen(false){
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

}
