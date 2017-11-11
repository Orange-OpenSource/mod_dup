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

#pragma once

#include <boost/function.hpp>
#include "MultiThreadQueue.hh"


namespace DupModule {

/**
 * @brief Manages a pool of threads depending on the size of its queue.
 * As its queue grows, it spawns new worker threads. If the queue shrinks again, it hands poison pills to a worker which should then exit.
 * The class gets the queue item type as its template argument. This makes it independent of any business needs and therefore more easily reusable.
 */
template <typename QueueT>
class ThreadPool {
public:
    /** @brief The type of the function object running in each thread which pulls the items off the queue */
    typedef boost::function1<void, MultiThreadQueue<QueueT> &> tQueueWorker;

    /** @brief The type of the function object which returns a stat */
    typedef boost::function0<const std::string> tStatProvider;

public:
    /**
     * @brief Constructs a ThreadPool object
     * @param pWorker a function object which will be executed by each worker thread
     * @param pPoisonItem an item which, if received, should cause a worker to exit
     */
    ThreadPool(tQueueWorker pWorker, const QueueT &pPoisonItem);

    /// @brief Destructs the ThreadPool object
    virtual ~ThreadPool();
    
    /**
     * @brief Set the interval between two stats log messages
     * @param pStatsInterval the interval in microseconds between two stats log messages
     */
    void setStatsInterval(const unsigned pStatsInterval);

    /**
     * @brief Add a stat provider
     * @param pStatName the name to be displayed along with the stat
     * @param pStatProvider the function returning the stat
     */
    void addStat(const std::string &pStatName, tStatProvider pStatProvider);

    /**
     * @brief Set the program name to be used in the stats log message
     * @param pProgramName the name of the program
     */
    void setProgramName(const std::string &pProgramName);

    /**
     * @brief Set the minimum and maximum number of threads
     * @param pMinThreads the minimum number of threads
     * @param pMaxThreads the maximum number of threads
     */
    void setThreads(const size_t pMinThreads, const size_t pMaxThreads);

    /**
     * @brief Set the minimum and maximum queue size
     * @param pMinQueued the minimum queue size
     * @param pMaxQueued the maximum queue size
     */
    void setQueue(const size_t pMinQueued, const size_t pMaxQueued);

    /// @brief Start the manager thread and the minimum number of worker threads
    void start();

    /// @brief Stop the manager thread and all the workers
    void stop();

    /**
     * @brief Queue an item
     * @param pItem the item to be queued
     */
    virtual void push(const QueueT &pItem);
    
    /**
     * @brief Get the number of threads currently running.
     * @return The number of threads currently running.
     */
    size_t getThreadCount();
       
private:
    /// @brief Spawn a new worker thread
    void newThread();
    
    /// @brief Add a poison pill to the front of queue, to kill the next thread reading from it
    void poisonThread();
    
    /// @brief Collect any threads which exited (probably because we poisoned them) and remove them from our list
    void collectKilled();
    
    /**
     * @brief Run an infinite loop which manages the threads in the pool,
     * depending on the amount of queued items
     */
    void run(); 
    
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
    
};

}
