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
#include <http_config.h>
#include <http_request.h>
#include <http_protocol.h>
#include <algorithm>
#include <boost/thread/detail/singleton.hpp>

#include <apr_pools.h>
#include <apr_hooks.h>
#include "apr_strings.h"

#include "testModCompare.hh"
#include "mod_compare.hh"
#include "Log.hh"
#include "CassandraDiff.h"

// cppunit
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

CPPUNIT_TEST_SUITE_REGISTRATION( TestModCompare );

extern module AP_DECLARE_DATA compare_module;

using namespace CompareModule;


//////////////////////////////////////////////////////////////
// Dummy implementations of apache funcs
/////////////////////////////////////////////////////////////
/*apr_status_t
ap_pass_brigade(ap_filter_t *, apr_bucket_brigade *){
    return OK;
}

const apr_bucket_type_t 	apr_bucket_type_eos = apr_bucket_type_t();
apr_bucket_brigade * 	apr_brigade_create (apr_pool_t *, apr_bucket_alloc_t *){
    return NULL;
}

apr_status_t 	apr_brigade_cleanup (void *){
    return OK;
}*/
/*apr_status_t ap_get_brigade(ap_filter_t *, apr_bucket_brigade *, ap_input_mode_t pMode, apr_read_type_e pBlock, apr_off_t pReadbytes)
{
    return APR_SUCCESS;
}*/
//////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

cmd_parms * TestModCompare::getParms() {
    cmd_parms * lParms = new cmd_parms;
    server_rec * lServer = new server_rec;
    memset(lParms, 0, sizeof(cmd_parms));
    memset(lServer, 0, sizeof(server_rec));
    lParms->server = lServer;
    apr_pool_create(&lParms->pool, 0);
    //lServer->module_config = reinterpret_cast<ap_conf_vector_t *>(apr_pcalloc(lParms->pool, sizeof(void *) * 16));
    //ap_set_module_config(lServer->module_config, &dup_module, gsDropRate);

    return lParms;
}

void TestModCompare::testInit()
{
    cmd_parms * lParms = getParms();
    registerHooks(lParms->pool);
    CPPUNIT_ASSERT(postConfig(lParms->pool, lParms->pool, lParms->pool, lParms->server)==OK);

    CPPUNIT_ASSERT( createDirConfig(lParms->pool, NULL) );
}


void TestModCompare::testConfig()
{
    CompareConf *lDoHandle = new CompareConf();

    CPPUNIT_ASSERT(setIgnoreList(NULL, (void *) lDoHandle, "pippo", ""));
    CPPUNIT_ASSERT(setIgnoreList(NULL, (void *) lDoHandle, "", "pippo"));
    CPPUNIT_ASSERT(setIgnoreList(NULL, (void *) lDoHandle, "toto", "pippo"));
    CPPUNIT_ASSERT(!setIgnoreList(NULL, (void *) lDoHandle, "Header", "pippo"));
    CPPUNIT_ASSERT(std::find( lDoHandle->mHeaderIgnoreList.begin(), lDoHandle->mHeaderIgnoreList.end(), "pippo") != lDoHandle->mHeaderIgnoreList.end());
    CPPUNIT_ASSERT(!setIgnoreList(NULL, (void *) lDoHandle, "Body", "pluto"));
    CPPUNIT_ASSERT(std::find( lDoHandle->mBodyIgnoreList.begin(), lDoHandle->mBodyIgnoreList.end(), "pluto") != lDoHandle->mBodyIgnoreList.end());

    CPPUNIT_ASSERT(setStopList(NULL, (void *) lDoHandle, "pippo", ""));
    CPPUNIT_ASSERT(setStopList(NULL, (void *) lDoHandle, "", "pippo"));
    CPPUNIT_ASSERT(setStopList(NULL, (void *) lDoHandle, "toto", "pippo"));
    CPPUNIT_ASSERT(!setStopList(NULL, (void *) lDoHandle, "Header", "pippo"));
    CPPUNIT_ASSERT(std::find( lDoHandle->mHeaderStopList.begin(), lDoHandle->mHeaderStopList.end(), "pippo") != lDoHandle->mHeaderStopList.end());
    CPPUNIT_ASSERT(!setStopList(NULL, (void *) lDoHandle, "Body", "pluto"));
    CPPUNIT_ASSERT(std::find( lDoHandle->mBodyStopList.begin(), lDoHandle->mBodyStopList.end(), "pluto") != lDoHandle->mBodyStopList.end());

    delete lDoHandle;
}

void TestModCompare::testPrintRequest()
{
    request_rec r;
    cmd_parms * lParms = getParms();

    memset(&r,0,sizeof(request_rec));
    apr_pool_create(&r.pool, 0);
    r.headers_in = apr_table_make(r.pool, 16);
    r.server = lParms->server;
    r.pool = lParms->pool;

    apr_table_set(r.headers_in, "UNIQUE_ID", "toto999");

    CompareModule::printRequest( &r, std::string("toto+pippo"));

}

void TestModCompare::testCheckCassandraDiff()
{
    CassandraDiff::Differences & lDiff = boost::detail::thread::singleton<CassandraDiff::Differences>::instance();
    std::string lID("IDtoto");
    CPPUNIT_ASSERT(!CompareModule::checkCassandraDiff(lID));

    CassandraDiff::FieldInfo lFieldInfo("toto", "pippo", "pepita", "maradona");
    lDiff.insert( std::pair<std::string, CassandraDiff::FieldInfo>(lID, lFieldInfo) );

    CPPUNIT_ASSERT(CompareModule::checkCassandraDiff(lID));

}

void TestModCompare::testGetLength()
{
    std::string lString("00000345Diego");
    size_t lFirst = 0;
    size_t lLength = 0;

    CPPUNIT_ASSERT(getLength( lString, lFirst, lLength ));
    CPPUNIT_ASSERT(lLength == 345);

    lString = "toto0345Diego";
    CPPUNIT_ASSERT(!getLength( lString, lFirst, lLength ));
}

void TestModCompare::testDeserializeBody()
{
    apr_status_t lStatus;
    apr_status_t BAD_REQUEST = 400;
    DupModule::RequestInfo lReqInfo;
    std::string lBody;

    // case1: body size too small
    lReqInfo.mBody = ("zfz");
    lStatus = deserializeBody(lReqInfo, lBody);
    CPPUNIT_ASSERT(lStatus == BAD_REQUEST);

    // case2: Unexpected body format for the first 8 characters
    lReqInfo.mBody = "00aze012dadadada00000002totototo00000125";
    lStatus = deserializeBody(lReqInfo, lBody);
    CPPUNIT_ASSERT(lStatus == BAD_REQUEST);

    // case3: Unexpected format for the size of response header (characters 000kk002)
    lReqInfo.mBody = "00000002da000kk002totototo00000125";
    lStatus = deserializeBody(lReqInfo, lBody);
    CPPUNIT_ASSERT(lStatus == BAD_REQUEST);

    // case4: Unexpected format for the size of response body (characters 000000d4)
    lReqInfo.mBody = "00000002da00000004toto000000d4tutu";
    lStatus = deserializeBody(lReqInfo, lBody);
    CPPUNIT_ASSERT(lStatus == BAD_REQUEST);

    // case5: invalid header format
    lReqInfo.mBody = "00000002da00000004toto00000004tutu";
    lStatus = deserializeBody(lReqInfo, lBody);
    CPPUNIT_ASSERT(lStatus == BAD_REQUEST);

    // case6: OK
    lReqInfo.mBody.clear();
    lReqInfo.mBody = "00000002da00000021toto: good\ntiti: bad\n00000004tutu";
    lStatus = deserializeBody(lReqInfo, lBody);
    CPPUNIT_ASSERT(lStatus == OK);
    CPPUNIT_ASSERT(lBody.compare("da") == 0);


}


void TestModCompare::testInputFilterHandler()
{
    /*cmd_parms * lParms = getParms();
    request_rec r;
    memset(&r,0,sizeof(request_rec));
    apr_pool_create(&r.pool, 0);

    ap_filter_t * lfilter = new ap_filter_t;
    memset(lfilter, 0, sizeof(ap_filter_t));
    lfilter->r = &r;

    ap_filter_t * lFilterNext = new ap_filter_t;
    memset(lFilterNext, 0, sizeof(ap_filter_t));
    lfilter->next = lFilterNext;

    r.headers_in = apr_table_make(r.pool, 16);
    r.server = lParms->server;
    r.pool = lParms->pool;

    apr_bucket_brigade *pB = new apr_bucket_brigade;
    memset(pB, 0, sizeof(apr_bucket_brigade));

    inputFilterHandler(lfilter, (apr_bucket_brigade*)pB, AP_MODE_READBYTES, APR_BLOCK_READ, 1);*/
}

