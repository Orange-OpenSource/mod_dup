/*
 * RequestCommon.cc
 *
 *  Created on: Jul 30, 2014
 *      Author: ccaraccio
 */

#include "RequestCommon.hh"

namespace MigrateModule {

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

}

}

namespace DupModule {

namespace ApplicationScope {

const char* c_ALL = "ALL";
const char* c_BODY = "BODY";
const char* c_HEADER = "HEADER";
const char* c_ERROR_ON_STRING_VALUE = "Invalid ApplicationScope Value. Supported Values: ALL | HEADER | BODY" ;

eApplicationScope stringToEnum(const char *str) throw (std::exception) {
    if (!strcmp(str, c_ALL))
        return ApplicationScope::ALL;
    if (!strcmp(str, c_HEADER))
        return ApplicationScope::HEADER;
    if (!strcmp(str, c_BODY))
        return ApplicationScope::BODY;
    throw std::exception();
}
}
}
