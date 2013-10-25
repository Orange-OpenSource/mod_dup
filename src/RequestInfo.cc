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

namespace DupModule {

/**
 * @brief Constructs the object using the three strings.
 * @param pConfPath The location (in the conf) which matched this query
 * @param pPath The path part of the request
 * @param pConfPath The parameters part of the query (without leading ?)
 */
    RequestInfo::RequestInfo(const std::string &pConfPath, const std::string &pPath, const std::string &pArgs, const std::string *pBody) :
		mPoison(false),
		mConfPath(pConfPath),
		mPath(pPath),
		mArgs(pArgs){
        if (pBody)
            mBody = *pBody;
}

/**
 * @brief Constructs a poisonous object causing the processor to stop when read
 */
RequestInfo::RequestInfo() :
		mPoison(true) {}

/**
 * @brief Returns wether the the request is poisonous
 * @return true if poisonous, false otherwhise
 */
bool
RequestInfo::isPoison() {
	return mPoison;
}

}
