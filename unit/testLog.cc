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

#include "testLog.hh"
#include "Log.hh"
#include <string>

// cppunit
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>

CPPUNIT_TEST_SUITE_REGISTRATION( TestLog );

void TestLog::Log()
{
    Log::close();
    Log::close();
    Log::init();
    Log::init();
    Log::close();
    Log::init();

    Log::debug("Test Message %s", "argument");
    Log::info(2,"Test Message %s", "argument");
    Log::notice(3,"Test Message %s", "argument");
    Log::warn(4,"Test Message %s", "argument");
    Log::error(5,"Test Message %s", "argument");
    Log::crit(6,"Test Message %s", "argument");

    Log::close();
    Log::init();
}
