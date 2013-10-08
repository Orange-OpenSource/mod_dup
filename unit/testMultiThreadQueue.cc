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

#include "MultiThreadQueue.hh"
#include "testMultiThreadQueue.hh"

// cppunit
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

CPPUNIT_TEST_SUITE_REGISTRATION( TestMultiThreadQueue );

#define CPPUNIT_ASSERT_EQUAL_UINT(a, b) CPPUNIT_ASSERT_EQUAL(static_cast<unsigned int>(a), static_cast<unsigned int>(b))

using namespace DupModule;

void TestMultiThreadQueue::run()
{
	unsigned lInCount, lOutCount, lDropCount;
	MultiThreadQueue<int> queue;

	queue.getCounters(lInCount, lOutCount, lDropCount);
	CPPUNIT_ASSERT_EQUAL_UINT(0, lInCount);
	CPPUNIT_ASSERT_EQUAL_UINT(0, lOutCount);
	CPPUNIT_ASSERT_EQUAL_UINT(0, lDropCount);
	queue.push(1);
	
	// We added one element since last getCounters call
	queue.getCounters(lInCount, lOutCount, lDropCount);
	CPPUNIT_ASSERT_EQUAL_UINT(1, lInCount);
	CPPUNIT_ASSERT_EQUAL_UINT(0, lOutCount);
	CPPUNIT_ASSERT_EQUAL_UINT(0, lDropCount);
	queue.push(2);
	queue.push(2);
	queue.push_front(3);

	// We added two elements since last getCounters call (push_front don't count)
	queue.getCounters(lInCount, lOutCount, lDropCount);
	CPPUNIT_ASSERT_EQUAL_UINT(2, lInCount);
	CPPUNIT_ASSERT_EQUAL_UINT(0, lOutCount);
	CPPUNIT_ASSERT_EQUAL_UINT(0, lDropCount);
	CPPUNIT_ASSERT_EQUAL(3, queue.pop());

	// We popped one element since last getCounters call
	queue.getCounters(lInCount, lOutCount, lDropCount);
	CPPUNIT_ASSERT_EQUAL_UINT(0, lInCount);
	CPPUNIT_ASSERT_EQUAL_UINT(1, lOutCount);
	CPPUNIT_ASSERT_EQUAL_UINT(0, lDropCount);
	CPPUNIT_ASSERT_EQUAL(1, queue.pop());
	CPPUNIT_ASSERT_EQUAL(2, queue.pop());
	CPPUNIT_ASSERT_EQUAL(2, queue.pop());

	// Nothing left in the queue
	CPPUNIT_ASSERT_EQUAL_UINT(0, queue.size());

	queue.setDropSize(1);
	queue.push(44444);
	queue.push(34);
	// Only one intem got inserted. Another one was dropped.
	// ... and we've got the three pops before the setDropSize
	queue.getCounters(lInCount, lOutCount, lDropCount);
	CPPUNIT_ASSERT_EQUAL_UINT(1, lInCount);
	CPPUNIT_ASSERT_EQUAL_UINT(3, lOutCount);
	CPPUNIT_ASSERT_EQUAL_UINT(1, lDropCount);
	CPPUNIT_ASSERT_EQUAL_UINT(1, queue.size());
	CPPUNIT_ASSERT_EQUAL(44444, queue.pop());

	queue.push(123);
	queue.push_front(1234);
	// 1234 forces 123 to be dropped
	queue.getCounters(lInCount, lOutCount, lDropCount);
	CPPUNIT_ASSERT_EQUAL_UINT(1, lInCount);
	CPPUNIT_ASSERT_EQUAL_UINT(1, lOutCount);
	CPPUNIT_ASSERT_EQUAL_UINT(1, lDropCount);
	CPPUNIT_ASSERT_EQUAL_UINT(1, queue.size());
	CPPUNIT_ASSERT_EQUAL(1234, queue.pop());


	// This works also with more complex types
	typedef std::pair<std::string, std::string> complexType;
	MultiThreadQueue<complexType > complexQueue;
	complexQueue.push(complexType(std::string("titi"), std::string("toto")));
	complexType popped = complexQueue.pop();
	CPPUNIT_ASSERT_EQUAL(popped.first, std::string("titi"));
	complexQueue.getCounters(lInCount, lOutCount, lDropCount);
	CPPUNIT_ASSERT_EQUAL_UINT(1, lInCount);
	CPPUNIT_ASSERT_EQUAL_UINT(1, lOutCount);
	CPPUNIT_ASSERT_EQUAL_UINT(0, lDropCount);
	CPPUNIT_ASSERT_EQUAL_UINT(0, queue.size());
}
