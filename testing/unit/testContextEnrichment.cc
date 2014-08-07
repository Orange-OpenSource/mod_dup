#if 0

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

#include "RequestInfo.hh"
#include "RequestProcessor.hh"
#include "Utils.hh"

#include <httpd.h>
#include <http_config.h>
#include <http_request.h>
#include <http_protocol.h>

#include "MultiThreadQueue.hh"
#include "testContextEnrichment.hh"
#include "testModDup.hh"

#include "mod_dup.hh"

// cppunit
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>
#include <boost/shared_ptr.hpp>

CPPUNIT_TEST_SUITE_REGISTRATION( TestContextEnrichment );

extern module AP_DECLARE_DATA dup_module;

using namespace DupModule;

static boost::shared_ptr<RequestInfo> POISON_REQUEST(new RequestInfo());

void TestContextEnrichment::setUp() {
    mParms = new cmd_parms;
    server_rec * lServer = new server_rec;

    memset(mParms, 0, sizeof(cmd_parms));
    memset(lServer, 0, sizeof(server_rec));
    mParms->server = lServer;
    apr_pool_create(&mParms->pool, 0);

    // Complete init
    registerHooks(mParms->pool);
    gProcessor = new RequestProcessor();
    CPPUNIT_ASSERT(gProcessor);
    gThreadPool = new DummyThreadPool<boost::shared_ptr<RequestInfo> >(boost::bind(&RequestProcessor::run, gProcessor, _1), POISON_REQUEST);
    CPPUNIT_ASSERT(gThreadPool);

    childInit(mParms->pool, mParms->server);
    preConfig(NULL, NULL, NULL);
    postConfig(mParms->pool, mParms->pool, mParms->pool, mParms->server);

}

void TestContextEnrichment::tearDown() {
    delete gThreadPool;
    delete gProcessor;
    delete mParms;
}

void TestContextEnrichment::testConfiguration() {

    mParms->path = new char[10];
    strcpy(mParms->path, "/spp/main");

    // Pointer to a boolean meant to activate the module on a given path
    DupConf *conf = new DupConf();


    // Correct insert
    CPPUNIT_ASSERT(!setEnrichContext(mParms, (void *)&conf,
                                     "myVar", "toMatch", "toSet"));

    // Invalid regex
    CPPUNIT_ASSERT(setEnrichContext(mParms, (void *)&conf,
                                     "myVar", "toM**(atch", "toSet"));


    CPPUNIT_ASSERT_EQUAL((size_t)1, gProcessor->mCommands[mParms->path].mEnrichContext.size());
}

void TestContextEnrichment::testEnrichBody() {

    mParms->path = new char[10];
    strcpy(mParms->path, "/spp/main");

    DupConf *conf = new DupConf();
    // Insertion of an enrichment on body
    conf->currentApplicationScope = ApplicationScope::HEADER;
    CPPUNIT_ASSERT(!setEnrichContext(mParms, (void *)conf,
                                     "myVar", "value1", "toSet"));

    // Creation of a request object as it would be filled by the earlyhook filter
    RequestInfo *info = new RequestInfo(std::string("42"));

    info->mConfPath = mParms->path;
    info->mArgs = "&arg1=valueNOT&arg2=value2";
    info->mBody = "Hey value1, you want me to shoot this guy?";

    // No match (value to find is in the body, not in the header)
    CPPUNIT_ASSERT_EQUAL(0, gProcessor->enrichContext(NULL, *info));

    // Insertion of an enrichment on header
    conf->currentApplicationScope = ApplicationScope::BODY;
    CPPUNIT_ASSERT(!setEnrichContext(mParms, (void *)conf,
                                     "myVar", "shoot", "toSet"));

    // No match (value to find is in the header, not in the body)
    CPPUNIT_ASSERT_EQUAL(1, gProcessor->enrichContext(NULL, *info));
}

void TestContextEnrichment::testEnrichBoth() {

    mParms->path = new char[10];
    strcpy(mParms->path, "/spp/main");

    DupConf *conf = new DupConf();

    // Enrich in both parts of the request
    conf->currentApplicationScope = ApplicationScope::ALL;
    CPPUNIT_ASSERT(!setEnrichContext(mParms, (void *)conf,
                                     "myVar", "Hey Joe", "toSet"));

    // Creation of a request object as it would be filled by the earlyhook filter
    RequestInfo *info = new RequestInfo(std::string("42"));

    info->mConfPath = mParms->path;

    // Match nowhere
    info->mArgs = "&arg1=value1&arg2=value2";
    info->mBody = "you want me to shoot this guy?";

    CPPUNIT_ASSERT_EQUAL(0, gProcessor->enrichContext(NULL, *info));

    // Match in the body
    info->mBody = "Hey Joe, you want me to shoot this guy?";
    CPPUNIT_ASSERT_EQUAL(1, gProcessor->enrichContext(NULL, *info));

    // Match in the header only
    info->mArgs = "&arg1=value1&arg2=Hey Joe";
    info->mBody = "you want me to shoot this guy?";
    CPPUNIT_ASSERT_EQUAL(1, gProcessor->enrichContext(NULL, *info));

    // Match in the header and body
    info->mArgs = "&arg1=value1&arg2=Hey Joe";
    info->mBody = "Hey Joe, you want me to shoot this guy?";
    CPPUNIT_ASSERT_EQUAL(2, gProcessor->enrichContext(NULL, *info));
}

#endif
