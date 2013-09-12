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
