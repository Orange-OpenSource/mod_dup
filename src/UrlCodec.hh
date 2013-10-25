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
#include <apr_pools.h>


namespace DupModule {

class IUrlCodec
{
public:
	virtual const std::string decode(const std::string &pIn) const = 0;
	virtual const std::string encode(apr_pool_t *pPool, const std::string &pIn) const = 0;
};

const IUrlCodec *
getUrlCodec(const std::string pUrlCodec="default");

}
