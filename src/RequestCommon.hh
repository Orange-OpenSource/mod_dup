#pragma once

#include <map>
#include <string>
#include <cstring>

namespace ApplicationScope {

/**
 * Scopes that a filter/sub can have
 * mask
 */
enum eApplicationScope{
    PATH = 1,
    QUERY_STRING = 2,
    URL = 3, // URL is path and query string
    HEADERS = 4,
    BODY = 8,
    METHOD = 16, // Http method (POST, PATCH, GET)
    URL_AND_HEADERS = 7,
    ALL = 31,
};
extern const char* c_ALL;
extern const char* c_PATH;
extern const char* c_QUERY_STRING;
extern const char* c_URL;
extern const char* c_HEADERS;
extern const char* c_URL_AND_HEADERS;
extern const char* c_BODY;
extern const char* c_ERROR_ON_STRING_VALUE;

/**
 * Translates the character value of a scope into it's enumerate value
 * raises a std::exception if the string doesn't match any predefined values
 * Values are : ALL, URL, BODY, HEADER
 */
eApplicationScope stringToEnum(const char* strValue) throw (std::exception);

/**
 * Translates the enumerate value of a scope into it's character value
 * raises a std::exception if the enumerate doesn't match any predefined values
 */
const char * enumToString(eApplicationScope scope) throw (std::exception);

}

