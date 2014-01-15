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

#include <assert.h>
#include "RequestInfo.hh"

namespace DupModule {

RequestInfo::RequestInfo(unsigned int id, const std::string &pConfPath, const std::string &pPath, const std::string &pArgs, const std::string *pBody)
    : mPoison(false),
      mId(id),
      mConfPath(pConfPath),
      mPath(pPath),
      mArgs(pArgs) {
    if (pBody)
        mBody = *pBody;
}

RequestInfo::RequestInfo(unsigned int id)
    : mPoison(false),
      mId(id) {
}

RequestInfo::RequestInfo() :
    mPoison(true) {}

bool
RequestInfo::isPoison() const {
    return mPoison;
}


}
