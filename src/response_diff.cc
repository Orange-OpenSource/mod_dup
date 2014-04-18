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
}
