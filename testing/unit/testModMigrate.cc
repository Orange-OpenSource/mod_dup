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

#include "testModMigrate.hh"
#include "mod_migrate.hh"
#include "Utils.hh"
#include "testBodies.hh"
#include <string>

// cppunit
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>
#include "TfyTestRunner.hh"

CPPUNIT_TEST_SUITE_REGISTRATION( TestModMigrate );

using namespace MigrateModule;
using namespace CommonModule;

static request_rec *prep_request_rec() {
    request_rec *req = new request_rec;
    memset(req, 0, sizeof(*req));
    apr_pool_create(&req->pool, 0);
    req->per_dir_config = (ap_conf_vector_t *)apr_pcalloc(req->pool, sizeof(void *) * 64);
    req->request_config = (ap_conf_vector_t *)apr_pcalloc(req->pool, sizeof(void *) * 64);
    req->connection = (conn_rec *)apr_pcalloc(req->pool, sizeof(*(req->connection)));
    req->connection->bucket_alloc = apr_bucket_alloc_create(req->pool);
    apr_pool_create(&req->connection->pool, 0);

    req->headers_in = apr_table_make(req->pool, 64);
    req->headers_out = apr_table_make(req->pool, 64);
    return req;
}


static void setConf(MigrateConf& conf, const std::string& pStr) {
    conf.mDirName = strdup("/location1");

    conf.mEnvLists["location1"].push_back(MigrateConf::MigrateEnv{"var1",boost::regex(pStr,boost::regex_constants::icase),"set1",ApplicationScope::ALL});
    conf.mEnvLists["location1"].push_back(MigrateConf::MigrateEnv{"var2",boost::regex(pStr,boost::regex_constants::icase),"set2",ApplicationScope::URL});
    conf.mEnvLists["location1"].push_back(MigrateConf::MigrateEnv{"var3",boost::regex(pStr,boost::regex_constants::icase),"set3",ApplicationScope::HEADER});
    conf.mEnvLists["location1"].push_back(MigrateConf::MigrateEnv{"var4",boost::regex(pStr,boost::regex_constants::icase),"set4",ApplicationScope::BODY});
}

void TestModMigrate::testEnrichContext()
{
    request_rec *req = new request_rec;
    memset(req, 0, sizeof(*req));
    apr_pool_create(&req->pool, 0);
    req->per_dir_config = (ap_conf_vector_t *)apr_pcalloc(req->pool, sizeof(void *) * 64);

    MigrateConf conf;
    setConf(conf,"myRegex");
    ap_set_module_config(req->per_dir_config,&migrate_module,&conf);
    CPPUNIT_ASSERT(reinterpret_cast<MigrateConf *>(ap_get_module_config(req->per_dir_config, &migrate_module)));

    RequestInfo info("123456");
    info.mConfPath = std::string("location1");

    // in URL
    info.mArgs = "myRegexsdfwhgtdwhoij";
    // 2 because of ALL and URL
    CPPUNIT_ASSERT_EQUAL(2, enrichContext(req,info));

    // in Header
    info.mArgs.clear();
    info.mHeader = "myRegexbalbaglsdfsdr";
    // 2 because of ALL and HEADER
    CPPUNIT_ASSERT_EQUAL(2, enrichContext(req,info));

    // in Body
    info.mArgs.clear();
    info.mHeader.clear();
    info.mBody = "myRegexbalbaglsdfsdr";
    // 2 because of ALL and BODY
    CPPUNIT_ASSERT_EQUAL(2, enrichContext(req,info));

    // in URL, HEADER and BODY
    info.mArgs = "myRegexsdfwhgtdwhoij";
    info.mHeader = "myRegexbalbaglsdfsdr";
    info.mBody = "sdfsdfesrtdfrg xdmyRegexbalbaglsdfsdr";
    // 6 because of ALL, BODY, HEADER and URL (3 times for ALL scope + 3*1 for each scope (all, body and url)
    CPPUNIT_ASSERT_EQUAL(6, enrichContext(req,info));

    delete req;
}

void TestModMigrate::testTranslateHook() {
    {
        // TESTS ALL THE DECLINED POSSIBILITIES AND THE EMPTY REQUEST
        request_rec *req = new request_rec;
        memset(req, 0, sizeof(*req));

        // No per_dir_config
        CPPUNIT_ASSERT_EQUAL(DECLINED, translateHook(req));


        MigrateConf *conf = new MigrateConf;
        apr_pool_t *pool = NULL;
        apr_pool_create(&pool, 0);
        req->per_dir_config = (ap_conf_vector_t *)apr_pcalloc(pool, sizeof(void *) * 42);
        req->request_config = (ap_conf_vector_t *)apr_pcalloc(pool, sizeof(void *) * 42);
        req->connection = (conn_rec *)apr_pcalloc(pool, sizeof(*(req->connection)));
        req->pool = pool;
        ap_set_module_config(req->per_dir_config, &migrate_module, conf);
        // Conf without dirName set in per_dir_config
        CPPUNIT_ASSERT_EQUAL(DECLINED, translateHook(req));

        // Conf in an active location
        conf->mDirName = strdup("/spp/main");
        CPPUNIT_ASSERT_EQUAL(DECLINED, translateHook(req));

        req->connection->pool = pool;
        req->headers_in = apr_table_make(pool, 42);
        req->headers_out = apr_table_make(pool, 42);
        CPPUNIT_ASSERT_EQUAL(DECLINED, translateHook(req));
        delete conf;
    }

    { // HEADER + BODY TEST
        request_rec *req = prep_request_rec();

        MigrateConf conf;
        setConf(conf,"myRegex");
        ap_set_module_config(req->per_dir_config,&migrate_module,&conf);
        CPPUNIT_ASSERT(reinterpret_cast<MigrateConf *>(ap_get_module_config(req->per_dir_config, &migrate_module)));

        req->input_filters = (ap_filter_t *)(void *) 0x42;
        CPPUNIT_ASSERT_EQUAL(DECLINED,translateHook(req));
        boost::shared_ptr<RequestInfo> *shPtr = reinterpret_cast<boost::shared_ptr<RequestInfo> *>(ap_get_module_config(req->request_config, &migrate_module));
        CPPUNIT_ASSERT(shPtr->get());
        RequestInfo *info = shPtr->get();

        const char* uidIn = apr_table_get(req->headers_in, c_UNIQUE_ID);
        const char* uidOut = apr_table_get(req->headers_out, c_UNIQUE_ID);

        CPPUNIT_ASSERT_EQUAL(info->mId, std::string(uidIn));
        CPPUNIT_ASSERT_EQUAL(info->mId, std::string(uidOut));

        CPPUNIT_ASSERT_EQUAL(std::string(testBody42), info->mBody);

        delete req;
    }

    { // HEADER + MASSIVE BODY TEST
        request_rec *req = prep_request_rec();

        MigrateConf conf;
        setConf(conf,"myRegex");
        ap_set_module_config(req->per_dir_config,&migrate_module,&conf);
        CPPUNIT_ASSERT(reinterpret_cast<MigrateConf *>(ap_get_module_config(req->per_dir_config, &migrate_module)));

        req->input_filters = (ap_filter_t *)(void *) 0x43;
        CPPUNIT_ASSERT_EQUAL(DECLINED,translateHook(req));
        boost::shared_ptr<RequestInfo> *shPtr = reinterpret_cast<boost::shared_ptr<RequestInfo> *>(ap_get_module_config(req->request_config, &migrate_module));
        CPPUNIT_ASSERT(shPtr->get());
        RequestInfo *info = shPtr->get();

        const char* uidIn = apr_table_get(req->headers_in, c_UNIQUE_ID);
        const char* uidOut = apr_table_get(req->headers_out, c_UNIQUE_ID);

        CPPUNIT_ASSERT_EQUAL(info->mId, std::string(uidIn));
        CPPUNIT_ASSERT_EQUAL(info->mId, std::string(uidOut));

        CPPUNIT_ASSERT_EQUAL(std::string(testBody43p1)+std::string(testBody43p2), info->mBody);

        delete req;
    }
}

template<class T> static T *memSet(T *addr, char c = 0) {
    memset(addr, c, sizeof(T));
    return addr;
}

void TestModMigrate::testInputFilterBody2Brigade() {
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
        RequestInfo *info = new RequestInfo(std::string("42"));
        boost::shared_ptr<RequestInfo> shPtr(info);
        ap_set_module_config(req->request_config, &migrate_module, (void *)&shPtr);

        info->mBody = testBody42;
        CPPUNIT_ASSERT_EQUAL(APR_SUCCESS, inputFilterBody2Brigade(filter, bb, AP_MODE_READBYTES,
                APR_BLOCK_READ, 8192));


        // Compare the brigade content to what should have been sent
        std::string result;
        extractBrigadeContent(bb, req->input_filters, result);
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

        RequestInfo *info = new RequestInfo(std::string("42"));
        boost::shared_ptr<RequestInfo> shPtr(info);
        ap_set_module_config(req->request_config, &migrate_module, (void *)&shPtr);

        info->mBody = testBody43p1;
        info->mBody += testBody43p2;
        bodyServed = 0;
        std::string result;
        bool done = false;
        std::cout << "Massive body: " << info->mBody.size() << std::endl;
        while (!done) {
            apr_brigade_cleanup(bb);
            CPPUNIT_ASSERT_EQUAL(APR_SUCCESS, inputFilterBody2Brigade(filter, bb, AP_MODE_READBYTES,
                    APR_BLOCK_READ, 8192));
            for (apr_bucket *b = APR_BRIGADE_FIRST(bb);
                    b != APR_BRIGADE_SENTINEL(bb);
                    b = APR_BUCKET_NEXT(b) ) {
                // Metadata end of stream
                if (APR_BUCKET_IS_EOS(b)) {
                    done = true;
                }
                if (APR_BUCKET_IS_METADATA(b))
                    continue;
                const char *data = 0;
                apr_size_t len = 0;
                apr_status_t rv = apr_bucket_read(b, &data, &len, APR_BLOCK_READ);
                if (len) {
                    result.append(data, len);
                }
            }
        }
        // Compare the brigade content to what should have been sent
        CPPUNIT_ASSERT_EQUAL(std::string(testBody43p1).size() + std::string(testBody43p2).size(), result.size());
        CPPUNIT_ASSERT_EQUAL(result, std::string(testBody43p1) + std::string(testBody43p2));
    }
}

static cmd_parms * getParms() {
    cmd_parms * lParms = new cmd_parms;
    server_rec * lServer = new server_rec;
    memset(lParms, 0, sizeof(cmd_parms));
    memset(lServer, 0, sizeof(server_rec));
    lParms->server = lServer;
    apr_pool_create(&lParms->pool, 0);
    return lParms;
}

void TestModMigrate::testConfig()
{
    cmd_parms * lParms = getParms();
    lParms->path = new char[10];
    strcpy(lParms->path, "/spp/main");
    // Pointer to a boolean meant to activate the module on a given path
    MigrateConf *lDoHandle = new MigrateConf();

    CPPUNIT_ASSERT(!setActive(lParms, lDoHandle));

    delete [] lParms->path;
}

void TestModMigrate::testScope()
{
    cmd_parms * lParms = getParms();
    lParms->path = new char[10];
    strcpy(lParms->path, "/spp/main");
    MigrateConf *conf = new MigrateConf();

    // Default value
    CPPUNIT_ASSERT_EQUAL(ApplicationScope::ALL, conf->mCurrentApplicationScope);

    // Switching to HEADER
    CPPUNIT_ASSERT(!setApplicationScope(lParms, (void *)conf, "HEADER"));
    CPPUNIT_ASSERT_EQUAL(ApplicationScope::HEADER, conf->mCurrentApplicationScope);

    // Switching to ALL
    CPPUNIT_ASSERT(!setApplicationScope(lParms, (void *)conf, "ALL"));
    CPPUNIT_ASSERT_EQUAL(ApplicationScope::ALL, conf->mCurrentApplicationScope);

    // Switching to URL
    CPPUNIT_ASSERT(!setApplicationScope(lParms, (void *)conf, "URL"));
    CPPUNIT_ASSERT_EQUAL(ApplicationScope::URL, conf->mCurrentApplicationScope);

    // Switching to BODY
    CPPUNIT_ASSERT(!setApplicationScope(lParms, (void *)conf, "BODY"));
    CPPUNIT_ASSERT_EQUAL(ApplicationScope::BODY, conf->mCurrentApplicationScope);

    // Incorrect value
    CPPUNIT_ASSERT(setApplicationScope(lParms, (void *)conf, "incorrect_vALUE"));
}

void TestModMigrate::testMigrateEnv()
{
    cmd_parms * lParms = getParms();
    lParms->path = new char[10];
    strcpy(lParms->path, "/spp/main");
    MigrateConf *conf = new MigrateConf();

    // Adding multiple MigrateEnv
    CPPUNIT_ASSERT(!setMigrateEnv(lParms, (void *)conf, "varname", "regex", "value"));
    CPPUNIT_ASSERT_EQUAL(std::string("varname"), conf->mEnvLists["/spp/main"].back().mVarName);
    CPPUNIT_ASSERT_EQUAL(boost::regex("regex",boost::regex_constants::icase), conf->mEnvLists["/spp/main"].back().mMatchRegex);
    CPPUNIT_ASSERT_EQUAL(std::string("value"), conf->mEnvLists["/spp/main"].back().mSetValue);

    CPPUNIT_ASSERT(!setMigrateEnv(lParms, (void *)conf, "varname2", "regex2", "value2"));
    CPPUNIT_ASSERT_EQUAL(std::string("varname2"), conf->mEnvLists["/spp/main"].back().mVarName);
    CPPUNIT_ASSERT_EQUAL(boost::regex("regex2",boost::regex_constants::icase), conf->mEnvLists["/spp/main"].back().mMatchRegex);
    CPPUNIT_ASSERT_EQUAL(std::string("value2"), conf->mEnvLists["/spp/main"].back().mSetValue);

    CPPUNIT_ASSERT(!setMigrateEnv(lParms, (void *)conf, "varname3", "regex3", "value3"));
    CPPUNIT_ASSERT_EQUAL(std::string("varname3"), conf->mEnvLists["/spp/main"].back().mVarName);
    CPPUNIT_ASSERT_EQUAL(boost::regex("regex3",boost::regex_constants::icase), conf->mEnvLists["/spp/main"].back().mMatchRegex);
    CPPUNIT_ASSERT_EQUAL(std::string("value3"), conf->mEnvLists["/spp/main"].back().mSetValue);
}

class Dummy {
public:
    Dummy(int &r) : mR(r) {
        mR = 0;
    }

    ~Dummy(){
        mR = 42;
    }

    int &mR;
};

void TestModMigrate::testInitAndCleanUp()
{
    cmd_parms * lParms = getParms();
    registerHooks(lParms->pool);
    postConfig(lParms->pool, lParms->pool, lParms->pool, lParms->server);

    // Cleaner test
    int test;
    Dummy d(test);
    cleaner<Dummy>((void *)&d);
    CPPUNIT_ASSERT_EQUAL(42, test);

}


#ifdef UNIT_TESTING
//--------------------------------------
// the main method
//--------------------------------------
int main(int argc, char* argv[])
{
    Log::init();

    apr_initialize();
    TfyTestRunner runner(argv[0]);
    runner.addTest(CppUnit::TestFactoryRegistry::getRegistry().makeTest());
    bool failed = runner.run();

    return !failed;
}
#endif
