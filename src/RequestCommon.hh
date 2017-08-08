#pragma once

#include <map>
#include <string>
#include <cstring>

namespace ApplicationScope {

/**
 * Scopes that a filter/sub can have
 */
enum eApplicationScope{
    ALL = 0x7,
    URL = 0x1,
    HEADER = 0x2,
    BODY = 0x4,
};
extern const char* c_ALL;
extern const char* c_URL;
extern const char* c_HEADER;
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

