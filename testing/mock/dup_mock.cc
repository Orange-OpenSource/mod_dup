/**
 * This module is a mock of a web service
 * Basically it is a web service that will write create an answer exactly as a reguler handler would.
 * We had to test the web service this way
 **/

#include <assert.h>
#include <httpd.h>
#include <http_config.h>
#include <http_core.h>
#include <http_protocol.h>
// Work-around boost::chrono 1.53 conflict on CR typedef vs define in apache
#undef CR

#include <iostream>
#include <list>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <boost/algorithm/string/split.hpp>
#include <map>
#include <vector>
#include <boost/lexical_cast.hpp>

#define SYSLOG_NAMES
#include <sys/syslog.h>

extern module AP_MODULE_DECLARE_DATA dup_mock;

namespace dupMock {

#define SETTINGS_FROM_PARMS(parms) reinterpret_cast<Conf *>(ap_get_module_config(parms->server->module_config, &dup_mock))
#define SETTINGS_FROM_SERVER(server) reinterpret_cast<Conf *>(ap_get_module_config(server->module_config, &dup_mock))

/*
 * Callback to iterate over the headers tables
 * Pushes a copy of key => value in a list
 */
int iterateOverHeadersCallBack(void *d, const char *key, const char *value) {
    std::map< std::string, std::string> *lHeader = reinterpret_cast< std::map< std::string, std::string> *>(d);

    (*lHeader)[std::string(key)] = std::string(value);

    return 1;
}

struct Conf {

    std::string path; /** Path to look the mock files in */
    std::list<std::pair<std::string, std::string> > mocks; /** Match , content to respond */

};

static Conf conf;

static int	wsmock_handler(request_rec *r);
static void	register_hooks(apr_pool_t *pool);


void *createServerConfig(apr_pool_t *pPool, server_rec* ) {
    return new Conf();
}

const char* readBodyData(request_rec* r) {
    int rc = ap_setup_client_block( r, REQUEST_CHUNKED_DECHUNK);

    if( rc != OK ){
        return NULL;
    }

    if( ap_should_client_block(r) ){

        char buf[HUGE_STRING_LEN];
        apr_off_t rpos = 0;
        apr_off_t length = r->remaining;
        char* result = reinterpret_cast<char*>(apr_pcalloc( r->pool, length + 1));
        syslog(LOG_ERR, "DUP MOCK %ld bytes remaining", r->remaining);
        
        if( result == NULL ){
            syslog(LOG_ERR, "Unable to allocate memory");
            return NULL;
        }

        long int bytes_read = 1;
        while( bytes_read > 0 ){
            bytes_read = ap_get_client_block(r, buf, HUGE_STRING_LEN);
            syslog(LOG_ERR, "Remaining %ld after we got a block of %ld",
                                     r->remaining, bytes_read);
            if( bytes_read <= 0 ){
                break;
            }
            if ( length < (bytes_read + rpos + 1) ) {
                ///TODO find a more efficient memory reallocation model
                // Compressed body with more bytes than planned, realloc more
                length = (length + bytes_read) * 2;
                // we end up storing again with twice the size
                char *prevres = result;
                result = reinterpret_cast<char*>(apr_pcalloc( r->pool, length + 1));
                syslog(LOG_ERR, "Realloc result buffer to %ld bytes because result is bigger than expected (gziped body decompression)", length);
                // so copy the previous buffer
                memcpy(result, prevres, rpos);
            }
            memcpy(result + rpos, buf, bytes_read);
            rpos += bytes_read;
        }
        return result;
        
    }
    return NULL;
}

static int wsmock_handler(request_rec *r) {
    std::string answer;

    if (!r->handler || strcmp(r->handler, "dup_mock"))
        return (DECLINED);

    syslog(LOG_ERR, "DUP MOCK HANDLING REQUEST");
    
    struct Conf *conf = SETTINGS_FROM_SERVER(r->server);
    assert(conf);

    readBodyData(r); // read body data (needed to proper testing of compare input filters

    std::string uri = r->unparsed_uri;

    // dup_mock will be used for dynamic inquiring purposes (get the headers, the method, etc.)
    // it will then dump the requested info in the body
    if (uri.find("/inquire?") != std::string::npos) { // /inquire?method,headers,contenttype
        std::string queryString = uri.substr(uri.find("?")+1,std::string::npos); // get the query string (past the ?)
        std::vector<std::string> infos;
        boost::split(infos, queryString, [](char c) { return c==',';});
        for (const std::string& info : infos) {
            if (info == "method") {
                ap_rputs("Method: ",r);
                ap_rputs(r->method,r);
                ap_rputs("\n",r);
                ap_rputs("Method number: ",r);
                ap_rputs(std::to_string(r->method_number).c_str(),r);
                ap_rputs("\n\n",r);
            } else
            if (info == "headers") {
                std::map< std::string, std::string> headersMap;
                apr_table_do(&iterateOverHeadersCallBack, &headersMap, r->headers_in, NULL);
                for (const auto& pair : headersMap) {
                    ap_rputs((pair.first+": ").c_str(),r);
                    ap_rputs((pair.second+"\n").c_str(),r);
                }
                ap_rputs("\n",r);
            } else
            if (info == "contenttype") {
                ap_rputs("Content-Type2: ",r);
                ap_rputs(r->content_type,r);
                ap_rputs("\n",r);
            }
        }
        return OK;
    } else if (uri.find("/sleep") != std::string::npos) { // /sleep?for=1 or just /sleep (1 sec)
        std::string lastUriPart = uri.substr(uri.find("/sleep"),std::string::npos);
        int duration;

        if (lastUriPart.find("?for") == std::string::npos) duration = 1;
        else sscanf(lastUriPart.c_str(),"/sleep?for=%d",&duration);

        sleep(duration);
        ap_rputs("Slept for ", r);
        ap_rputs(boost::lexical_cast<std::string>(duration).c_str(),r);
        ap_rputs(" sec\n",r);
        syslog(LOG_ERR, "END SLEEP DUP MOCK");
        return OK;
    }

    ap_set_content_type(r, "text/html");

    std::list<std::pair<std::string, std::string> > &mocks = conf->mocks;
    std::list<std::pair<std::string, std::string> >::iterator it = mocks.begin(),
        ite = mocks.end();

    while (it != ite) {
        if (std::string::npos != uri.find(it->first)) {
            ap_rputs(it->second.c_str(), r);
            return OK;
        }
        ++it;
    }

    ap_rputs("*** Generic mock answer (no match) ***", r);
    return OK;
}


static void register_hooks(apr_pool_t *pool) {
    // declare the web service handler
    ap_hook_handler(wsmock_handler, NULL, NULL, APR_HOOK_MIDDLE);
}


const char* setDir(cmd_parms* pParams, void* pCfg, const char* pPath) {
    if (!pPath || strlen(pPath) == 0) {
        return "Set the directory name";
    }
    struct Conf *conf = SETTINGS_FROM_PARMS(pParams);

    assert(conf);
    conf->path = pPath;
    return NULL;
}

std::string read_file(const std::string &fileName) {
    std::string content;
    int fd = open(fileName.c_str(), O_RDONLY);
    if (fd == -1 )
        return std::string("Cannot open file: ") + fileName;
    char buf[1024];
    int r;
    while ((r = read(fd, buf, 1024)) > 0) {
        content.append(buf, r);
    }
    close(fd);
    return content;
}

const char* setMockFile(cmd_parms* pParams, void* pCfg, const char* toMatch, const char *fileName) {
    if (!toMatch || !strlen(toMatch) || !fileName || !strlen(fileName) ) {
        return "Usage: 'String to match in URI' 'filename'";
    }
    struct Conf *conf = SETTINGS_FROM_PARMS(pParams);
    assert(conf);

    if (conf->path.empty()) {
        return "Set mock location first";
    }

    // read content
    std::string fileContent = read_file(conf->path + fileName);

    // Push it into list
    std::list<std::pair<std::string, std::string> > &mocks = conf->mocks;
    std::list<std::pair<std::string, std::string> >::iterator it = mocks.begin(),
        ite = mocks.end();

    while (it != ite) {
        std::list<std::pair<std::string, std::string> >::iterator itn = it;
        ++itn;
        if (itn == ite) {
            break;
        }
        const std::string &loc = itn->first;
        if (loc.size() < strlen(toMatch))
            break;
        ++it;
    }

    if (it == ite) {
        mocks.push_back(std::make_pair(toMatch, fileContent));
    } else {
        mocks.insert(it, std::make_pair(toMatch, fileContent));
    }

    return NULL;
}

command_rec gCmds[] = {
    AP_INIT_TAKE1("MockDir",
                  reinterpret_cast<const char *(*)()>(&dupMock::setDir),
                  0,
                  OR_ALL,
                  "The path where to look for content"),
    AP_INIT_TAKE2("MockFile",
                  reinterpret_cast<const char *(*)()>(&dupMock::setMockFile),
                  0,
                  OR_ALL,
                  "Set a file content as answer for url matching the first parameter"),
};

}; // namespace dupMock


module AP_MODULE_DECLARE_DATA dup_mock = {
    STANDARD20_MODULE_STUFF,
    NULL,
    NULL,
    dupMock::createServerConfig,
    NULL,
    dupMock::gCmds,
    dupMock::register_hooks,
};
