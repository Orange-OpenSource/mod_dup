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
#include <boost/archive/text_oarchive.hpp>
#include <math.h>
#include <boost/tokenizer.hpp>
#include <iomanip>
#include <apache2/httpd.h>


const char *c_UNIQUE_ID = "UNIQUE_ID";
const size_t SECTION_SIZE_CHARS = 8 ;
unsigned const MAX_SECTION_SIZE  = pow(10, static_cast<double>(SECTION_SIZE_CHARS))-1;
const std::string DIFF_SEPARATOR("-------------------\n");

namespace CompareModule {

void
printRequest(request_rec *pRequest, std::string pBody)
{
    const char *reqId = apr_table_get(pRequest->headers_in, c_UNIQUE_ID);
    Log::debug("### Filtering a request with ID: %s, body size:%ld", reqId, pBody.size());
    Log::debug("### Uri:%s", pRequest->uri);
    Log::debug("### Request args: %s", pRequest->args);
}

void map2string(const std::map< std::string, std::string> &pMap, std::string &pString) {
    std::map< std::string, std::string>::const_iterator lIter;
    for ( lIter = pMap.begin(); lIter != pMap.end(); ++lIter )
    {
        pString += lIter->first + ": " + lIter->second + "\n";
    }
}

/**
 * @brief write response differences in a file
 * @param pReqInfo info of the original request
 */
void writeDifferences(const DupModule::RequestInfo &pReqInfo,const std::string& headerDiff,const std::string& bodyDiff, const double time )
{
    std::string lReqHeader;
    map2string( pReqInfo.mReqHeader, lReqHeader );
    std::stringstream diffLog;

    diffLog << "BEGIN NEW REQUEST DIFFERENCE n°: " << pReqInfo.mId ;
    if (time > 0){
    	diffLog << " / Elapsed time : " << time << "s";
    }
    diffLog << std::endl << lReqHeader << std::endl;
    diffLog << pReqInfo.mReqBody.c_str() << std::endl;
    diffLog << DIFF_SEPARATOR << headerDiff << std::endl;
    diffLog << DIFF_SEPARATOR << bodyDiff << std::endl;
    diffLog << "END DIFFERENCE n°:" << pReqInfo.mId << std::endl;
    diffLog.flush();

    if (gFile.is_open()){
        boost::lock_guard<boost::interprocess::named_mutex>  fileLock(gMutex);
        gFile << diffLog.str();
        gFile.flush();
    }
    else
    {
        Log::error(12, "File not correctly opened");
    }
}

/**
 * @brief write request body and header in file with serialized boost method
 * @param type specify custom information about the request (Request,Response,DupResponse,...)
 * @param header the header
 * @param body the body
 */
void writeSerializedRequest(const DupModule::RequestInfo& req)
{
    if (gFile.is_open()){
        boost::lock_guard<boost::interprocess::named_mutex>  fileLock(gMutex);
        boost::archive::text_oarchive oa(gFile);
        oa << req;
    }
    else
    {
        Log::error(12, "File not correctly opened");
    }
}

/**
 * @brief write the Cassandra differences in log file
 * @param pUniqueID the UNIQUE_ID of the request to check
 * @return true if there are differences, false otherwise
 */
bool writeCassandraDiff(std::string &pUniqueID)
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

    std::stringstream diffStr;

    diffStr << "FieldInfo differences for pUniqueID : " << pUniqueID << "\n";
    for(;lPairIter.first!=lPairIter.second;++lPairIter.first){
    	diffStr << lPairIter.first->second;
    }
    diffStr << DIFF_SEPARATOR;
    diffStr.flush();

    if (gFile.is_open()){
        boost::lock_guard<boost::interprocess::named_mutex>  fileLock(gMutex);
        gFile << diffStr.rdbuf();
        gFile.flush();
    }
    else {
        Log::error(12, "File not correctly opened");
    }

    lDiff.erase(pUniqueID);

    return true;
}

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

/*
 * Callback to iterate over the headers tables
 * Pushes a copy of key => value in a list
 */
int iterateOverHeadersCallBack(void *d, const char *key, const char *value) {
    std::map< std::string, std::string> *lHeader = reinterpret_cast< std::map< std::string, std::string> *>(d);

    (*lHeader)[std::string(key)] = std::string(value);

    return 1;
}

apr_status_t inputFilterHandler(ap_filter_t *pF, apr_bucket_brigade *pB, ap_input_mode_t pMode, apr_read_type_e pBlock, apr_off_t pReadbytes)
{
    apr_status_t lStatus = ap_get_brigade(pF->next, pB, pMode, pBlock, pReadbytes);
    if (lStatus != APR_SUCCESS) {
        return lStatus;
    }
    request_rec *pRequest = pF->r;
    if (!pRequest)
    {
        return lStatus;
    }

    const char *lDupType = apr_table_get(pRequest->headers_in, "Duplication-Type");
    if (( lDupType == NULL ) || ( strcmp("Response", lDupType) != 0) )
    {
        return lStatus;
    }

    if(pRequest->per_dir_config == NULL){
        return lStatus;
    }
    struct CompareConf *tConf = reinterpret_cast<CompareConf *>(ap_get_module_config(pRequest->per_dir_config, &compare_module));
    if (!tConf) {
            return lStatus; // SHOULD NOT HAPPEN
    }
    // No context? new request
    if (!pF->ctx) {
        // If there is no UNIQUE_ID in the request header copy thr Request ID generated in both headers
        const char* lID = apr_table_get(pRequest->headers_in, c_UNIQUE_ID);
        unsigned int lReqID;
        if( lID == NULL){
        lReqID = DupModule::getNextReqId();
        std::string reqId = boost::lexical_cast<std::string>(lReqID);
        apr_table_set(pRequest->headers_in, c_UNIQUE_ID, reqId.c_str());
        //apr_table_set(pRequest->headers_out, c_UNIQUE_ID, reqId.c_str());
        }
        else {
            //lReqID = boost::lexical_cast<unsigned int>(std::string(lID));
            //apr_table_set(pRequest->headers_out, c_UNIQUE_ID, lID);
        }

        DupModule::RequestInfo *info = new DupModule::RequestInfo(lReqID);
        ap_set_module_config(pRequest->request_config, &compare_module, (void *)info);

        // Backup of info struct in the request context
        pF->ctx = info;
    } else if (pF->ctx == (void *)1) {
        return lStatus;
    }

    DupModule::RequestInfo *lRI = static_cast<DupModule::RequestInfo *>(pF->ctx);
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

        lRI->mBody += std::string(lReqPart, lLength);
    }
    apr_brigade_cleanup(pB);

    lStatus =  deserializeBody(*lRI);
#ifndef UNIT_TESTING
    apr_table_set(pRequest->headers_in, "Content-Length",boost::lexical_cast<std::string>(lRI->mReqBody.size()).c_str());
    apr_brigade_write(pB, ap_filter_flush, pF, lRI->mReqBody.c_str(), lRI->mReqBody.length() );
#endif
    apr_table_do(&iterateOverHeadersCallBack, &(lRI->mReqHeader), pRequest->headers_in, NULL);
    printRequest(pRequest, lRI->mReqBody);
    return lStatus;
}


apr_status_t
outputFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade) {
    request_rec *pRequest = pFilter->r;
    // Truncate the log before writing if the URI is set to "comp_truncate"
    std::string lArgs( static_cast<const char *>(pRequest->uri) );
    if ( lArgs.find("comp_truncate") != std::string::npos){
        gFile.close();
        gFile.open(gFilePath, std::ofstream::out | std::ofstream::trunc );
        apr_brigade_cleanup(pBrigade);
        apr_table_set(pRequest->headers_out, "Content-Length", "0");
        return ap_pass_brigade(pFilter->next, pBrigade);
    }

    if (!pRequest || !pRequest->per_dir_config)
    {
        return ap_pass_brigade(pFilter->next, pBrigade);
    }

    const char *lDupType = apr_table_get(pRequest->headers_in, "Duplication-Type");
    if ( ( lDupType == NULL ) || ( strcmp("Response", lDupType) != 0) )
    {
        return ap_pass_brigade(pFilter->next, pBrigade);
    }

    struct CompareConf *tConf = reinterpret_cast<CompareConf *>(ap_get_module_config(pRequest->per_dir_config, &compare_module));
    if( tConf == NULL ){
    	return ap_pass_brigade(pFilter->next, pBrigade);
    }

    boost::scoped_ptr<DupModule::RequestInfo> req(reinterpret_cast<DupModule::RequestInfo*>(ap_get_module_config(pRequest->request_config, &compare_module)));
    ap_set_module_config(pRequest->request_config, &compare_module, NULL);

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
            std::string lUniqueID( apr_table_get(pRequest->headers_in, c_UNIQUE_ID) );
            writeCassandraDiff(lUniqueID);

            //write headers in Map
            apr_table_do(&iterateOverHeadersCallBack, &(req->mDupResponseHeader), pRequest->headers_out, NULL);

            std::string diffBody,diffHeader;
            if( tConf->mCompareDisabled){
            	writeSerializedRequest(*req);
            }else{
				clock_t start=clock();
				if(tConf->mCompHeader.retrieveDiff(req->mResponseHeader,req->mDupResponseHeader,diffHeader)){
					if (tConf->mCompBody.retrieveDiff(req->mResponseBody,req->mDupResponseBody,diffBody)){
						if(diffHeader.length()!=0 || diffBody.length()!=0){
							writeDifferences(*req,diffHeader,diffBody,double(clock() - start)/CLOCKS_PER_SEC);
						}
					}
				}
            }
            //we want to avoid to send the response body on the network
            apr_table_set(pRequest->headers_out, "Content-Length", "0");
            apr_brigade_cleanup(pBrigade);
            rv =  ap_pass_brigade(pFilter->next, pBrigade);
            pFilter->ctx = (void *) -1;
        }
    }

    return OK;
}

};
