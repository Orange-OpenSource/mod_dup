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

// Stubs for Apache API
// don't care about memory leaks
// these are just stubs

#include <string>
#include <sstream>

#include <httpd.h>
#include <http_config.h>  // for the config functions
#include <http_request.h>
#include <http_protocol.h>
#include <apr_pools.h>
#include <apr_hooks.h>
#include <unixd.h>

#include <http_log.h>
#include "testBodies.hh"

#include <cstring>
#include <cassert>

unixd_config_rec unixd_config;

AP_DECLARE_NONSTD(int) ap_rprintf( request_rec *r, const char *fmt, ...)
{
    assert( r != NULL );
    assert( fmt != NULL );

    char* lDest = NULL;

    va_list l_ap;
    va_start(l_ap, fmt);

    int n = vasprintf( &lDest, fmt, l_ap);

    va_end(l_ap);

    if( r->filename == NULL ){
        r->filename = lDest;
    }
    else {
        std::string lNew(r->filename);
        lNew.append(lDest);
        free(r->filename);
        r->filename = strdup(lNew.c_str());
        free(lDest);
    }

     return n;
};

AP_DECLARE(int) ap_is_initial_req(request_rec *r)
{
    return 0;
};

const std::string getLevel(int level) {
    switch ( level ) {
    case APLOG_NOERRNO|APLOG_WARNING:
        return "warning";
    case APLOG_NOERRNO|APLOG_ERR:
        return "err";
    case APLOG_NOERRNO|APLOG_NOTICE:
        return "notice";
    case APLOG_NOERRNO|APLOG_INFO:
        return "info";
    case APLOG_NOERRNO|APLOG_DEBUG:
        return "debug";
    case APLOG_NOERRNO|APLOG_EMERG:
        return "crit";
    }
    return "unknown";
}

AP_DECLARE_NONSTD(void) ap_log_error(const char *file, int line, int level,
                             apr_status_t status, const server_rec *s,
                             const char *fmt, ...)
{

    char* lDest = NULL;
    va_list l_ap;
    va_start(l_ap, fmt);

    vasprintf( &lDest, fmt, l_ap);
    printf("%s: %s\n", getLevel(level).c_str(), lDest);

    va_end(l_ap);
}

AP_DECLARE(void) ap_add_version_component(apr_pool_t *pconf, const char *component)
        {
    // NOP
        }

AP_CORE_DECLARE(ap_conf_vector_t *) ap_create_request_config(apr_pool_t *p)
{
    void *conf_vector = apr_pcalloc(p, sizeof(void *) * 16);
     return reinterpret_cast<ap_conf_vector_t *>(conf_vector);

}

AP_DECLARE(apr_status_t) ap_get_brigade(ap_filter_t *filter,
                                        apr_bucket_brigade *bb,
                                        ap_input_mode_t mode,
                                        apr_read_type_e block,
                                        apr_off_t readbytes)
{

    apr_pool_t *pool = NULL;
    apr_pool_create(&pool, 0);
    apr_bucket_alloc_t *bA = apr_bucket_alloc_create(pool);


    // No filter, simply forge an EOS
    if ((void *)filter == (void *)0x42) {
        // We request a real test brigade
        assert(apr_brigade_write(bb, NULL, NULL, testBody42, std::string(testBody42).size()) == APR_SUCCESS);
    }
    apr_bucket *e = apr_bucket_eos_create(bA);
    assert(e);
    APR_BRIGADE_INSERT_TAIL(bb, e);
    return APR_SUCCESS;
}

AP_DECLARE(ap_filter_t *) ap_add_input_filter(const char *name, void *ctx,
                                              request_rec *r, conn_rec *c)
{
	return NULL;
}

apr_status_t
ap_pass_brigade(ap_filter_t *, apr_bucket_brigade *)
{
    return OK;
}
