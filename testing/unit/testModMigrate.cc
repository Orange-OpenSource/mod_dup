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
    CPPUNIT_ASSERT_EQUAL(2, enrichContext(req,info));

    // in Body
    info.mArgs.clear();
    info.mHeader.clear();
    info.mBody = "myRegexbalbaglsdfsdr";
    CPPUNIT_ASSERT_EQUAL(2, enrichContext(req,info));

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

void TestModMigrate::testInputFilterBody2Brigade() {

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
