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
#include <httpd.h>
// Work-around boost::chrono 1.53 conflict on CR typedef vs define in apache
#undef CR

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <boost/date_time/time_facet.hpp>
#include <boost/thread/locks.hpp>


namespace CompareModule {

void map2string(const std::map< std::string, std::string> &pMap, std::string &pString) {
    std::map< std::string, std::string>::const_iterator lIter;
    for ( lIter = pMap.begin(); lIter != pMap.end(); ++lIter )
    {
        pString += lIter->first + ": " + lIter->second + "\n";
    }
}

void writeDifferences(const DupModule::RequestInfo &pReqInfo,
		LibWsDiff::diffPrinter& printer,
		boost::posix_time::time_duration time)
{
    double t=time.total_milliseconds();
    if (t > 0){
    	printer.addInfo("ElapsedTime",t);
    }

    std::map< std::string, std::string >::const_iterator it = pReqInfo.mReqHeader.find("ELAPSED_TIME_BY_DUP");
    std::string diffTime;
    try {
    	if(it!=pReqInfo.mReqHeader.end()){
    		t=boost::lexical_cast<int>(it->second)-boost::lexical_cast<int>(pReqInfo.getElapsedTimeMS());
    		printer.addRuntime("DIFF",t);
    	}
    } catch ( boost::bad_lexical_cast &e ) {
        Log::error(12, "[COMPARE] Failed to cast ELAPSED_TIME_BY_DUP: %s to an int", it->second.c_str());
    }

#ifdef UNIT_TESTING
    printer.addInfo("Date","UNITTEST_TODAY_VALUE");
#else
    boost::posix_time::ptime today=boost::posix_time::microsec_clock::universal_time();
	printer.addInfo("Date",boost::posix_time::to_iso_extended_string(today));
#endif

    if(it!=pReqInfo.mReqHeader.end()){
    	printer.addRuntime("DUP",boost::lexical_cast<int>(it->second));
    }
  	printer.addRuntime("COMP",pReqInfo.getElapsedTimeMS());

    if(pReqInfo.mRequest.back()=='?'){
    	printer.addRequestUri(pReqInfo.mRequest,pReqInfo.mReqBody);
    }else{
    	printer.addRequestUri(pReqInfo.mRequest);
    	if(!pReqInfo.mReqBody.empty()){
    		printer.addRequestBody(pReqInfo.mReqBody);
    	}
	}

    std::map< std::string, std::string>::const_iterator lIter;
	for ( lIter = pReqInfo.mReqHeader.begin(); lIter != pReqInfo.mReqHeader.end(); ++lIter )
	{
		printer.addRequestHeader(lIter->first,lIter->second);
	}

    printer.addStatus("DUP",pReqInfo.mReqHttpStatus);
    printer.addStatus("COMP",pReqInfo.mDupResponseHttpStatus);

    std::string res;
    bool cassDiff = writeCassandraDiff( pReqInfo.mId, printer );
    bool diff = printer.retrieveDiff(res);

    if ( ( !cassDiff ) && ( !diff ) ){
        Log::debug("No diff produced");
        return;
    }
    
    if(!gWriteInFile){
        if (printer.getPrinterType() == LibWsDiff::diffPrinter::MULTILINE) {
            Log::debug("We have a MULTILINE diff written to syslog");
            pthread_mutex_t *lMutex = getGlobalMutex();
            if (not lMutex) {
                Log::error(12, "[COMPARE] Cannot write differences ! Mutex not initialized");
                return;
            }
            if (pthread_mutex_lock(lMutex) == EOWNERDEAD) {
                pthread_mutex_consistent(lMutex);
            }
            std::vector<std::string> resLines;
            boost::split(resLines,res,boost::is_any_of("\n"));
            for(std::vector<std::string>::iterator it= resLines.begin();
                            it!=resLines.end();it++)
            {
                writeInFacility(*it);
            }
            pthread_mutex_unlock(lMutex);
        } else {
            Log::debug("We have a json diff written to syslog");
            // syslog is threadsafe
            Log::error(12, "%s", res.c_str());
        }
    }
    else {
        if (gFile.is_open()){
            pthread_mutex_t *lMutex = getGlobalMutex();
            if (not lMutex) {
                Log::error(12, "[COMPARE] Cannot write differences ! Mutex not initialized");
                return;
            }
            if (pthread_mutex_lock(lMutex) == EOWNERDEAD) {
                pthread_mutex_consistent(lMutex);
            }
            Log::debug("We have a diff written to file");
            gFile << res;
            gFile.flush();
            pthread_mutex_unlock(lMutex);
        }
        else
        {
            Log::error(12, "[COMPARE] File not correctly opened");
        }
    }
}

/**
 * @brief split the input string every 1024 characters and writes it in the syslog
 * @param pDiffLog string to split and to write in syslog
 */
void writeInFacility(const std::string& pDiffLog){
    int lSplitSize = LOGMAXSIZE;
    int stringLength = pDiffLog.size();
    for (int i = 0; i < stringLength ; i += lSplitSize)
    {
        if (i + lSplitSize > stringLength){
            lSplitSize = stringLength  - i;
        }
        Log::error(12, "%s", pDiffLog.substr(i, lSplitSize).c_str());
    }
}


/**
 * @brief write request body and header in the syslog or in file with serialized boost method
 * @param req object containing the infos about the request
 */
void writeSerializedRequest(const DupModule::RequestInfo& req)
{
    if(!gWriteInFile){
        std::stringstream lSerialRequest;
        boost::archive::text_oarchive oa(lSerialRequest);
        oa << req;
        //no need to split on '\n'
        writeInFacility(lSerialRequest.str());
    }
    else {
        if (gFile.is_open()){
            pthread_mutex_t *lMutex = getGlobalMutex();
            if (not lMutex) {
                Log::error(12, "[COMPARE] Cannot write differences ! Mutex not initialized");
                return;
            }
            if (pthread_mutex_lock(lMutex) == EOWNERDEAD) {
                pthread_mutex_consistent(lMutex);
            }
            boost::archive::text_oarchive oa(gFile);
            oa << req;
            pthread_mutex_unlock(lMutex);
        }
        else
        {
            Log::error(12, "[COMPARE] File not correctly opened");
        }
    }

}

/**
 * @brief write the Cassandra differences in log file
 * @param pUniqueID the UNIQUE_ID of the request to check
 * @return true if there are differences, false otherwise
 */
bool writeCassandraDiff(const std::string &pUniqueID, LibWsDiff::diffPrinter& printer)
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

    for(;lPairIter.first!=lPairIter.second;++lPairIter.first){
    	printer.addCassandraDiff(lPairIter.first->second.mName,
    			lPairIter.first->second.mMultivalueKey,
    			lPairIter.first->second.mDBValue,
    			lPairIter.first->second.mReqValue);
    }
    lDiff.erase(pUniqueID);
    return true;
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
}
