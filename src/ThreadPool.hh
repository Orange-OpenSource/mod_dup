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

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/function.hpp>

#include "MultiThreadQueue.hh"

using namespace boost::posix_time;

namespace DupModule {

/**
 * @brief Manages a pool of threads depending on the size of its queue.
 * As its queue grows, it spawns new worker threads. If the queue shrinks again, it hands poison pills to a worker which should then exit.
 * The class gets the queue item type as its template argument. This makes it independent of any business needs and therefore more easily reusable.
 */
template <typename QueueT>
class ThreadPool
{
public:
	/** @brief The type of the function object running in each thread which pulls the items off the queue */
	typedef boost::function1<void, MultiThreadQueue<QueueT> &> tQueueWorker;

	/** @brief The type of the function object which returns a stat */
	typedef boost::function0<const std::string> tStatProvider;

private:
	/** @brief The time in micro sec for which we wait before controlling the number of threads in the pool */
	static const unsigned mManageInterval = 100000;

	/** @brief Contains the threads */
	std::list<boost::thread *> mThreads;
	/** @brief The thread managing all others */
	boost::thread *mManagerThread;
	/** @brief The minimum number of threads in the pool */
	size_t mMinThreads;
	/** @brief The maximum number of threads in the pool */
	size_t mMaxThreads;
	/** @brief The minimum number of queued items per thread */
	size_t mMinQueued;
	/** @brief The maximum number of queued items per thread */
	size_t mMaxQueued;
	// @ brief The number of threads a poison pill got sent to but have not exited yet
	unsigned mBeingKilled;
	// The time in micro sec for which we wait before emitting statistics in the log output
	unsigned mStatsInterval;
	/** @brief A function object which pulls items off the queue */
	tQueueWorker mWorker;
	/** @brief The queue of items to be handled by the threads */
	MultiThreadQueue<QueueT> mQueue;
	/** @brief The poison item which should be send to force a thread to exit */
	const QueueT mPoisonItem;
	/** @brief true if the pool should continue to run */
	volatile bool mRunning;
    /** @brief Program name */
    std::string mProgramName;
	/** @brief Map containing additional stats providers */
	std::map<std::string, tStatProvider> mAdditionalStats;

	/**
	 * @brief Spawn a new worker thread
	 */
	void
	newThread() {
		mThreads.push_back(new boost::thread(mWorker, boost::ref(this->mQueue)));
	}

	/**
	 * @brief Add a poison pill to the front of queue, to kill the next thread reading from it
	 */
	void poisonThread() {
		Log::debug("Dropping a poison pill.");
		mQueue.push_front(mPoisonItem);
		mBeingKilled++;
	}

	/**
	 * @brief Collect any threads which exited (probably because we poisoned them) and remove them from our list
	 */
	void collectKilled() {
		time_duration shortWait(0, 0, 0, 10000); // wait for 0h,0min,0s,10ms

		std::list<boost::thread *>::iterator it = mThreads.begin();
		while (it != mThreads.end()) {
            if ((*it)->timed_join(shortWait)) {
				Log::debug("Removing a terminated thread from pool.");
				delete *it;
				it = mThreads.erase(it);
				mBeingKilled--;
			} else {
                ++it;
			}
		}
	}

	/**
	 * @brief Run an infinite loop which manages the threads in the pool,
	 * depending on the amount of queued items
	 */
	void
	run() {
		unsigned pid = getpid();
		unsigned iterationsSinceStats = 0;

		while (mRunning) {
			size_t lQueued = mQueue.size();
			float lQueuedPerThread = static_cast<float>(mQueue.size()) / mThreads.size();
            
			collectKilled();

			if ((lQueuedPerThread > mMaxQueued) && (mThreads.size() < mMaxThreads)) {
				newThread();
			} else if ((lQueuedPerThread < mMinQueued) && ((mThreads.size() - mBeingKilled) > mMinThreads)) {
				poisonThread();
			}

			if (++iterationsSinceStats * mManageInterval >= mStatsInterval) {
				unsigned lInCount, lOutCount, lDropCount;
				mQueue.getCounters(lInCount, lOutCount, lDropCount);

				// FIXME: Hardcoding retrieval of only additional stats for now. This should become more generic.
				std::map<std::string, tStatProvider>::const_iterator lStatsIter = mAdditionalStats.find("#TmOut");
				const std::string lTimeoutCount = lStatsIter == mAdditionalStats.end() ? "??" : lStatsIter->second();
				lStatsIter = mAdditionalStats.find("#DupReq");
                const std::string lDuplicateCount = lStatsIter == mAdditionalStats.end() ? "??" : lStatsIter->second();

				Log::notice(201, "%s - %u - %zu - %zu - %u - %u - %u - %s - %s",
				        mProgramName.c_str(), pid, lQueued, mThreads.size(), lInCount, lOutCount,
                        lDropCount, lTimeoutCount.c_str(), lDuplicateCount.c_str());
				if (lDropCount > 0) {
					Log::warn(301, "Pool %u dropped %d requests during last cycle!", pid, lDropCount);
				}
				iterationsSinceStats = 0;
			}
			usleep(mManageInterval);
		}

		// Poison all threads, not an issue if some were already exiting
        for (unsigned i=0; i<mThreads.size(); ++i) {
			poisonThread();
		}
        collectKilled();
	}

public:
	/**
	 * @brief Constructs a ThreadPool object
	 * @param pWorker a function object which will be executed by each worker thread
	 * @param pPoisonItem an item which, if received, should cause a worker to exit
	 */
	ThreadPool(tQueueWorker pWorker, const QueueT &pPoisonItem) :
									   mManagerThread(NULL),
									   mMinThreads(1), mMaxThreads(10),
									   mMinQueued(1), mMaxQueued(10),
									   mBeingKilled(0),
									   mStatsInterval(10000000),
									   mWorker(pWorker),
									   mPoisonItem(pPoisonItem),
									   mRunning(false),
                                       mProgramName("ModDup") {
	}

	/**
	 * @brief Destructs the ThreadPool object
	 */
	virtual ~ThreadPool() {
		stop();
	}

	/**
	 * @brief Set the interval between two stats log messages
	 * @param pStatsInterval the interval in microseconds between two stats log messages
	 */
	void
	setStatsInterval(const unsigned pStatsInterval) {
		mStatsInterval = pStatsInterval;
	}

	/**
	 * @brief Add a stat provider
	 * @param pStatName the name to be displayed along with the stat
	 * @param pStatProvider the function returning the stat
	 */
	void
	addStat(const std::string &pStatName, tStatProvider pStatProvider) {
		mAdditionalStats[pStatName] = pStatProvider;
	}

	/**
	 * @brief Set the program name to be used in the stats log message
	 * @param pProgramName the name of the program
	 */
	void
	setProgramName(const std::string &pProgramName) {
		mProgramName = pProgramName;
	}

	/**
	 * @brief Set the minimum and maximum number of threads
	 * @param pMinThreads the minimum number of threads
	 * @param pMaxThreads the maximum number of threads
	 */
	void
	setThreads(const size_t pMinThreads, const size_t pMaxThreads) {
		mMinThreads = pMinThreads;
		mMaxThreads = pMaxThreads;
	}

	/**
	 * @brief Set the minimum and maximum queue size
	 * @param pMinQueued the minimum queue size
	 * @param pMaxQueued the maximum queue size
	 */
	void
	setQueue(const size_t pMinQueued, const size_t pMaxQueued) {
		mMinQueued = pMinQueued;
		mMaxQueued = pMaxQueued;
	}

	/**
	 * @brief Start the manager thread and the minimum number of worker threads
	 */
	void
	start() {
		mRunning = true;
		mQueue.setDropSize(mMaxQueued * mMaxThreads);

		Log::debug("Started thread pool %p", this);
		for (unsigned i=0; i<mMinThreads; ++i) {
			newThread();
		}

		mManagerThread = new boost::thread(boost::bind(&ThreadPool::run, this));
	}

	/**
	 * @brief Stop the manager thread and all the workers
	 */
	void
	stop() {
            mRunning = false;
            if (mManagerThread) {
                // TODO improve this part.
                // The process can be stuck here if curl calls do not terminate
                mManagerThread->join();
                delete mManagerThread;
                mManagerThread = NULL;
            }
	}

	/**
	 * @brief Queue an item
	 * @param pItem the item to be queued
	 */
	virtual void
	push(const QueueT &pItem) {
		mQueue.push(pItem);
	}

	/**
	 * @brief Get the number of threads currently running.
	 * @return The number of threads currently running.
	 */
	size_t getThreadCount() {
		return mThreads.size();
	}
};

}
