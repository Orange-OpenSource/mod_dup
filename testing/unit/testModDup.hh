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

#pragma once

#include <httpd.h>
#include <http_config.h>  // for the config functions
#include <http_request.h>
#include <http_protocol.h>
// Work-around boost::chrono 1.53 conflict on CR typedef vs define in apache
#undef CR

#include <apr_pools.h>
#include <apr_hooks.h>

#include <cppunit/extensions/HelperMacros.h>

#include "MultiThreadQueue.hh"
#include "mod_dup.hh"

#ifdef CPPUNIT_HAVE_NAMESPACES
using namespace CPPUNIT_NS;
#endif

class TestModDup :
    public TestFixture
{

    CPPUNIT_TEST_SUITE(TestModDup);
    CPPUNIT_TEST(testInit);
    CPPUNIT_TEST(testConfig);
    CPPUNIT_TEST(testScope);
    CPPUNIT_TEST(testDuplicationType);
    CPPUNIT_TEST(testHighestDuplicationType);
    CPPUNIT_TEST(testInitAndCleanUp);
    CPPUNIT_TEST(testDuplicationPercentage);
    CPPUNIT_TEST_SUITE_END();

public:
    cmd_parms* getParms();
    void testInit();
    void testInitAndCleanUp();
    void testConfig();
    void testScope();
    void testDuplicationType();
    void testHighestDuplicationType();

    void testDuplicationPercentage();
};


template <typename QueueT>
class DummyThreadPool : public DupModule::ThreadPool<QueueT> {
public:
    typedef boost::function1<void, DupModule::MultiThreadQueue<QueueT> &> tQueueWorker;

    std::list<QueueT> mDummyQueued;

    DummyThreadPool(tQueueWorker pWorker, const QueueT &pPoisonItem) : DupModule::ThreadPool<QueueT>(pWorker, pPoisonItem) {
    }

    void
    push(const QueueT &pItem) {
        mDummyQueued.push_back(pItem);
    }
};
