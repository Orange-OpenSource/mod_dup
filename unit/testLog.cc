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

    Log::debug(1,"Test Message %s", "argument");
    Log::info(2,"Test Message %s", "argument");
    Log::notice(3,"Test Message %s", "argument");
    Log::warn(4,"Test Message %s", "argument");
    Log::error(5,"Test Message %s", "argument");
    Log::crit(6,"Test Message %s", "argument");

    Log::close();
    Log::init();
}
