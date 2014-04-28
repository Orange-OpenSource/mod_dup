/*
* mod_compare - compare apache requests
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

#include "mod_compare.hh"
#include "RequestInfo.hh"
#include "Utils.hh"

#include <http_config.h>
#include <assert.h>
#include <stdexcept>
#include <boost/thread/detail/singleton.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <math.h>
#include <boost/tokenizer.hpp>
#include <iomanip>
#include <apache2/httpd.h>
#include <boost/date_time/time_facet.hpp>

const size_t SECTION_SIZE_CHARS = 8 ;
unsigned const MAX_SECTION_SIZE  = pow(10, static_cast<double>(SECTION_SIZE_CHARS))-1;

namespace CompareModule {

/**
 * @brief convert a substring of pString in size_t
 * @param pString the string from which to extract a substring to convert
 * @param pLength the length calculated
 * @return true if the conversion gets success, false otherwise
 */
size_t getLength(const std::string pString, const size_t pFirst)
{
	size_t res;
    try
    {
        res =   boost::lexical_cast<unsigned int>( pString.substr(pFirst,SECTION_SIZE_CHARS));
    }
    catch (boost::bad_lexical_cast & e)
    {
    	Log::error(12, "Invalid size value");
    	throw e;
    }
    if( res > MAX_SECTION_SIZE)
    {
        Log::error(12, "Value of length out of range");
        throw std::out_of_range("Value of length out of range");
    }
    return res;
}

/**
 * @brief extract the request body, the header answer and the response answer
 * @param pReqInfo info of the original request
 * @return a http status
 */
apr_status_t deserializeBody(DupModule::RequestInfo &pReqInfo)
{
    size_t lBodyReqSize, lHeaderResSize, lBodyResSize;
    size_t pos;
    std::string lResponseHeader;
    if ( pReqInfo.mBody.size() < 3*SECTION_SIZE_CHARS )
    {
        Log::error(11, "Unexpected body format");
        Log::error(13, "Current body size: %d", static_cast<int>(pReqInfo.mBody.size()));
        return HTTP_BAD_REQUEST;
    }
    try {
    	pos=0;
    	lBodyReqSize = getLength( pReqInfo.mBody, pos);
    	pos+= SECTION_SIZE_CHARS;
    	pReqInfo.mReqBody = pReqInfo.mBody.substr(pos,lBodyReqSize);
    	pos+=lBodyReqSize;

    	lHeaderResSize = getLength( pReqInfo.mBody,pos);
    	pos+= SECTION_SIZE_CHARS;
    	lResponseHeader = pReqInfo.mBody.substr(pos,lHeaderResSize);
    	pos+=lHeaderResSize;

    	lBodyResSize = getLength( pReqInfo.mBody, pos);
    	pos+= SECTION_SIZE_CHARS;
    	pReqInfo.mResponseBody = pReqInfo.mBody.substr(pos, lBodyResSize);

        Log::error(42, "Deserialized sizes: BodyReq:%ld Header:%ld Bodyres:%ld ", lBodyReqSize, lHeaderResSize, lBodyResSize);
    	deserializeHeader(pReqInfo,lResponseHeader);
    }
    catch ( const std::out_of_range &oor)
    {
        Log::error(13, "Out of range error: %s", oor.what());
        return HTTP_BAD_REQUEST;
    }catch (boost::bad_lexical_cast & e ){
    	return HTTP_BAD_REQUEST;
    }

	return OK;
}
/**
 * @brief extract the request body, the header answer and the response answer
 * @param pReqInfo info of the original request
 * @return a http status
 */
apr_status_t deserializeHeader(DupModule::RequestInfo &pReqInfo,const std::string& header)
{
	//deserialize the response header in a map
	std::string lDelim(": ");
	std::string lLine;
	std::stringstream stmHeader;
	stmHeader << header;
	while (std::getline(stmHeader, lLine) )
	{
		size_t lPos = 0;
		lPos = lLine.find(lDelim);
		if (lPos == std::string::npos)
		{
			Log::error(13,"Invalid Header format" );
			throw std::out_of_range("Invalid Header format");
		}
		std::string lKey = lLine.substr( 0, lPos );
		std::string lValue = lLine.substr( lPos + lDelim.size(), lLine.size() - lPos - lDelim.size() );
		pReqInfo.mResponseHeader[lKey] = lValue;
	}

	return OK;
}
}
