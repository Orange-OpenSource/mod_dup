/*
 * RequestCommon.cc
 *
 *  Created on: Jul 30, 2014
 *      Author: ccaraccio
 */

#include "RequestCommon.hh"

namespace ApplicationScope {

const char* c_ALL = "ALL";
const char* c_PATH = "PATH";
const char* c_QUERY_STRING = "QUERY_STRING";
const char* c_URL = "URL";
const char* c_URL_AND_HEADERS = "URL_AND_HEADERS";
const char* c_HEADERS = "HEADERS";
const char* c_BODY = "BODY";
const char* c_ERROR_ON_STRING_VALUE = "Invalid ApplicationScope Value. Supported Values: PATH | QUERY_STRING | URL | HEADERS | URL_AND_HEADERS | BODY | ALL" ;

eApplicationScope stringToEnum(const char *str) throw (std::exception) {
    if (!strcmp(str, c_ALL))
        return ApplicationScope::ALL;
    if (!strcmp(str, c_PATH))
        return ApplicationScope::PATH;
    if (!strcmp(str, c_QUERY_STRING))
        return ApplicationScope::QUERY_STRING;
    if (!strcmp(str, c_URL_AND_HEADERS))
        return ApplicationScope::URL_AND_HEADERS;
    if (!strcmp(str, c_URL))
        return ApplicationScope::URL;
    if (!strcmp(str, c_HEADERS))
            return ApplicationScope::HEADERS;
    if (!strcmp(str, c_BODY))
        return ApplicationScope::BODY;
    throw std::exception();
}

const char * enumToString(eApplicationScope scope) throw (std::exception) {
    switch (scope) {
        case ApplicationScope::ALL:
            return c_ALL;
        case ApplicationScope::PATH:
            return c_PATH;
        case ApplicationScope::QUERY_STRING:
            return c_QUERY_STRING;
        case ApplicationScope::URL:
            return c_URL;
        case ApplicationScope::URL_AND_HEADERS:
            return c_URL_AND_HEADERS;
        case ApplicationScope::HEADERS:
            return c_HEADERS;
        case ApplicationScope::BODY:
            return c_BODY;
        default:
            throw std::exception();
    }
}

}

