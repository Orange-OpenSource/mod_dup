/*
 * RequestCommon.cc
 *
 *  Created on: Jul 30, 2014
 *      Author: ccaraccio
 */

#include "RequestCommon.hh"

namespace ApplicationScope {

const char* c_ALL = "ALL";
const char* c_URL = "URL";
const char* c_HEADER = "HEADER";
const char* c_BODY = "BODY";
const char* c_ERROR_ON_STRING_VALUE = "Invalid ApplicationScope Value. Supported Values: ALL | URL | HEADER | BODY" ;

eApplicationScope stringToEnum(const char *str) throw (std::exception) {
    if (!strcmp(str, c_ALL))
        return ApplicationScope::ALL;
    if (!strcmp(str, c_URL))
        return ApplicationScope::URL;
    if (!strcmp(str, c_HEADER))
            return ApplicationScope::HEADER;
    if (!strcmp(str, c_BODY))
        return ApplicationScope::BODY;
    throw std::exception();
}

const char * enumToString(eApplicationScope scope) throw (std::exception) {
    switch (scope) {
        case ApplicationScope::ALL:
            return c_ALL;
        case ApplicationScope::URL:
            return c_URL;
        case ApplicationScope::HEADER:
            return c_HEADER;
        case ApplicationScope::BODY:
            return c_BODY;
        default:
            throw std::exception();
    }
}

}

