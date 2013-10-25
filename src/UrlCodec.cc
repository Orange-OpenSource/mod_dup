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

#include <httpd.h>
#include <boost/algorithm/string/replace.hpp>

#include "Log.hh"
#include "UrlCodec.hh"

namespace DupModule {

class ApacheUrlCodec : public IUrlCodec
{
public:
	/**
	 * @brief Helper function to decode queries
	 * @param pIn string to be decoded
	 * @return decoded string
	 */
	const std::string
	decode(const std::string &pIn) const {
		char *lBuffer = new char[pIn.size()+1];
		strncpy(lBuffer, pIn.c_str(), pIn.size()+1);
		if (ap_unescape_url(lBuffer) == HTTP_BAD_REQUEST) {
			Log::warn(302, "Bad escape values in request: %s", lBuffer);
		}
		std::string lOut(lBuffer);
		delete[] lBuffer;

		return lOut;
	}

	/**
	 * @brief Helper function to encode queries
	 * @param pIn string to be encoded
	 * @return encoded string
	 */
	const std::string
	encode(apr_pool_t *pPool, const std::string &pIn) const {
		// ap_escape_path_segment returns a char * which is allocated in the apache pool
		// No need to free it, as apache will destroy the whole pool
		return std::string(ap_escape_path_segment(pPool, pIn.c_str()));
	}
};

class DefaultUrlCodec : public IUrlCodec
{
public:
	/**
	 * @brief Helper function to decode queries
	 * @param pIn string to be decoded
	 * @return decoded string
	 */
	const std::string
	decode(const std::string &pIn) const {
		std::string preDecoded = boost::replace_all_copy(pIn, "+", " ");
		char *lBuffer = new char[preDecoded.size()+1];
		strncpy(lBuffer, preDecoded.c_str(), preDecoded.size()+1);
		if (ap_unescape_url(lBuffer) == HTTP_BAD_REQUEST) {
			Log::warn(302, "Bad escape values in request: %s", lBuffer);
		}
		std::string lOut(lBuffer);
		delete[] lBuffer;

		return lOut;
	}

	/**
	 * @brief Helper function to encode queries
	 * @param pIn string to be encoded
	 * @return encoded string
	 */
	const std::string
	encode(apr_pool_t *pPool, const std::string &pIn) const {
		// ap_escape_path_segment returns a char * which is allocated in the apache pool
		// No need to free it, as apache will destroy the whole pool
		std::string encoded(ap_escape_path_segment(pPool, pIn.c_str()));
		return boost::replace_all_copy(encoded, "+", "%2b");
	}
};

const IUrlCodec *
getUrlCodec(const std::string pUrlCodec)
{
	if (pUrlCodec == "apache") {
		return new ApacheUrlCodec();
	} else {
		return new DefaultUrlCodec();
	}
}

}
