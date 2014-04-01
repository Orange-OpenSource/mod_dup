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
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <boost/date_time/time_facet.hpp>


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
    boost::posix_time::time_facet *facet = new boost::posix_time::time_facet("%d-%b-%Y %H:%M:%S.%f");
    std::stringstream date_stream;
    diffLog.imbue(std::locale(std::cout.getloc(), facet));

    diffLog << "BEGIN NEW REQUEST DIFFERENCE n°: " << pReqInfo.mId ;
    if (time > 0){
    	diffLog << " / Elapsed time : " << time << "s";
    }
#ifndef UNIT_TESTING
    diffLog << std::endl << "Date : " << boost::posix_time::microsec_clock::local_time() <<std::endl;
#endif
    diffLog << std::endl << pReqInfo.mRequest.c_str() << std::endl;
    diffLog << std::endl << lReqHeader << std::endl;
    diffLog << pReqInfo.mReqBody.c_str() << std::endl;
    writeCassandraDiff( pReqInfo.mId, diffLog );
    diffLog << DIFF_SEPARATOR << headerDiff << std::endl;
    diffLog << DIFF_SEPARATOR << bodyDiff << std::endl;
    diffLog << "END DIFFERENCE n°:" << pReqInfo.mId << std::endl;
    diffLog.flush();

    if (gFile.is_open()){
        boost::lock_guard<boost::interprocess::named_mutex>  fileLock(getGlobalMutex());
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
        boost::lock_guard<boost::interprocess::named_mutex> fileLock(getGlobalMutex());
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
void writeCassandraDiff(const std::string &pUniqueID, std::stringstream &diffStr)
{
    typedef std::multimap<std::string, CassandraDiff::FieldInfo> tMultiMapDiff;

    CassandraDiff::Differences & lDiff = boost::detail::thread::singleton<CassandraDiff::Differences>::instance();
    boost::lock_guard<boost::mutex>  lLock(lDiff.getMutex());

    std::pair <tMultiMapDiff::iterator, tMultiMapDiff::iterator> lPairIter;
    lPairIter = lDiff.equal_range(pUniqueID);
    if ( lPairIter.first ==  lPairIter.second )
    {
        return;
    }


    diffStr << std::endl << "FieldInfo from Cassandra Driver :" << "\n";
    for(;lPairIter.first!=lPairIter.second;++lPairIter.first){
    	diffStr << lPairIter.first->second;
    }
    diffStr << DIFF_SEPARATOR;

    lDiff.erase(pUniqueID);
}

/**
 * @brief checks if there are differences in Cassandra
 * @param pUniqueID the UNIQUE_ID of the request to check
 * @return true if there are differences, false otherwise
 */
bool checkCassandraDiff(const std::string &pUniqueID)
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

/*
 * Callback to iterate over the headers tables
 * Pushes a copy of key => value in a list
 */
int iterateOverHeadersCallBack(void *d, const char *key, const char *value) {
    std::map< std::string, std::string> *lHeader = reinterpret_cast< std::map< std::string, std::string> *>(d);

    (*lHeader)[std::string(key)] = std::string(value);

    return 1;
}

const unsigned int CMaxBytes = 8192;

bool
extractBrigadeContent(apr_bucket_brigade *bb, ap_filter_t *pF, std::string &content) {
    if (ap_get_brigade(pF->next,
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


apr_status_t inputFilterHandler(ap_filter_t *pF, apr_bucket_brigade *pB, ap_input_mode_t pMode, apr_read_type_e pBlock, apr_off_t pReadbytes)
{
    apr_status_t lStatus;
    request_rec *pRequest = pF->r;
    if (!pRequest) {
        return ap_get_brigade(pF->next, pB, pMode, pBlock, pReadbytes);
    }

    const char *lDupType = apr_table_get(pRequest->headers_in, "Duplication-Type");
    if (( lDupType == NULL ) || ( strcmp("Response", lDupType) != 0) ) {
        return ap_get_brigade(pF->next, pB, pMode, pBlock, pReadbytes);
    }

    if(pRequest->per_dir_config == NULL){
        return ap_get_brigade(pF->next, pB, pMode, pBlock, pReadbytes);
    }
    struct CompareConf *tConf = reinterpret_cast<CompareConf *>(ap_get_module_config(pRequest->per_dir_config, &compare_module));
    if (!tConf) {
        return ap_get_brigade(pF->next, pB, pMode, pBlock, pReadbytes); // SHOULD NOT HAPPEN
    }
    // No context? new request
    if (!pF->ctx) {
        // If there is no UNIQUE_ID in the request header copy thr Request ID generated in both headers
        const char* lID = apr_table_get(pRequest->headers_in, c_UNIQUE_ID);
        unsigned int lReqID;
        DupModule::RequestInfo *info;
        if( lID == NULL){
        lReqID = DupModule::getNextReqId();
        std::string reqId = boost::lexical_cast<std::string>(lReqID);
        apr_table_set(pRequest->headers_in, c_UNIQUE_ID, reqId.c_str());
        apr_table_set(pRequest->headers_out, c_UNIQUE_ID, reqId.c_str());
        info = new DupModule::RequestInfo(reqId);
        }
        else {
            info = new DupModule::RequestInfo(std::string(lID));
            apr_table_set(pRequest->headers_out, c_UNIQUE_ID, lID);
        }

        // Allocation on a shared pointer on the request pool
        // We guarantee that whatever happens, the RequestInfo will be deleted
        void *space = apr_palloc(pRequest->pool, sizeof(boost::shared_ptr<DupModule::RequestInfo>));
        new (space) boost::shared_ptr<DupModule::RequestInfo>(info);
        // Registering of the shared pointer destructor on the pool
        apr_pool_cleanup_register(pRequest->pool, space, apr_pool_cleanup_null,
                                  apr_pool_cleanup_null);
        // Backup in request context
        ap_set_module_config(pRequest->request_config, &compare_module, (void *)space);

        // Backup of info struct in the request context
        pF->ctx = info;

        DupModule::RequestInfo *lRI = static_cast<DupModule::RequestInfo *>(pF->ctx);
        while (!extractBrigadeContent(pB, pF, lRI->mBody)){
            apr_brigade_cleanup(pB);
        }
        pF->ctx = (void *)1;
        apr_brigade_cleanup(pB);
        lRI->offset = 0;
        lStatus =  deserializeBody(*lRI);
#ifndef UNIT_TESTING
        apr_table_set(pRequest->headers_in, "Content-Length",boost::lexical_cast<std::string>(lRI->mReqBody.size()).c_str());
        apr_table_do(&iterateOverHeadersCallBack, &(lRI->mReqHeader), pRequest->headers_in, NULL);
#endif
        printRequest(pRequest, lRI->mReqBody);
        //return lStatus;
    } else if (pF->ctx == (void *)1) {
        // Request is already read and deserialized, sending it to the client
        boost::shared_ptr<DupModule::RequestInfo> * reqInfo(reinterpret_cast<boost::shared_ptr<DupModule::RequestInfo> *>(ap_get_module_config(pF->r->request_config,
                                                                                                                         &compare_module)));
        DupModule::RequestInfo *lRI = reqInfo->get();
        std::string &lBodyToSend = lRI->mReqBody;
        int toSend = std::min((apr_off_t)(lBodyToSend.size() - lRI->offset), pReadbytes);
        if (toSend > 0){
            apr_status_t st;
            if ((st = apr_brigade_write(pB, NULL, NULL, lBodyToSend.c_str() + lRI->offset, toSend)) != APR_SUCCESS ) {
                Log::warn(1, "Failed to write request body in a brigade");
                return st;
            }
            lRI->offset += toSend;
            return APR_SUCCESS;
        } else {
            //pF->ctx = (void *)-1;
            return ap_get_brigade(pF->next, pB, pMode, pBlock, pReadbytes);
        }
    }
    // Everything is read and rewritten, simply returning a get brigade call
    return ap_get_brigade(pF->next, pB, pMode, pBlock, pReadbytes);
}


apr_status_t
outputFilterHandler(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade) {
    request_rec *pRequest = pFilter->r;
    apr_status_t lStatus;
    if (pFilter->ctx == (void *)-1){
        lStatus =  ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return lStatus;
    }

    // Truncate the log before writing if the URI is set to "comp_truncate"
    std::string lArgs( static_cast<const char *>(pRequest->uri) );
    if ( lArgs.find("comp_truncate") != std::string::npos){
        gFile.close();
        gFile.open(gFilePath, std::ofstream::out | std::ofstream::trunc );
        apr_table_set(pRequest->headers_out, "Content-Length", "0");
        pFilter->ctx = (void *) -1;
        lStatus = ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return lStatus;
    }

    if (!pRequest || !pRequest->per_dir_config) {
        pFilter->ctx = (void *) -1;
        lStatus =  ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return lStatus;
    }

    const char *lDupType = apr_table_get(pRequest->headers_in, "Duplication-Type");
    if ( ( lDupType == NULL ) || ( strcmp("Response", lDupType) != 0) ) {
        pFilter->ctx = (void *) -1;
        lStatus = ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return lStatus;
    }

    struct CompareConf *tConf = reinterpret_cast<CompareConf *>(ap_get_module_config(pRequest->per_dir_config, &compare_module));
    if( tConf == NULL ){
        pFilter->ctx = (void *) -1;
        lStatus =  ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return lStatus;
    }

    boost::shared_ptr<DupModule::RequestInfo> *shPtr(reinterpret_cast<boost::shared_ptr<DupModule::RequestInfo> *>(ap_get_module_config(pRequest->request_config, &compare_module)));

    if (!shPtr || !shPtr->get()) {
        pFilter->ctx = (void *) -1;
        lStatus =  ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return lStatus;
    }
    DupModule::RequestInfo *req = shPtr->get();

    apr_bucket *currentBucket;
    apr_status_t rv;
    for ( currentBucket = APR_BRIGADE_FIRST(pBrigade);
          currentBucket != APR_BRIGADE_SENTINEL(pBrigade);
          currentBucket = APR_BUCKET_NEXT(currentBucket) ) {
        const char *data;
        apr_size_t len;

        if (APR_BUCKET_IS_EOS(currentBucket)) {
            req->eos_seen = 1;
            continue;
        }

        rv = apr_bucket_read(currentBucket, &data, &len, APR_BLOCK_READ);

        if ((rv == APR_SUCCESS) && (data != NULL)) {
            req->mDupResponseBody.append(data, len);
        }
    }

    rv = ap_pass_brigade(pFilter->next, pBrigade);
    apr_brigade_cleanup(pBrigade);
    return rv;
}


apr_status_t
outputFilterHandler2(ap_filter_t *pFilter, apr_bucket_brigade *pBrigade) {

    apr_status_t lStatus;
    if (pFilter->ctx == (void *)-1){
        lStatus =  ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return lStatus;
    }

    request_rec *pRequest = pFilter->r;

    struct CompareConf *tConf = reinterpret_cast<CompareConf *>(ap_get_module_config(pRequest->per_dir_config, &compare_module));
    if( tConf == NULL ){
        lStatus =  ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return lStatus;
    }

    boost::shared_ptr<DupModule::RequestInfo> *shPtr(reinterpret_cast<boost::shared_ptr<DupModule::RequestInfo> *>(ap_get_module_config(pRequest->request_config, &compare_module)));

    if ( !shPtr || !shPtr->get()) {
        lStatus =  ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return lStatus;
    }

    DupModule::RequestInfo *req = shPtr->get();
    if (!req->eos_seen) {
        lStatus =  ap_pass_brigade(pFilter->next, pBrigade);
        apr_brigade_cleanup(pBrigade);
        return lStatus;
    }

    req->mRequest = std::string(pRequest->unparsed_uri);
    //write headers in Map
    apr_table_do(&iterateOverHeadersCallBack, &(req->mDupResponseHeader), pRequest->headers_out, NULL);

    std::string diffBody,diffHeader;
    if( tConf->mCompareDisabled){
        writeSerializedRequest(*req);
    }else{
        clock_t start=clock();
        if(tConf->mCompHeader.retrieveDiff(req->mResponseHeader,req->mDupResponseHeader,diffHeader)){
            if (tConf->mCompBody.retrieveDiff(req->mResponseBody,req->mDupResponseBody,diffBody)){
                if(diffHeader.length()!=0 || diffBody.length()!=0 || checkCassandraDiff(req->mId) ){
                    writeDifferences(*req,diffHeader,diffBody,double(clock() - start)/CLOCKS_PER_SEC);
                }
            }
        }
    }
    pFilter->ctx = (void *) -1;
    lStatus = ap_pass_brigade(pFilter->next, pBrigade);
    apr_brigade_cleanup(pBrigade);
    return lStatus;
}

};
