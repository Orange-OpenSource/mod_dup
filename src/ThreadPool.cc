/*
 * mod_dup - duplicates apache requests
 *
 * Copyright (C) 2017 Orange
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

#include "ThreadPool.hh"
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include "RequestInfo.hh"

using namespace boost::posix_time;

namespace DupModule {
template <typename QueueT> ThreadPool<QueueT>::ThreadPool(tQueueWorker pWorker, const QueueT &pPoisonItem) :
    mManagerThread(NULL),
    mMinThreads(1), mMaxThreads(10),
    mMinQueued(1), mMaxQueued(10),
    mBeingKilled(0),
    mStatsInterval(10000000),
    mWorker(pWorker),
    mPoisonItem(pPoisonItem),
    mRunning(false),
    mProgramName("ModDup")
{
}

template <typename QueueT> ThreadPool<QueueT>::~ThreadPool()
{
    stop();
}

template <typename QueueT> void ThreadPool<QueueT>::newThread()
{
    mThreads.push_back(new boost::thread(mWorker, boost::ref(this->mQueue)));
}

template <typename QueueT> void ThreadPool<QueueT>::poisonThread()
{
    Log::debug("Dropping a poison pill.");
    mQueue.push_front(mPoisonItem);
    mBeingKilled++;
}

template <typename QueueT> void ThreadPool<QueueT>::collectKilled()
{
    time_duration shortWait(0, 0, 0, 10000); // wait for 0h,0min,0s,10ms

    std::list<boost::thread *>::iterator it = mThreads.begin();
    while (it != mThreads.end()) {
        if ((*it)->timed_join(shortWait)) {
            Log::debug("Removing a terminated thread from pool.");
            delete *it;
            it = mThreads.erase(it);
            mBeingKilled--;
        }
        else {
            ++it;
        }
    }
}

template <typename QueueT> void ThreadPool<QueueT>::run()
{
    unsigned pid = getpid();
    unsigned iterationsSinceStats = 0;

    while (mRunning) {
        size_t lQueued = mQueue.size();
        float lQueuedPerThread = static_cast<float>(mQueue.size()) / mThreads.size();

        collectKilled();

        if ((lQueuedPerThread > mMaxQueued) && (mThreads.size() < mMaxThreads)) {
            newThread();
        }
        else if ((lQueuedPerThread < mMinQueued) && ((mThreads.size() - mBeingKilled) > mMinThreads)) {
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
    for (unsigned i = 0; i < mThreads.size(); ++i) {
        poisonThread();
    }
    collectKilled();
}

template <typename QueueT> void ThreadPool<QueueT>::setStatsInterval(const unsigned pStatsInterval)
{
    mStatsInterval = pStatsInterval;
}

template <typename QueueT> void ThreadPool<QueueT>::addStat(const std::string &pStatName, tStatProvider pStatProvider)
{
    mAdditionalStats[pStatName] = pStatProvider;
}

template <typename QueueT> void ThreadPool<QueueT>::setProgramName(const std::string &pProgramName)
{
    mProgramName = pProgramName;
}

template <typename QueueT> void ThreadPool<QueueT>::setThreads(const size_t pMinThreads, const size_t pMaxThreads)
{
    mMinThreads = pMinThreads;
    mMaxThreads = pMaxThreads;
}

template <typename QueueT> void ThreadPool<QueueT>::setQueue(const size_t pMinQueued, const size_t pMaxQueued)
{
    mMinQueued = pMinQueued;
    mMaxQueued = pMaxQueued;
}

template <typename QueueT> void ThreadPool<QueueT>::start()
{
    mRunning = true;
    mQueue.setDropSize(mMaxQueued * mMaxThreads);

    Log::debug("Started thread pool %p", this);
    for (unsigned i = 0; i < mMinThreads; ++i) {
        newThread();
    }

    mManagerThread = new boost::thread(boost::bind(&ThreadPool::run, this));
}

template <typename QueueT> void ThreadPool<QueueT>::stop()
{
    mRunning = false;
    if (mManagerThread) {
        // TODO improve this part.
        // The process can be stuck here if curl calls do not terminate
        mManagerThread->join();
        delete mManagerThread;
        mManagerThread = NULL;
    }
}

template <typename QueueT> void ThreadPool<QueueT>::push(const QueueT &pItem)
{
    mQueue.push(pItem);
}

template <typename QueueT> size_t ThreadPool<QueueT>::getThreadCount()
{
    return mThreads.size();
}

// Explicitly instantiate the ones we use
template class ThreadPool<boost::shared_ptr<RequestInfo>>;
template class ThreadPool<int>;

}
