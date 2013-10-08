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

#include <deque>
#include <boost/foreach.hpp>
#include <boost/thread.hpp>

#include "Log.hh"

namespace DupModule {

/**
 * @brief A thread safe (using boost::mutex and boost::condition_variable) wrapper around a std::deque.
 * It exposes the typical FIFO methods pop and push as well as push_front which makes it possible to add a prioritized item to the front of the queue.
 * It also keeps track of 3 counters for the number of pushed, popped and dropped items. getCounters will return those values and reset them.
 * The class gets the queue item type as its template argument. This makes it independent of any business needs and therefore more easily reusable.
 */
template <typename T>
class MultiThreadQueue
{
private:
	/** @brief The underlying queue holding the itms */
	std::deque<T> mQueue;
	/** @brief The mutex used to ensure thread safety */
	boost::mutex mMutex;
	/** @brief Used to make pull-clients wait and wake them up when necessary */
	boost::condition_variable mAvailableCondition;
	/** @brief Number of added items since last call to getCounters */
	unsigned mInCount;
	/** @brief Number of removed items since last call to getCounters */
	unsigned mOutCount;
	/** @brief Number of dropped items since last call to getCounters */
	unsigned mDropCount;
	/** @brief Maximum number of items to be queued after which any new ones should get dropped */
	size_t mDropSize;

public:
	/**
	 * @brief Constructs a MultiThreadQueue
	 */
	MultiThreadQueue() : mInCount(0), mOutCount(0), mDropCount(0), mDropSize(0) {}

	/**
	 * @brief Adds the given object to the back of the queue so it will be the last one to be pulled
	 * @param object The object to be inserted
	 */
	void push(const T object)
	{
		{
			boost::lock_guard<boost::mutex> lLock(mMutex);
			if (mDropSize > 0 && mQueue.size() >= mDropSize) {
				mDropCount++;
			} else {
				mQueue.push_back(object);
				mInCount++;
			}
		}
		mAvailableCondition.notify_one();
	}

	/**
	 * @brief Adds the given object to the front of the queue so it will be the next one to be pulled
	 * @param object The object to be inserted
	 */
	void push_front(const T object)
	{
		{
			boost::lock_guard<boost::mutex> lLock(mMutex);
			if (mDropSize > 0 && mQueue.size() >= mDropSize) {
				mDropCount++;
				mQueue.pop_back();
			}
			mQueue.push_front(object);
		}
		mAvailableCondition.notify_one();
	}

	/**
	 * @brief Remove and return the first object in the queue. Blocks until something is available.
	 * @return the object
	 */
	const T pop()
	{
		boost::unique_lock<boost::mutex> lLock(mMutex);
		while (mQueue.empty()) {
			mAvailableCondition.wait(lLock);
		}
		T lObject = mQueue.front();
		mQueue.pop_front();
		mOutCount++;
		return lObject;
	}

	/**
	 * @brief Returns the size of the queue
	 * @return the size of the queue
	 */
	size_t size() {
		return mQueue.size();
	}

	/**
	 * @brief Sets the maximum size of the queue. Beyond this size, pushed elements will not be inserted anymnore
	 * @param pDropSize the maximum size of the queue. A value <= 0 means there's no maximum size.
	 */
	void setDropSize(size_t pDropSize) {
		mDropSize = pDropSize;
	}

	/**
	 * @brief Gets various counters. Then resets all counters.
	 * @param pInCount the number of elements pushed since last call
	 * @param pOutCount the number of elements popped since last call
	 * @param pDropCount the number of elements dropped since last call
	 */
	void getCounters(unsigned &pInCount, unsigned &pOutCount, unsigned &pDropCount) {
		pInCount = mInCount;
		pOutCount = mOutCount;
		pDropCount = mDropCount;
		mInCount = mOutCount = mDropCount = 0;
	}
};

}
