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
#include <http_config.h>
#include <http_request.h>

namespace CommonModule {

    extern const unsigned int CMaxBytes;
    extern const char* c_UNIQUE_ID;

    /*
     * Returns the next random request ID
     * method is reentrant
     */
    unsigned int getNextReqId();

    bool extractBrigadeContent(apr_bucket_brigade *bb, ap_filter_t *pF, std::string &content);

    std::string getOrSetUniqueID(request_rec *pRequest);

    /*
     * Method that calls the destructor of an object which type is templated
     */
    template <class T> inline apr_status_t cleaner(void *self) {
        if (self) {
            T *elt = reinterpret_cast<T *>(self);
            assert(elt);
            elt->~T();
        }
        return 0;
    }

    /*
     * Method that allocates on the request pool a RequestInfo object (template is needed to choose from MigrateModule::RequestInfo or DupModule::RequestInfo)
     * The second template parameter is the module address, usually &(compare|dup|migrate)_module
     */
    template<typename T, const module * mod> inline boost::shared_ptr<T> *makeRequestInfo(request_rec *pRequest) {
        // Unique request id
        std::string uid = getOrSetUniqueID(pRequest);
        T* info = new T(uid);

        // Allocation on a shared pointer on the request pool
        // We guarantee that whatever happens, the RequestInfo will be deleted
        void *space = apr_palloc(pRequest->pool, sizeof(boost::shared_ptr<T>));
        boost::shared_ptr<T> *wrappedInfo = new (space) boost::shared_ptr<T>(info);
        // Registering of the shared pointer destructor on the pool
        apr_pool_cleanup_register(pRequest->pool, space, cleaner<boost::shared_ptr<T> >, apr_pool_cleanup_null);

        // Backup in request context
        ap_set_module_config(pRequest->request_config, mod, (void *)space);

        return wrappedInfo;
    }

};
