#include "RequestProcessor.hh"
#include <httpd.h>
#include <http_config.h>
#include <http_request.h>
#include <http_protocol.h>

#include "MultiThreadQueue.hh"
#include "testModDup.hh"
#include "mod_dup.hh"

// cppunit
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

CPPUNIT_TEST_SUITE_REGISTRATION( TestModDup );

extern module AP_DECLARE_DATA dup_module;

using namespace DupModule;

namespace DupModule {

RequestProcessor *gProcessor;
ThreadPool<RequestInfo> *gThreadPool;
std::set<std::string> gActiveLocations;


template <typename QueueT>
class DummyThreadPool : public ThreadPool<QueueT> {
public:
    typedef boost::function1<void, MultiThreadQueue<QueueT> &> tQueueWorker;

    std::list<QueueT> mDummyQueued;

    //DummyThreadPool(int pWorker, const QueueT &pPoisonItem) : ThreadPool<QueueT>(pWorker, pPoisonItem) {
    DummyThreadPool(tQueueWorker pWorker, const QueueT &pPoisonItem) : ThreadPool<QueueT>(pWorker, pPoisonItem) {
    }

    void
    push(const QueueT &pItem) {
        mDummyQueued.push_back(pItem);
    }
};

}

cmd_parms * TestModDup::getParms() {
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


void TestModDup::testInit()
{
    preConfig(NULL, NULL, NULL);
    CPPUNIT_ASSERT(gProcessor);
    CPPUNIT_ASSERT(gThreadPool);

    gThreadPool = new DummyThreadPool<RequestInfo>(boost::bind(&RequestProcessor::run, gProcessor, _1), POISON_REQUEST);
}

void TestModDup::testInitAndCleanUp()
{
    cmd_parms * lParms = getParms();
    registerHooks(lParms->pool);
    childInit(lParms->pool, lParms->server);
    postConfig(lParms->pool, lParms->pool, lParms->pool, lParms->server);
    cleanUp(NULL);
    CPPUNIT_ASSERT(!gProcessor);
    CPPUNIT_ASSERT(!gThreadPool);
}

void TestModDup::testRequestHandler()
{
    // THIS TEST IS PRETTY MUCH INVALID AS THE CALLS RELY ON APACHE REQUEST FILTERING NOW
    // request_rec lReq;
    // memset(&lReq, 0, sizeof(request_rec));
    // cmd_parms * lParms = getParms();
    // lReq.server = lParms->server;
    // lParms->path = new char[10];
    // strcpy(lParms->path, "/spp/main");

    // gActiveLocations.clear();

    // lReq.handler = NULL;
    // lReq.per_dir_config = reinterpret_cast<ap_conf_vector_t *>(apr_pcalloc(lParms->pool, sizeof(void *) * 1000));

    // DummyThreadPool<RequestInfo> *gDummyThreadPool = dynamic_cast<DummyThreadPool<RequestInfo> *>(gThreadPool);

    // size_t lQueued = 0;

    // gDummyThreadPool->mDummyQueued.clear();
    // CPPUNIT_ASSERT_EQUAL(lQueued, gDummyThreadPool->mDummyQueued.size());

    // // Path not active yet
    // DupModule::pushRequest("/spp/main", "");
    // CPPUNIT_ASSERT_EQUAL(lQueued, gDummyThreadPool->mDummyQueued.size());

    // void *lCfg = createDirConfig(lParms->pool, strdup("/spp/main"));
    // CPPUNIT_ASSERT(!setActive(lParms, lCfg));

    // // Activate path
    // DupModule::pushRequest("/spp/main", "");
    // CPPUNIT_ASSERT_EQUAL(++lQueued, gDummyThreadPool->mDummyQueued.size());

    // // Non-activate path
    // DupModule::pushRequest("/spp/main200", "");
    // CPPUNIT_ASSERT_EQUAL(lQueued, gDummyThreadPool->mDummyQueued.size());

    // // Activate sub-path
    // DupModule::pushRequest("/spp/main/200", "");
    // CPPUNIT_ASSERT_EQUAL(++lQueued, gDummyThreadPool->mDummyQueued.size());

    // // Activate sub-path
    // DupModule::pushRequest("/spp/main/200/", "a=b&c=12");
    // CPPUNIT_ASSERT_EQUAL(++lQueued, gDummyThreadPool->mDummyQueued.size());

    // // Non-activate path
    // DupModule::pushRequest("/spp/", "");
    //    CPPUNIT_ASSERT_EQUAL(lQueued, gDummyThreadPool->mDummyQueued.size());
}

void TestModDup::testConfig()
{
        CPPUNIT_ASSERT(setDestination(NULL, NULL, NULL));
        CPPUNIT_ASSERT(setDestination(NULL, NULL, ""));
        CPPUNIT_ASSERT(!setDestination(NULL, NULL, "localhost"));

        CPPUNIT_ASSERT(setThreads(NULL, NULL, "", "1"));
        CPPUNIT_ASSERT(setThreads(NULL, NULL, "1", ""));
        CPPUNIT_ASSERT(setThreads(NULL, NULL, "2", "1"));
        CPPUNIT_ASSERT(!setThreads(NULL, NULL, "1", "2"));
        CPPUNIT_ASSERT(!setThreads(NULL, NULL, "0", "0"));
        CPPUNIT_ASSERT(setThreads(NULL, NULL, "-1", "2"));


        CPPUNIT_ASSERT(setQueue(NULL, NULL, "", "1"));
        CPPUNIT_ASSERT(setQueue(NULL, NULL, "1", ""));
        CPPUNIT_ASSERT(setQueue(NULL, NULL, "2", "1"));
        CPPUNIT_ASSERT(!setQueue(NULL, NULL, "1", "2"));
        CPPUNIT_ASSERT(!setQueue(NULL, NULL, "0", "0"));
        CPPUNIT_ASSERT(setQueue(NULL, NULL, "-1", "2"));

    cmd_parms * lParms = getParms();
        lParms->path = new char[10];
        strcpy(lParms->path, "/spp/main");
    // Pointer to a boolean meant to activate the module on a given path
        DupConf *lDoHandle = new DupConf();
        memset(lDoHandle, 0, sizeof(*lDoHandle));
        CPPUNIT_ASSERT(!setSubstitution(lParms, (void *)lDoHandle, tFilterBase::eFilterScope::HEADER, "toto", "toto", "titi"));
        CPPUNIT_ASSERT(lDoHandle);
        // Invalid regexp
        CPPUNIT_ASSERT(setSubstitution(lParms, (void *) lDoHandle, tFilterBase::eFilterScope::HEADER, "toto", "*t(oto", "titi"));

        memset(lDoHandle, 0, sizeof(*lDoHandle));

        CPPUNIT_ASSERT(!setFilter(lParms, (void *)lDoHandle, "HEADER", "titi", "toto"));
        CPPUNIT_ASSERT(lDoHandle);
        // Invalid regexp
        CPPUNIT_ASSERT(setFilter(lParms, (void *)lDoHandle, "HEADER", "titi", "*toto"));

        memset(lDoHandle, 0, sizeof(*lDoHandle));

        CPPUNIT_ASSERT(!setActive(lParms, lDoHandle));
        CPPUNIT_ASSERT(lDoHandle);

        delete lParms->path;
}
