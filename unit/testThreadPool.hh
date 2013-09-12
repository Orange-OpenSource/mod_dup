#ifndef TESTThreadPool_HH_
#define TESTThreadPool_HH_

#include <cppunit/extensions/HelperMacros.h>


#ifdef CPPUNIT_HAVE_NAMESPACES
using namespace CPPUNIT_NS;
#endif

class TestThreadPool :
    public TestFixture
{

    CPPUNIT_TEST_SUITE(TestThreadPool);
    CPPUNIT_TEST(run);
    CPPUNIT_TEST_SUITE_END();

public:
    void run();
};


#endif /* TESTThreadPool_HH_ */
