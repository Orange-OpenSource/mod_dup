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

#include "MultiThreadQueue.hh"
#include "RequestInfo.hh"
#include "Log.hh"
#include <boost/foreach.hpp>

namespace DupModule {
        template <typename T> void MultiThreadQueue<T>::push(const T object)
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
        
        template <typename T> void MultiThreadQueue<T>::push_front(const T object)
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
        
        template <typename T> T MultiThreadQueue<T>::pop()
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
        
        template <typename T> size_t MultiThreadQueue<T>::size() const {
            return mQueue.size();
        }
        
        template <typename T> void MultiThreadQueue<T>::setDropSize(size_t pDropSize) {
            mDropSize = pDropSize;
        }
        
        template <typename T> void MultiThreadQueue<T>::getCounters(unsigned &pInCount, unsigned &pOutCount, unsigned &pDropCount) {
            pInCount = mInCount;
            pOutCount = mOutCount;
            pDropCount = mDropCount;
            mInCount = mOutCount = mDropCount = 0;
        }
   
   template class MultiThreadQueue<boost::shared_ptr<RequestInfo>>;
   template class MultiThreadQueue<int>;
   template class MultiThreadQueue<std::pair<std::string,std::string>>;
   
}
