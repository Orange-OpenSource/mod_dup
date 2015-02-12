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
//#include <boost/thread/lock_guard.hpp>


const std::string DIFF_SEPARATOR("-------------------\n");

namespace CompareModule {

void map2string(const std::map< std::string, std::string> &pMap, std::string &pString) {
    std::map< std::string, std::string>::const_iterator lIter;
    for ( lIter = pMap.begin(); lIter != pMap.end(); ++lIter )
    {
        pString += lIter->first + ": " + lIter->second + "\n";
    }
}

/**
 * @brief write response differences in a file or in syslog
 * @param pReqInfo info of the original request
 */
void writeDifferences(const DupModule::RequestInfo &pReqInfo,const std::string& headerDiff,const std::string& bodyDiff, boost::posix_time::time_duration time )
{
    std::string lReqHeader;
    map2string( pReqInfo.mReqHeader, lReqHeader );
    std::stringstream diffLog;
    //boost::posix_time::time_facet *facet = new boost::posix_time::time_facet("%d-%b-%Y %H:%M:%S.%f");
    std::locale lLocale = diffLog.getloc();
    diffLog.imbue(lLocale);

    diffLog << "BEGIN NEW REQUEST DIFFERENCE n°: " << pReqInfo.mId ;
    if (time.total_microseconds()/1000 > 0){
        diffLog << " / Elapsed time for diff computation : " << time.total_microseconds()/1000 << "ms";
    }
    std::map< std::string, std::string >::const_iterator it = pReqInfo.mReqHeader.find("ELAPSED_TIME_BY_DUP");
    std::string diffTime;
    try {
        diffTime = it != pReqInfo.mReqHeader.end() ? boost::lexical_cast<std::string>(boost::lexical_cast<int>(it->second)-boost::lexical_cast<int>(pReqInfo.getElapsedTimeMS())) : "N/A";
    } catch ( boost::bad_lexical_cast &e ) {
        Log::error(12, "Failed to cast ELAPSED_TIME_BY_DUP: %s to an int", it->second.c_str());
        diffTime = "N/C";
    }
#ifndef UNIT_TESTING
    diffLog << std::endl << "Date : " << boost::posix_time::microsec_clock::local_time() <<std::endl;
#endif
    diffLog << std::endl << "Elapsed time for requests (ms): DUP " << (it != pReqInfo.mReqHeader.end() ? it->second : "N/A") << " COMP " << pReqInfo.getElapsedTimeMS() << " DIFF " << diffTime << std::endl;
    diffLog << std::endl << pReqInfo.mRequest.c_str() << std::endl;
    diffLog << std::endl << lReqHeader << std::endl;
    diffLog << pReqInfo.mReqBody.c_str() << std::endl;

    if( pReqInfo.mReqHttpStatus != pReqInfo.mDupResponseHttpStatus ){
        diffLog <<  DIFF_SEPARATOR << "Http Status Codes: DUP " <<  pReqInfo.mReqHttpStatus << " COMP " << pReqInfo.mDupResponseHttpStatus << std::endl;;
    }
    writeCassandraDiff( pReqInfo.mId, diffLog );
    diffLog << DIFF_SEPARATOR << headerDiff << std::endl;
    diffLog << DIFF_SEPARATOR << bodyDiff << std::endl;
    diffLog << "END DIFFERENCE n°:" << pReqInfo.mId << std::endl;
    diffLog.flush();

    if(!gWriteInFile){
        std::string lLine;
        boost::lock_guard<boost::interprocess::named_mutex>  fileLock(getGlobalMutex());
        while (std::getline(diffLog, lLine) )
        {
            writeInFacility(lLine);
        }
    }
    else {
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
}

/**
 * @brief split the input string every 1024 characters and writes it in the syslog
 * @param pDiffLog string to split and to write in syslog
 */
void writeInFacility(std::string pDiffLog){
    int lSplitSize = 1024;
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
            boost::lock_guard<boost::interprocess::named_mutex> fileLock(getGlobalMutex());
            boost::archive::text_oarchive oa(gFile);
            oa << req;
        }
        else
        {
            Log::error(12, "File not correctly opened");
        }
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
}
