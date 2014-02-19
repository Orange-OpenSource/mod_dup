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
#include <boost/assign.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <fstream>
#include <iterator>
#include <iostream>

#include <libws-diff/stringCompare.hh>
#include <libws-diff/mapCompare.hh>

#include <apr_pools.h>
#include <apr_hooks.h>
#include "apr_strings.h"

#include "testModCompare.hh"
#include "mod_compare.hh"
#include "Log.hh"
#include "CassandraDiff.h"
#include "testBodies.hh"
#include "RequestInfo.hh"

// cppunit
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

CPPUNIT_TEST_SUITE_REGISTRATION( TestModCompare );

//extern module AP_DECLARE_DATA compare_module;

using namespace CompareModule;

cmd_parms * TestModCompare::getParms() {
    cmd_parms * lParms = new cmd_parms;
    server_rec * lServer = new server_rec;
    memset(lParms, 0, sizeof(cmd_parms));
    memset(lServer, 0, sizeof(server_rec));
    lParms->server = lServer;
    apr_pool_create(&lParms->pool, 0);

    return lParms;
}

void TestModCompare::tearDown() {
    cmd_parms * lParms = getParms();
    if( lParms != NULL){
       delete lParms;
    }
}

static request_rec *prep_request_rec() {
    request_rec *req = new request_rec;
    memset(req, 0, sizeof(*req));
    apr_pool_create(&req->pool, 0);
    req->per_dir_config = (ap_conf_vector_t *)apr_pcalloc(req->pool, sizeof(void *) * 42);
    req->request_config = (ap_conf_vector_t *)apr_pcalloc(req->pool, sizeof(void *) * 42);
    req->connection = (conn_rec *)apr_pcalloc(req->pool, sizeof(*(req->connection)));
    req->connection->bucket_alloc = apr_bucket_alloc_create(req->pool);
    apr_pool_create(&req->connection->pool, 0);

    req->headers_in = apr_table_make(req->pool, 42);
    req->headers_out = apr_table_make(req->pool, 42);
    return req;
}

template<class T>
T *memSet(T *addr, char c = 0) {
    memset(addr, c, sizeof(T));
    return addr;
}


void TestModCompare::testInit()
{
    cmd_parms * lParms = getParms();
    registerHooks(lParms->pool);
    CPPUNIT_ASSERT(postConfig(lParms->pool, lParms->pool, lParms->pool, lParms->server)==OK);

    CPPUNIT_ASSERT( createDirConfig(lParms->pool, NULL) );

    setFilePath(lParms, NULL , "toto");
    childInit(lParms->pool, lParms->server);
}


void TestModCompare::testConfig()
{
    CompareConf *lDoHandle = new CompareConf();

    CPPUNIT_ASSERT(setHeaderList(NULL, (void *) lDoHandle, "pippo","myKey",  ""));
    CPPUNIT_ASSERT(setHeaderList(NULL, (void *) lDoHandle, "","myKey", "pippo"));
    CPPUNIT_ASSERT(setHeaderList(NULL, (void *) lDoHandle, "toto","myKey", "pippo"));
    CPPUNIT_ASSERT(!setHeaderList(NULL, (void *) lDoHandle, "IGNORE", "myKey","pippo"));
    CPPUNIT_ASSERT(!setHeaderList(NULL, (void *) lDoHandle, "STOP","myKey", "pluto"));

    CPPUNIT_ASSERT(setBodyList(NULL, (void *) lDoHandle, "pippo", ""));
    CPPUNIT_ASSERT(setBodyList(NULL, (void *) lDoHandle, "", "pippo"));
    CPPUNIT_ASSERT(setBodyList(NULL, (void *) lDoHandle, "toto", "pippo"));
    CPPUNIT_ASSERT(!setBodyList(NULL, (void *) lDoHandle, "IGNORE", "pippo"));
    CPPUNIT_ASSERT(!setBodyList(NULL, (void *) lDoHandle, "STOP", "pluto"));

    setDisableLibwsdiff(NULL, (void *) lDoHandle, "true");
    CPPUNIT_ASSERT(lDoHandle->mCompareDisabled);
    setDisableLibwsdiff(NULL, (void *) lDoHandle, "whatever");
    CPPUNIT_ASSERT(!lDoHandle->mCompareDisabled);
    setDisableLibwsdiff(NULL, (void *) lDoHandle, "1");
    CPPUNIT_ASSERT(lDoHandle->mCompareDisabled);

    CPPUNIT_ASSERT( CompareConf::cleaner( (void *)lDoHandle ) == 0 );
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

void TestModCompare::testWriteCassandraDiff()
{
	std::string lPath( getenv("HOME") );
	lPath.append("/log_differences_Cass.txt");
	gFile.close();
    gFile.open(lPath.c_str());

    CassandraDiff::Differences & lDiff = boost::detail::thread::singleton<CassandraDiff::Differences>::instance();
    std::string lID("IDtoto"),DiffCase("noDiffID");
    CPPUNIT_ASSERT(!CompareModule::writeCassandraDiff(lID));

    CassandraDiff::FieldInfo lFieldInfo1("myName", "myMultiValueKey", "myDbValue", "myReqValue");
    CassandraDiff::FieldInfo lFieldInfo2("myOtherData", "myOtherMultiValueKey", "myOtherDbValue", "myOtherReqValue");
    lDiff.insert(std::pair<std::string, CassandraDiff::FieldInfo>(DiffCase,lFieldInfo1));
    lDiff.insert(std::pair<std::string, CassandraDiff::FieldInfo>(DiffCase,lFieldInfo2));
    CPPUNIT_ASSERT(CompareModule::writeCassandraDiff(DiffCase));

    CassandraDiff::FieldInfo lFieldInfo("toto", "pippo", "pepita", "maradona");
    lDiff.insert( std::pair<std::string, CassandraDiff::FieldInfo>(lID, lFieldInfo) );

    CPPUNIT_ASSERT(CompareModule::writeCassandraDiff(lID));

    CPPUNIT_ASSERT( closeFile( (void *)1) == APR_SUCCESS);

    {
		std::ifstream readFile;
		readFile.open(lPath.c_str());
		std::stringstream buffer;
		buffer << readFile.rdbuf() ;

		std::string assertRes("FieldInfo differences for pUniqueID : noDiffID\n"
"Field name in the db : 'myName'\n"
"Multivalue/Collection index/key : 'myMultiValueKey'\n"
"Value retrieved in Database : 'myDbValue'\n"
"Value about to be set from Request : 'myReqValue'\n"
"Field name in the db : 'myOtherData'\n"
"Multivalue/Collection index/key : 'myOtherMultiValueKey'\n"
"Value retrieved in Database : 'myOtherDbValue'\n"
"Value about to be set from Request : 'myOtherReqValue'\n"
"-------------------\n"
"FieldInfo differences for pUniqueID : IDtoto\n"
"Field name in the db : 'toto'\n"
"Multivalue/Collection index/key : 'pippo'\n"
"Value retrieved in Database : 'pepita'\n"
"Value about to be set from Request : 'maradona'\n"
"-------------------\n");
		CPPUNIT_ASSERT(buffer.str()==assertRes);
    }
}

void TestModCompare::testWriteSerializedRequests(){
    std::string lPath( getenv("HOME") );
    lPath.append("/log_differences_serialized.txt");
    gFile.close();
    gFile.open(lPath);

    std::map<std::string,std::string> header1 = boost::assign::map_list_of("header","header1");
    std::map<std::string,std::string> header2 = boost::assign::map_list_of("header","header1");
    std::map<std::string,std::string> header3 = boost::assign::map_list_of("header","header1");
    DupModule::RequestInfo req(header1,"mybody1",header2,"mybody2",header3,"mybody3");

    writeSerializedRequest(&req);

    CPPUNIT_ASSERT( closeFile( (void *)1) == APR_SUCCESS);
    {
		std::ifstream readFile;
		readFile.open(lPath.c_str());
		DupModule::RequestInfo retrievedReq;
		boost::archive::text_iarchive iarch(readFile);

		iarch >>retrievedReq;
		CPPUNIT_ASSERT(req.mReqBody == retrievedReq.mReqBody &&
					req.mResponseHeader == retrievedReq.mResponseHeader &&
					req.mResponseBody == retrievedReq.mResponseBody &&
					req.mDupResponseHeader == retrievedReq.mDupResponseHeader &&
					req.mDupResponseBody == retrievedReq.mDupResponseBody);
	}
    //TODO prepare conf to check call writeSerializeRequest ok
}

void TestModCompare::testWriteDifferences()
{
    std::string lPath( getenv("HOME") );
    lPath.append("/log_differences.txt");
    gFile.close();
    gFile.open(lPath.c_str());

    DupModule::RequestInfo lReqInfo;
    lReqInfo.mReqHeader["content-type"]= "plain/text";  //size = 11
    lReqInfo.mReqHeader["agent-type"]= "myAgent";  //size = 11
    lReqInfo.mReqHeader["date"]= "TODAY";  //size = 11
    lReqInfo.mReqBody="MyClientRequest";
    lReqInfo.mId=123;

    writeDifferences(lReqInfo,"myHeaderDiff","myBodyDiff",0.001);
    CPPUNIT_ASSERT( closeFile( (void *)1) == APR_SUCCESS);

    {
		std::ifstream readFile;
		readFile.open(lPath.c_str());
		std::stringstream buffer;
		buffer << readFile.rdbuf();
		std::string assertRes("BEGIN NEW REQUEST DIFFERENCE n°: 123 / Elapsed time : 0.001s\n"
				"agent-type: myAgent\n"
				"content-type: plain/text\n"
				"date: TODAY\n"
				"\n"
				"MyClientRequest\n"
				"-------------------\n"
				"myHeaderDiff\n"
				"-------------------\n"
				"myBodyDiff\n"
				"END DIFFERENCE n°:123\n");
		std::cout << "\n==>" << buffer.str() << "\n-\n"<< assertRes << std::endl;
		CPPUNIT_ASSERT(buffer.str()==assertRes);
    }
}

void TestModCompare::testGetLength()
{
    std::string lString("00000345Diego");
    size_t lFirst = 0;

    CPPUNIT_ASSERT(getLength( lString, lFirst)== 345 );

    bool hasThrownError=false;
    try{
        lString = "toto0345Diego";
        getLength( lString, lFirst);
    }catch(const std::out_of_range &oor){
    	hasThrownError=true;
    }catch(boost::bad_lexical_cast){
    	hasThrownError=true;
    }
    CPPUNIT_ASSERT(hasThrownError);
}

void TestModCompare::testDeserializeBody()
{
    apr_status_t lStatus;
    apr_status_t BAD_REQUEST = 400;
    DupModule::RequestInfo lReqInfo;

    // case1: body size too small
    lReqInfo.mBody = ("zfz");
    lStatus = deserializeBody(lReqInfo);
    CPPUNIT_ASSERT(lStatus == BAD_REQUEST);

    // case2: Unexpected body format for the first 8 characters
    lReqInfo.mBody = "00aze012dadadada00000002totototo00000125";
    lStatus = deserializeBody(lReqInfo);
    CPPUNIT_ASSERT(lStatus == BAD_REQUEST);

    // case3: Unexpected format for the size of response header (characters 000kk002)
    lReqInfo.mBody = "00000002da000kk002totototo00000125";
    lStatus = deserializeBody(lReqInfo);
    CPPUNIT_ASSERT(lStatus == BAD_REQUEST);

    // case4: Unexpected format for the size of response body (characters 000000d4)
    lReqInfo.mBody = "00000002da00000004toto000000d4tutu";
    lStatus = deserializeBody(lReqInfo);
    CPPUNIT_ASSERT(lStatus == BAD_REQUEST);

    // case5: invalid header format
    lReqInfo.mBody = "00000002da00000004toto00000004tutu";
    lStatus = deserializeBody(lReqInfo);
    CPPUNIT_ASSERT(lStatus == BAD_REQUEST);

    // case6: OK
    lReqInfo.mBody.clear();
    lReqInfo.mBody = "00000002da00000021toto: good\ntiti: bad\n00000004tutu";
    lStatus = deserializeBody(lReqInfo);
    CPPUNIT_ASSERT(lStatus == OK);
    CPPUNIT_ASSERT(lReqInfo.mReqBody.compare("da") == 0);


}

void TestModCompare::testMap2string()
{
    std::map< std::string, std::string> lMap;
    std::string lString;
    lMap["Maradona"] = "TheBest";
    lMap["Pele"] = "GoodPlayer";
    lMap["toto"] = "titi";

    map2string(lMap, lString);
    CPPUNIT_ASSERT( lString == "Maradona: TheBest\nPele: GoodPlayer\ntoto: titi\n");
}


void TestModCompare::testIterOverHeader()
{
    std::map< std::string, std::string> lMap;
    iterateOverHeadersCallBack( &lMap, "Maradona", "TheBest");
    iterateOverHeadersCallBack( &lMap, "Pele", "GoodPlayer");
    iterateOverHeadersCallBack( &lMap, "toto", "titi");

    CPPUNIT_ASSERT( lMap.find("Maradona") != lMap.end() );
    CPPUNIT_ASSERT( lMap.find("Pele") != lMap.end() );
    CPPUNIT_ASSERT( lMap.find("toto") != lMap.end() );
}

void TestModCompare::testInputFilterHandler()
{
    {
        // get_brigade fails
        request_rec *req = prep_request_rec();

        ap_filter_t *filter = new ap_filter_t;
        memSet(filter);
        apr_pool_t *pool = NULL;
        apr_pool_create(&pool, 0);
        filter->r = req;
        filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
        filter->next = (ap_filter_t *)(void *) 0x44;
        filter->c->bucket_alloc = apr_bucket_alloc_create(pool);
        apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);
        req->per_dir_config = NULL;
        CPPUNIT_ASSERT_EQUAL( 1, inputFilterHandler( filter, bb, AP_MODE_READBYTES, APR_BLOCK_READ, 8192 ) );
    }

    {
        // request_rec NULL
        request_rec *req = prep_request_rec();

        ap_filter_t *filter = new ap_filter_t;
        memSet(filter);
        apr_pool_t *pool = NULL;
        apr_pool_create(&pool, 0);
        filter->r = NULL;
        filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
        filter->next = (ap_filter_t *)(void *) 0x43;
        filter->c->bucket_alloc = apr_bucket_alloc_create(pool);
        apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);
        req->per_dir_config = NULL;
        CPPUNIT_ASSERT_EQUAL( APR_SUCCESS, inputFilterHandler( filter, bb, AP_MODE_READBYTES, APR_BLOCK_READ, 8192 ) );
    }

    {
        // missing header "Response"
        request_rec *req = prep_request_rec();

        ap_filter_t *filter = new ap_filter_t;
        memSet(filter);
        apr_pool_t *pool = NULL;
        apr_pool_create(&pool, 0);
        filter->r = req;
        filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
        filter->c->bucket_alloc = apr_bucket_alloc_create(pool);
        filter->next = (ap_filter_t *)(void *) 0x43;
        apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);
        req->per_dir_config = NULL;
        CPPUNIT_ASSERT_EQUAL( APR_SUCCESS, inputFilterHandler( filter, bb, AP_MODE_READBYTES, APR_BLOCK_READ, 8192 ) );

    }
    {
        // empty per_dir_conf
        request_rec *req = prep_request_rec();

        ap_filter_t *filter = new ap_filter_t;
        memSet(filter);
        apr_pool_t *pool = NULL;
        apr_pool_create(&pool, 0);
        filter->r = req;
        filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
        filter->c->bucket_alloc = apr_bucket_alloc_create(pool);
        filter->next = (ap_filter_t *)(void *) 0x43;
        apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);
        req->per_dir_config = NULL;
        apr_table_set(req->headers_in, "Duplication-Type", "Response");
        CPPUNIT_ASSERT_EQUAL( APR_SUCCESS, inputFilterHandler( filter, bb, AP_MODE_READBYTES, APR_BLOCK_READ, 8192 ) );

    }

    {
        // NULL CompareConf
        request_rec *req = prep_request_rec();

        ap_filter_t *filter = new ap_filter_t;
        memSet(filter);
        apr_pool_t *pool = NULL;
        apr_pool_create(&pool, 0);
        filter->r = req;
        filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
        filter->c->bucket_alloc = apr_bucket_alloc_create(pool);
        filter->next = (ap_filter_t *)(void *) 0x43;
        apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);
        apr_table_set(req->headers_in, "Duplication-Type", "Response");
        CPPUNIT_ASSERT_EQUAL( APR_SUCCESS, inputFilterHandler( filter, bb, AP_MODE_READBYTES, APR_BLOCK_READ, 8192 ) );

    }

    {
        // // NOMINAL CASE REQUEST + EMPTY BODY
        request_rec *req = prep_request_rec();

        ap_filter_t *filter = new ap_filter_t;
        memSet(filter);
        apr_pool_t *pool = NULL;
        apr_pool_create(&pool, 0);
        filter->r = req;
        filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
        filter->c->bucket_alloc = apr_bucket_alloc_create(pool);
        filter->next = NULL;
        apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);
        apr_table_set(req->headers_in, "Duplication-Type", "Response");
        CompareConf *conf = new CompareConf;
        ap_set_module_config(req->per_dir_config, &compare_module, conf);
        CPPUNIT_ASSERT_EQUAL( 400, inputFilterHandler( filter, bb, AP_MODE_READBYTES, APR_BLOCK_READ, 8192 ) );

    }

    {
        // // NOMINAL CASE REQUEST + BODY ( invalid body format)
        request_rec *req = prep_request_rec();

        ap_filter_t *filter = new ap_filter_t;
        memSet(filter);
        apr_pool_t *pool = NULL;
        apr_pool_create(&pool, 0);
        filter->r = req;
        filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
        filter->c->bucket_alloc = apr_bucket_alloc_create(pool);
        filter->next = (ap_filter_t *)(void *) 0x42;
        apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);
        apr_table_set(req->headers_in, "Duplication-Type", "Response");
        CompareConf *conf = new CompareConf;
        ap_set_module_config(req->per_dir_config, &compare_module, conf);
        apr_table_set(req->headers_in, "UNIQUE_ID", "toto");
        CPPUNIT_ASSERT_EQUAL( 400, inputFilterHandler( filter, bb, AP_MODE_READBYTES, APR_BLOCK_READ, 8192 ) );

        // Second call, tests context backup
        CPPUNIT_ASSERT_EQUAL( APR_SUCCESS, inputFilterHandler( filter, bb, AP_MODE_READBYTES, APR_BLOCK_READ, 8192 ) );

    }

}

void TestModCompare::testOutputFilterHandler()
{
    {
        // URI comp_truncate
        request_rec *req = prep_request_rec();

        ap_filter_t *filter = new ap_filter_t;
        memSet(filter);
        apr_pool_t *pool = NULL;
        apr_pool_create(&pool, 0);
        filter->r = req;
        filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
        filter->c->bucket_alloc = apr_bucket_alloc_create(pool);
        filter->next = (ap_filter_t *)(void *) 0x42;
        apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);
        req->uri = (char *)"comp_truncate";
        CPPUNIT_ASSERT_EQUAL( APR_SUCCESS, outputFilterHandler( filter, bb ) );
    }

    {
        // empty per_dir_conf
        request_rec *req = prep_request_rec();

        ap_filter_t *filter = new ap_filter_t;
        memSet(filter);
        apr_pool_t *pool = NULL;
        apr_pool_create(&pool, 0);
        req->per_dir_config = NULL;
        filter->r = req;
        filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
        filter->c->bucket_alloc = apr_bucket_alloc_create(pool);
        filter->next = (ap_filter_t *)(void *) 0x42;
        apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);
        req->uri = (char *)"";
        CPPUNIT_ASSERT_EQUAL( APR_SUCCESS, outputFilterHandler( filter, bb ) );
    }

    {
        // missing header "Response"
        request_rec *req = prep_request_rec();

        ap_filter_t *filter = new ap_filter_t;
        memSet(filter);
        apr_pool_t *pool = NULL;
        apr_pool_create(&pool, 0);
        filter->r = req;
        filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
        filter->c->bucket_alloc = apr_bucket_alloc_create(pool);
        filter->next = (ap_filter_t *)(void *) 0x42;
        apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);
        req->uri = (char *)"";
        CompareConf *conf = new CompareConf;
        ap_set_module_config(req->per_dir_config, &compare_module, conf);
        CPPUNIT_ASSERT_EQUAL( APR_SUCCESS, outputFilterHandler( filter, bb ) );
    }

    {
        // case pFilter->ctx = -1
        request_rec *req = prep_request_rec();

        ap_filter_t *filter = new ap_filter_t;
        memSet(filter);
        apr_pool_t *pool = NULL;
        apr_pool_create(&pool, 0);
        filter->r = req;
        filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
        filter->c->bucket_alloc = apr_bucket_alloc_create(pool);
        filter->next = (ap_filter_t *)(void *) 0x42;
        apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);
        req->uri = (char *)"";
        CompareConf *conf = new CompareConf;
        ap_set_module_config(req->per_dir_config, &compare_module, conf);
        apr_table_set(req->headers_in, "Duplication-Type", "Response");
        filter->ctx = (void *)(-1);
        CPPUNIT_ASSERT_EQUAL( APR_SUCCESS, outputFilterHandler( filter, bb ) );
    }
    {
        // case req->request_config = NULL
        request_rec *req = prep_request_rec();

        ap_filter_t *filter = new ap_filter_t;
        memSet(filter);
        apr_pool_t *pool = NULL;
        apr_pool_create(&pool, 0);
        filter->r = req;
        filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
        filter->c->bucket_alloc = apr_bucket_alloc_create(pool);
        filter->next = (ap_filter_t *)(void *) 0x42;
        apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);
        req->uri = (char *)"";
        CompareConf *conf = new CompareConf;
        ap_set_module_config(req->per_dir_config, &compare_module, conf);
        apr_table_set(req->headers_in, "Duplication-Type", "Response");
        CPPUNIT_ASSERT_EQUAL( APR_SUCCESS, outputFilterHandler( filter, bb ) );
    }

    {
        // NOMINAL TEST
        request_rec *req = prep_request_rec();

        ap_filter_t *filter = new ap_filter_t;
        memSet(filter);
        apr_pool_t *pool = NULL;
        apr_pool_create(&pool, 0);
        filter->r = req;
        filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
        filter->c->bucket_alloc = apr_bucket_alloc_create(pool);
        filter->next = (ap_filter_t *)(void *) 0x43;
        req->uri = (char *)"";

        CompareConf *conf = new CompareConf;
        ap_set_module_config(req->per_dir_config, &compare_module, conf);

        apr_table_set(req->headers_in, "Duplication-Type", "Response");

        DupModule::RequestInfo *info = new DupModule::RequestInfo(42);
        ap_set_module_config(req->request_config, &compare_module, info);

        apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);
        CPPUNIT_ASSERT_EQUAL(APR_SUCCESS, apr_brigade_write(bb, NULL, NULL, testBody42, std::string(testBody42).size()));

        CPPUNIT_ASSERT_EQUAL( APR_SUCCESS, outputFilterHandler( filter, bb ) );

        // Adding eos to bb
        apr_bucket_alloc_t *bA = apr_bucket_alloc_create(pool);
        apr_bucket *e = apr_bucket_eos_create(bA);
        CPPUNIT_ASSERT(e);
        APR_BRIGADE_INSERT_TAIL(bb, e);

        apr_table_set(req->headers_in, "UNIQUE_ID", "toto");

        CPPUNIT_ASSERT_EQUAL( APR_SUCCESS, outputFilterHandler( filter, bb ) );
    }

}

