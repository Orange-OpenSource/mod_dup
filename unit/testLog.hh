#ifndef TESTLog_HH_
#define TESTLog_HH_

#include <cppunit/extensions/HelperMacros.h>


#ifdef CPPUNIT_HAVE_NAMESPACES
using namespace CPPUNIT_NS;
#endif

class TestLog :
    public TestFixture
{

    CPPUNIT_TEST_SUITE( TestLog );
    CPPUNIT_TEST( Log );
    CPPUNIT_TEST_SUITE_END();

public:
    void Log();
};


#endif /* TESTLog_HH_ */
