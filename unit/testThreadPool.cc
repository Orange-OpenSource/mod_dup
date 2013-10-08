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

#include "ThreadPool.hh"
#include "MultiThreadQueue.hh"
#include "testThreadPool.hh"

// cppunit
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

CPPUNIT_TEST_SUITE_REGISTRATION( TestThreadPool );

#define CPPUNIT_ASSERT_EQUAL_UINT(a, b) CPPUNIT_ASSERT_EQUAL(static_cast<unsigned int>(a), static_cast<unsigned int>(b))

using namespace DupModule;

boost::mutex mutex;
int count = 0;

const int POISON = 42;

void worker(MultiThreadQueue<int> &queue)
{
	for (;;) {
		int item = queue.pop();
		if (item == POISON)
			break;
		usleep(item);
		{
			boost::mutex::scoped_lock lock( mutex );
			count++;
		}
	}
}

void TestThreadPool::run()
{
    apr_initialize();

    Log::init(NULL);

	MultiThreadQueue<int> queue;
	ThreadPool<int> pool(&worker, POISON);
	// Display stats every second, so that this test triggers it
	pool.setStatsInterval(1000000);

	// 2 seconds worth of work
	for (int i=0; i<1000; ++i)
		pool.push(2000);

	pool.setQueue(1, 2);
	pool.setThreads(1, 4);

	CPPUNIT_ASSERT_EQUAL(0, count);
	pool.start();

	// theoretical processing time of 2 seconds of work with 1 to 4 threads: 650 ms
	// theoretical thread wind down time: 400 ms
	// error margin: 200ms
	usleep(650000 + 400000 + 200000);

	// So now we should have processed all items and wound down to a single thread again
	CPPUNIT_ASSERT_EQUAL(1000, count);
	CPPUNIT_ASSERT_EQUAL_UINT(1, pool.getThreadCount());

	pool.stop();
}
