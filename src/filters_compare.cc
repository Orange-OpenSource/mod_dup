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
#include "CassandraDiff.h"
#include "Utils.hh"

#include <http_config.h>
#include <assert.h>
#include <stdexcept>
#include <boost/thread/detail/singleton.hpp>
#include <math.h>
#include <boost/tokenizer.hpp>

const char *c_UNIQUE_ID = "UNIQUE_ID";
const size_t SECTION_SIZE_CHARS = 8 ;
unsigned const MAX_SECTION_SIZE  = pow(10, static_cast<double>(SECTION_SIZE_CHARS))-1;

namespace CompareModule {


void
printRequest(request_rec *pRequest, std::string pBody)
{
    const char *reqId = apr_table_get(pRequest->headers_in, c_UNIQUE_ID);
    Log::debug("### Filtering a request with ID: %s, body size:%ld", reqId, pBody.size());
    Log::debug("### Uri:%s", pRequest->uri);
    Log::debug("### Request args: %s", pRequest->args);
}

/**
 * @brief write response differences in a file
 * @param
 */
void writeDifferences()
{

}

/**
 * @brief check if there is a difference for a set in Cassandra
 * @param pUniqueID the UNIQUE_ID of the request to check
 * @return true if there are differences, false otherwise
 */
bool checkCassandraDiff(std::string &pUniqueID)
{
    typedef std::multimap<std::string, CassandraDiff::FieldInfo> tMultiMapDiff;

    CassandraDiff::Differences & lDiff = boost::detail::thread::singleton<CassandraDiff::Differences>::instance();
    boost::lock_guard<boost::mutex>  lLock(lDiff.getMutex());

    std::pair <tMultiMapDiff::iterator, tMultiMapDiff::iterator> lPairIter;
    lPairIter = lDiff.equal_range(pUniqueID);
    if ( lPairIter.first ==  lPairIter.second )
    {
        return false;
    }

    writeDifferences();
    lDiff.erase(pUniqueID);

    return true;

}

/**
 * @brief convert a substring of pString in size_t
 * @param pString the string from which to extract a substring to convert
 * @param pLength the length calculated
 * @return true if the conversion gets success, false otherwise
 */
bool getLength(const std::string pString, const size_t pFirst, size_t &pLength )
{
    try
    {
        pLength =   boost::lexical_cast<unsigned int>( pString.substr(pFirst,SECTION_SIZE_CHARS));
    }
    catch (boost::bad_lexical_cast &)
    {
        Log::error(12, "Invalid size value");
        return false;
    }

    if( pLength > MAX_SECTION_SIZE)
    {
        Log::error(12, "Value of length out of range");
        return false;
    }

    return true;
}

/**
 * @brief extract the request body, the header answer and the response answer
 * @param pReqInfo info of the original request
 * @param lReqBody deserialized body to pass on to the next apache module
 * @return a http status
 */
apr_status_t deserializeBody(DupModule::RequestInfo &pReqInfo, std::string &lReqBody)
{
    int BAD_REQUEST = 400;
    size_t lBodyReqSize, lHeaderResSize, lBodyResSize;
    std::string lResponseHeader;
    if ( pReqInfo.mBody.size() < 3*SECTION_SIZE_CHARS )
    {
        Log::error(11, "Unexpected body format");
        return BAD_REQUEST;
    }
    if ( !getLength( pReqInfo.mBody, 0, lBodyReqSize ) )
    {
        return BAD_REQUEST;
    }
    if ( !getLength( pReqInfo.mBody, SECTION_SIZE_CHARS+lBodyReqSize, lHeaderResSize ))
    {
        return BAD_REQUEST;
    }
    lResponseHeader = pReqInfo.mBody.substr(2*SECTION_SIZE_CHARS + lBodyReqSize,lHeaderResSize);

    if ( !getLength( pReqInfo.mBody, 2*SECTION_SIZE_CHARS + lBodyReqSize + lHeaderResSize, lBodyResSize ) )
    {
        return BAD_REQUEST;
    }
    try
    {
        lReqBody = pReqInfo.mBody.substr(SECTION_SIZE_CHARS,lBodyReqSize);
        lResponseHeader = pReqInfo.mBody.substr(2*SECTION_SIZE_CHARS + lBodyReqSize,lHeaderResSize);
        pReqInfo.mResponseBody = pReqInfo.mBody.substr(3*SECTION_SIZE_CHARS + lBodyReqSize +  lHeaderResSize, lBodyResSize);
    }
    catch ( const std::out_of_range &oor)
    {
        Log::error(13, "Out of range error: %s", oor.what());
        return BAD_REQUEST;
    }


    //deserialize the response header in a map
    std::string lDelim(": ");
    std::string lLine;
    std::stringstream lHeader;
    lHeader << lResponseHeader;
    while (std::getline(lHeader, lLine) )
    {
        size_t lPos = 0;
        lPos = lLine.find(lDelim);
        if (lPos == std::string::npos)
        {
            Log::error(13, "Invalid Header format ");
            return BAD_REQUEST;
        }
        std::string lKey = lLine.substr( 0, lPos );
        std::string lValue = lLine.substr( lPos + lDelim.size(), lLine.size() - lPos - lDelim.size() );
        pReqInfo.mResponseHeader[lKey] = lValue;
    }

    return OK;

}

apr_status_t
inputFilterHandler(ap_filter_t *pF, apr_bucket_brigade *pB, ap_input_mode_t pMode, apr_read_type_e pBlock, apr_off_t pReadbytes)
{
    apr_status_t lStatus = ap_get_brigade(pF->next, pB, pMode, pBlock, pReadbytes);
    if (lStatus != APR_SUCCESS) {
        return lStatus;
    }
    request_rec *pRequest = pF->r;
    if (!pRequest)
    {
        return DECLINED;
    }

    const char *lDupType = apr_table_get(pRequest->headers_in, "Duplication-Type");
    if (( lDupType == NULL ) || ( strcmp("answer", lDupType) != 0) )
    {
        return DECLINED;
    }


    struct CompareConf *tConf = reinterpret_cast<CompareConf *>(ap_get_module_config(pRequest->per_dir_config, &compare_module));
    if (!tConf) {
            return DECLINED; // SHOULD NOT HAPPEN
    }
    // No context? new request
    if (!pF->ctx) {
        DupModule::RequestInfo *info = new DupModule::RequestInfo(getNextReqId());
        ap_set_module_config(pRequest->request_config, &compare_module, (void *)info);
        // Copy Request ID in both headers
        std::string reqId = boost::lexical_cast<std::string>(info->mId);
        apr_table_set(pRequest->headers_in, c_UNIQUE_ID, reqId.c_str());
        apr_table_set(pRequest->headers_out, c_UNIQUE_ID, reqId.c_str());
        // Backup of info struct in the request context
        pF->ctx = info;
    } else if (pF->ctx == (void *)1) {
        return OK;
    }

    DupModule::RequestInfo *pBH = static_cast<DupModule::RequestInfo *>(pF->ctx);
    for (apr_bucket *b = APR_BRIGADE_FIRST(pB);
         b != APR_BRIGADE_SENTINEL(pB);
         b = APR_BUCKET_NEXT(b) ) {
        // Metadata end of stream
        if ( APR_BUCKET_IS_EOS(b) ) {
            pF->ctx = (void *)1;
            break;
        }
        const char* lReqPart = NULL;
        apr_size_t lLength = 0;
        apr_status_t lStatus = apr_bucket_read(b, &lReqPart, &lLength, APR_BLOCK_READ);
        if ((lStatus != APR_SUCCESS) || (lReqPart == NULL)) {
            continue;
        }
        pBH->mBody += std::string(lReqPart, lLength);
    }
    apr_brigade_cleanup(pB);

    std::string lReqBody;
    lStatus =  deserializeBody(*pBH, lReqBody);
    std::stringstream lStringSize;
    lStringSize << lReqBody.size();
#ifndef UNIT_TESTING
    apr_table_set(pRequest->headers_in, "Content-Length", lStringSize.str().c_str());
    apr_brigade_write(pB, ap_filter_flush, pF, lReqBody.c_str(), lReqBody.length() );
#endif
    printRequest(pRequest, lReqBody);

    return lStatus;
}

/*
 * Callback to iterate over the headers tables
 * Pushes a copy of key => value in a list
 */
static int iterateOverHeadersCallBack(void *d, const char *key, const char *value) {
    std::map< std::string, std::string> *lResHeaderLocalWS = reinterpret_cast< std::map< std::string, std::string> *>(d);

    (*lResHeaderLocalWS)[std::string(key)] = std::string(value);

    return 1;
}

apr_status_t
outputFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade) {
    request_rec *pRequest = pFilter->r;
    if (!pRequest || !pRequest->per_dir_config)
    {
        return ap_pass_brigade(pFilter->next, pBrigade);
    }

    const char *lDupType = apr_table_get(pRequest->headers_in, "Duplication-Type");
    if ( ( lDupType == NULL ) || ( strcmp("answer", lDupType) != 0) )
    {
        return DECLINED;
    }

    struct CompareConf *tConf = reinterpret_cast<CompareConf *>(ap_get_module_config(pRequest->per_dir_config, &compare_module));
    assert(tConf);

    if (pFilter->ctx == (void *) -1)
    {
        apr_brigade_cleanup(pBrigade);
        apr_table_set(pRequest->headers_out, "Content-Length", "0");
        return ap_pass_brigade(pFilter->next, pBrigade);
    }

    DupModule::RequestInfo *req = reinterpret_cast<DupModule::RequestInfo *>(ap_get_module_config(pFilter->r->request_config, &compare_module));
    if ( req == NULL)
    {
        apr_brigade_cleanup(pBrigade);
        apr_table_set(pRequest->headers_out, "Content-Length", "0");
        return ap_pass_brigade(pFilter->next, pBrigade);
    }
    // Asynchronous push of request WITH the answer
    apr_bucket *currentBucket;
    while ((currentBucket = APR_BRIGADE_FIRST(pBrigade)) != APR_BRIGADE_SENTINEL(pBrigade))
    {
        const char *data;
        apr_size_t len;
        apr_status_t rv;
        rv = apr_bucket_read(currentBucket, &data, &len, APR_BLOCK_READ);

        if ((rv == APR_SUCCESS) && (data != NULL))
        {
            req->mDupResponseBody.append(data, len);
        }
        // Remove bucket e from bb.
        APR_BUCKET_REMOVE(currentBucket);

        if (APR_BUCKET_IS_EOS(currentBucket))
        {
            apr_brigade_cleanup(pBrigade);
            apr_table_set(pRequest->headers_out, "Content-Length", "0");
            rv =  ap_pass_brigade(pFilter->next, pBrigade);
            pFilter->ctx = (void *) -1;

            std::string lUniqueID( apr_table_get(pRequest->headers_in, c_UNIQUE_ID) );
            checkCassandraDiff(lUniqueID);

            //check headers
            apr_table_do(&iterateOverHeadersCallBack, &(req->mDupResponseHeader), pRequest->headers_out, NULL);
            // call the comparison function for the header (map, map)
            // write differences
            // call the comparison function for the body
            // write differences
        }
    }

    return OK;
}

};
