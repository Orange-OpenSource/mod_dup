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

#include "RequestProcessor.hh"
#define CORE_PRIVATE
#include <httpd.h>
#include <http_config.h>
#include <http_request.h>
#include <http_protocol.h>

#include "MultiThreadQueue.hh"
#include "testFilters.hh"
#include "testModDup.hh"
#include "testBodies.hh"
#include "mod_dup.hh"

// cppunit
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

CPPUNIT_TEST_SUITE_REGISTRATION( TestFilters );

extern module AP_DECLARE_DATA dup_module;

using namespace DupModule;

void TestFilters::setUp() {
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
    gThreadPool = new DummyThreadPool<RequestInfo *>(boost::bind(&RequestProcessor::run, gProcessor, _1), &POISON_REQUEST);
    CPPUNIT_ASSERT(gThreadPool);

    childInit(mParms->pool, mParms->server);
    preConfig(NULL, NULL, NULL);
    postConfig(mParms->pool, mParms->pool, mParms->pool, mParms->server);

}

void TestFilters::tearDown() {
    delete gThreadPool;
    delete gProcessor;
    delete mParms;
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


void TestFilters::translateHook() {

{
    // TESTS ALL THE DECLINED POSSIBILITIES AND THE EMPTY REQUEST
    request_rec *req = new request_rec;
    memset(req, 0, sizeof(*req));

    // No per_dir_config
    CPPUNIT_ASSERT_EQUAL(DECLINED, DupModule::translateHook(req));


    DupConf *conf = new DupConf;
    apr_pool_t *pool = NULL;
    apr_pool_create(&pool, 0);
    req->per_dir_config = (ap_conf_vector_t *)apr_pcalloc(pool, sizeof(void *) * 42);
    req->request_config = (ap_conf_vector_t *)apr_pcalloc(pool, sizeof(void *) * 42);
    req->connection = (conn_rec *)apr_pcalloc(pool, sizeof(*(req->connection)));
    ap_set_module_config(req->per_dir_config, &dup_module, conf);
    // Conf without dirName set in per_dir_config
    CPPUNIT_ASSERT_EQUAL(DECLINED, DupModule::translateHook(req));

    // Conf in an active location
    conf->dirName = strdup("/spp/main");
    CPPUNIT_ASSERT_EQUAL(DECLINED, DupModule::translateHook(req));

    req->connection->pool = pool;
    req->headers_in = apr_table_make(pool, 42);
    req->headers_out = apr_table_make(pool, 42);
    CPPUNIT_ASSERT_EQUAL(DECLINED, DupModule::translateHook(req));
    delete conf;
 }

 {
     // NOMINAL CASE REQUEST + BODY
     DupConf *conf = new DupConf;
     request_rec *req = prep_request_rec();
     ap_set_module_config(req->per_dir_config, &dup_module, conf);
     conf->dirName = strdup("/spp/main");

     req->input_filters = (ap_filter_t *)(void *) 0x42;
     CPPUNIT_ASSERT_EQUAL(DECLINED, DupModule::translateHook(req));

     RequestInfo *info = reinterpret_cast<RequestInfo *>(ap_get_module_config(req->request_config, &dup_module));
     CPPUNIT_ASSERT(info);

     CPPUNIT_ASSERT_EQUAL(std::string(testBody42), info->mBody);
     delete conf;
 }

 {
     // NOMINAL CASE REQUEST + MASSIVE BODY
     DupConf *conf = new DupConf;
     request_rec *req = prep_request_rec();
     ap_set_module_config(req->per_dir_config, &dup_module, conf);
     conf->dirName = strdup("/spp/main");
     bodyServed = 0;

     req->input_filters = (ap_filter_t *)(void *) 0x43;
     CPPUNIT_ASSERT_EQUAL(DECLINED, DupModule::translateHook(req));

     RequestInfo *info = reinterpret_cast<RequestInfo *>(ap_get_module_config(req->request_config, &dup_module));
     CPPUNIT_ASSERT(info);

     CPPUNIT_ASSERT_EQUAL(std::string(testBody43p1) + std::string(testBody43p2), info->mBody);
     delete conf;
 }

}

template<class T>
T *memSet(T *addr, char c = 0) {
    memset(addr, c, sizeof(T));
    return addr;
}

namespace DupModule {
bool extractBrigadeContent(apr_bucket_brigade *bb, request_rec *pRequest, std::string &content);
};


void TestFilters::inputFilterBody2BrigadeTest() {

{
    // NO PER_DIR_CONFIG
    request_rec *req = prep_request_rec();

    ap_filter_t *filter = new ap_filter_t;
    memSet(filter);
    apr_pool_t *pool = NULL;
    apr_pool_create(&pool, 0);
    filter->r = req;
    filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
    filter->c->bucket_alloc = apr_bucket_alloc_create(pool);
    apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);
    req->per_dir_config = NULL;
    CPPUNIT_ASSERT_EQUAL(APR_SUCCESS, inputFilterBody2Brigade(filter, bb, AP_MODE_READBYTES,
                                                           APR_BLOCK_READ, 8192));
 }

 {
    // MISSING INFO CASE
    request_rec *req = prep_request_rec();

    ap_filter_t *filter = new ap_filter_t;
    memSet(filter);
    apr_pool_t *pool = NULL;
    apr_pool_create(&pool, 0);
    filter->r = req;
    filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
    filter->c->bucket_alloc = apr_bucket_alloc_create(pool);
    apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);
    CPPUNIT_ASSERT_EQUAL(APR_SUCCESS, inputFilterBody2Brigade(filter, bb, AP_MODE_READBYTES,
                                                           APR_BLOCK_READ, 8192));
 }

 {
     // NOMINAL CASE WITH A SMALL BODY
     request_rec *req = prep_request_rec();

     ap_filter_t *filter = new ap_filter_t;
     memSet(filter);
     apr_pool_t *pool = NULL;
     apr_pool_create(&pool, 0);
     filter->r = req;
     filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
     filter->c->bucket_alloc = apr_bucket_alloc_create(pool);
     apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);

     RequestInfo *info = new RequestInfo(42);
     ap_set_module_config(req->request_config, &dup_module, (void *)info);

     info->mBody = testBody42;
     CPPUNIT_ASSERT_EQUAL(APR_SUCCESS, inputFilterBody2Brigade(filter, bb, AP_MODE_READBYTES,
                                                               APR_BLOCK_READ, 8192));


     // Compare the brigade content to what should have been sent
     std::string result;
     extractBrigadeContent(bb, req, result);
     CPPUNIT_ASSERT_EQUAL(result, std::string(testBody42));
 }

 {
     // NOMINAL CASE WITH A MASSIVE BODY
     request_rec *req = prep_request_rec();

     ap_filter_t *filter = new ap_filter_t;
     memSet(filter);
     apr_pool_t *pool = NULL;
     apr_pool_create(&pool, 0);
     filter->r = req;
     filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
     filter->c->bucket_alloc = apr_bucket_alloc_create(pool);
     apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);

     RequestInfo *info = new RequestInfo(42);
     ap_set_module_config(req->request_config, &dup_module, (void *)info);

     info->mBody = testBody43p1;
     info->mBody += testBody43p2;
     CPPUNIT_ASSERT_EQUAL(APR_SUCCESS, inputFilterBody2Brigade(filter, bb, AP_MODE_READBYTES,
                                                               APR_BLOCK_READ, 8192));


     // Compare the brigade content to what should have been sent
     std::string result;
     extractBrigadeContent(bb, req, result);
     CPPUNIT_ASSERT_EQUAL(result, std::string(testBody43p1) + std::string(testBody43p2));
 }
}

void TestFilters::outputFilterHandlerTest() {
{
    // NO PER_DIR_CONFIG
    request_rec *req = prep_request_rec();

    ap_filter_t *filter = new ap_filter_t;
    memSet(filter);
    apr_pool_t *pool = NULL;
    apr_pool_create(&pool, 0);
    filter->r = req;
    filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
    filter->c->bucket_alloc = apr_bucket_alloc_create(pool);
    apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);
    req->per_dir_config = NULL;
    CPPUNIT_ASSERT_EQUAL(APR_SUCCESS, outputFilterHandler(filter, bb));
 }

{
    // NO CONF
    request_rec *req = prep_request_rec();

    ap_filter_t *filter = new ap_filter_t;
    memSet(filter);
    apr_pool_t *pool = NULL;
    apr_pool_create(&pool, 0);
    filter->r = req;
    filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
    filter->c->bucket_alloc = apr_bucket_alloc_create(pool);
    apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);
    CPPUNIT_ASSERT_EQUAL(APR_SUCCESS, outputFilterHandler(filter, bb));
 }

{
    // NOMINAL TEST DUPLICATION TYPE == NONE
    request_rec *req = prep_request_rec();

    ap_filter_t *filter = new ap_filter_t;
    memSet(filter);
    apr_pool_t *pool = NULL;
    apr_pool_create(&pool, 0);
    filter->r = req;
    filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
    filter->c->bucket_alloc = apr_bucket_alloc_create(pool);
    apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);

    DupConf *conf = new DupConf();
    conf->dirName = strdup("/spp/main");
    ap_set_module_config(req->per_dir_config, &dup_module, conf);
    CPPUNIT_ASSERT_EQUAL(APR_SUCCESS, outputFilterHandler(filter, bb));
 }

{
    // NOMINAL TEST DUPLICATION TYPE != REQUEST_WITH_ANSWER
    request_rec *req = prep_request_rec();
    req->uri = "/spp/main/test.cgi";
    ap_filter_t *filter = new ap_filter_t;
    memSet(filter);
    apr_pool_t *pool = NULL;
    apr_pool_create(&pool, 0);
    filter->r = req;
    filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
    filter->c->bucket_alloc = apr_bucket_alloc_create(pool);
    apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);

    DupConf *conf = new DupConf();
    conf->dirName = strdup("/spp/main");
    conf->setCurrentDuplicationType(DuplicationType::HEADER_ONLY);
    ap_set_module_config(req->per_dir_config, &dup_module, conf);

    RequestInfo *info = new RequestInfo(42);
    // Backup in request context
    ap_set_module_config(req->request_config, &dup_module, (void *)info);

    CPPUNIT_ASSERT_EQUAL(APR_SUCCESS, outputFilterHandler(filter, bb));
    // Second call, tests context backup
    CPPUNIT_ASSERT_EQUAL(APR_SUCCESS, outputFilterHandler(filter, bb));
 }

{
    // NOMINAL TEST DUPLICATION TYPE == REQUEST_WITH_ANSWER
    // small payload
    request_rec *req = prep_request_rec();
    req->uri = "/spp/main/test.cgi";
    ap_filter_t *filter = new ap_filter_t;
    memSet(filter);
    apr_pool_t *pool = NULL;
    apr_pool_create(&pool, 0);
    filter->r = req;
    filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
    filter->c->bucket_alloc = apr_bucket_alloc_create(pool);

    DupConf *conf = new DupConf();
    conf->dirName = strdup("/spp/main");
    conf->setCurrentDuplicationType(DuplicationType::REQUEST_WITH_ANSWER);
    ap_set_module_config(req->per_dir_config, &dup_module, conf);

    RequestInfo *info = new RequestInfo(42);
    // Backup in request context
    ap_set_module_config(req->request_config, &dup_module, (void *)info);

    // Setting answer to read
    apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);
    CPPUNIT_ASSERT_EQUAL(APR_SUCCESS, apr_brigade_write(bb, NULL, NULL, testBody42, std::string(testBody42).size()));

    // First call, must extract this data
    CPPUNIT_ASSERT_EQUAL(APR_SUCCESS, outputFilterHandler(filter, bb));

    // Adding eos to bb
    apr_bucket_alloc_t *bA = apr_bucket_alloc_create(pool);
    apr_bucket *e = apr_bucket_eos_create(bA);
    CPPUNIT_ASSERT(e);
    APR_BRIGADE_INSERT_TAIL(bb, e);

    // Second call, tests context backup
    CPPUNIT_ASSERT_EQUAL(APR_SUCCESS, outputFilterHandler(filter, bb));

    CPPUNIT_ASSERT_EQUAL(std::string(testBody42), info->mAnswer);

 }

{
    // NOMINAL TEST DUPLICATION TYPE == REQUEST_WITH_ANSWER
    // Big payload and answer headers
    request_rec *req = prep_request_rec();
    req->uri = "/spp/main/test.cgi";
    ap_filter_t *filter = new ap_filter_t;
    memSet(filter);
    apr_pool_t *pool = NULL;
    apr_pool_create(&pool, 0);
    filter->r = req;
    filter->c = (conn_rec *)apr_pcalloc(pool, sizeof(*(filter->c)));
    filter->c->bucket_alloc = apr_bucket_alloc_create(pool);

    DupConf *conf = new DupConf();
    conf->dirName = strdup("/spp/main");
    conf->setCurrentDuplicationType(DuplicationType::REQUEST_WITH_ANSWER);
    ap_set_module_config(req->per_dir_config, &dup_module, conf);

    RequestInfo *info = new RequestInfo(42);
    // Backup in request context
    ap_set_module_config(req->request_config, &dup_module, (void *)info);
    // Adding some headers out
    apr_table_set(req->headers_out, "KeyOut1", "value1");
    apr_table_set(req->headers_out, "KeyOut2", "value2");
    apr_table_set(req->headers_out, "KeyOut3", "value3");

    // Setting answer to read
    apr_bucket_brigade *bb = apr_brigade_create(req->connection->pool, req->connection->bucket_alloc);
    // Sending part one
    CPPUNIT_ASSERT_EQUAL(APR_SUCCESS, apr_brigade_write(bb, NULL, NULL, testBody43p1, std::string(testBody43p1).size()));
    CPPUNIT_ASSERT_EQUAL(APR_SUCCESS, outputFilterHandler(filter, bb));

    // Sending part two
    CPPUNIT_ASSERT_EQUAL(APR_SUCCESS, apr_brigade_write(bb, NULL, NULL, testBody43p2, std::string(testBody43p2).size()));
    CPPUNIT_ASSERT_EQUAL(APR_SUCCESS, outputFilterHandler(filter, bb));

    // Sending eos
    apr_bucket_alloc_t *bA = apr_bucket_alloc_create(pool);
    apr_bucket *e = apr_bucket_eos_create(bA);
    CPPUNIT_ASSERT(e);
    APR_BRIGADE_INSERT_TAIL(bb, e);
    CPPUNIT_ASSERT_EQUAL(APR_SUCCESS, outputFilterHandler(filter, bb));

    CPPUNIT_ASSERT_EQUAL(std::string(testBody43p1) + std::string(testBody43p2),
                         info->mAnswer);


    struct MyComp {
        MyComp(const char *k, const char *v)
            : key(k), value(v) {
        }

        bool operator()(const std::pair<std::string, std::string > &elt) {
            return (elt.first == key && elt.second == value);
        }

        const char *key;
        const char *value;
    };

    CPPUNIT_ASSERT(info->mHeadersOut.end() != std::find_if(info->mHeadersOut.begin(), info->mHeadersOut.end(), MyComp("KeyOut1", "value1")));
    CPPUNIT_ASSERT(info->mHeadersOut.end() != std::find_if(info->mHeadersOut.begin(), info->mHeadersOut.end(), MyComp("KeyOut2", "value2")));
    CPPUNIT_ASSERT(info->mHeadersOut.end() != std::find_if(info->mHeadersOut.begin(), info->mHeadersOut.end(), MyComp("KeyOut3", "value3")));

 }


}
