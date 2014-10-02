/**
 * This module is a mock of a web service
 * Basically it is a web service that will write create an answer exactly as a reguler handler would.
 * We had to test the web service this way
 **/


#include <httpd.h>
#include <http_config.h>
#include <http_core.h>
#include <http_protocol.h>

#include <iostream>

static int	wsmock_handler(request_rec *r);
static void	register_hooks(apr_pool_t *pool);


static int wsmock_handler(request_rec *r)
{
    std::string answer;

    if (!r->handler || strcmp(r->handler, "dup_mock"))
        return (DECLINED);

    ap_set_content_type(r, "text/html");

    answer = "Dynamic answer machine";

    ap_rputs(answer.c_str(), r);
    return OK;
}


static void register_hooks(apr_pool_t *pool)
{
    // declare the web service handler
    ap_hook_handler(wsmock_handler, NULL, NULL, APR_HOOK_MIDDLE);
}

module AP_MODULE_DECLARE_DATA dup_mock =
{
    STANDARD20_MODULE_STUFF,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    register_hooks
};
