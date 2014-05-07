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

#ifndef __subReqMgr__hh__
#define __subReqMgr__hh__

#include <httpd.h>
#include <http_request.h>
#include <apr_tables.h>
#include <util_filter.h>


class subReqMgr {
public:
    inline subReqMgr(const void *);
    inline request_rec *subreq(const char *, const char *, const request_rec *);
    inline int run_subreq(request_rec *);
    inline void set_out_flt(const char *, request_rec *);
    inline void set_in_flt(const char *, request_rec *);
    inline void internal_redirect(const char *, request_rec *);
    inline void destroy_subreq(request_rec *);
};

inline
subReqMgr::subReqMgr(const void *)
{
}

inline request_rec *
subReqMgr::subreq(const char *method, const char *url, const request_rec *main_req) {
    return ap_sub_req_method_uri(method, url, main_req, 0);
}

inline int
subReqMgr::run_subreq(request_rec *r) {
    return ap_run_sub_req(r);
}

inline void
subReqMgr::set_out_flt(const char *flt_name, request_rec *r) {
    ap_add_output_filter(flt_name, NULL, r, r->connection);
}

inline void
subReqMgr::set_in_flt(const char *flt_name, request_rec *r) {
    ap_add_input_filter(flt_name, NULL, r, r->connection);
}

inline void
subReqMgr::internal_redirect(const char *uri, request_rec *r) {
    ap_internal_redirect(uri, r);
}

inline void
subReqMgr::destroy_subreq(request_rec *r) {
    ap_destroy_sub_req(r);
}

#endif
