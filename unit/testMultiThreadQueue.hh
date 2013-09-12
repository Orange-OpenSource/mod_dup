#ifndef TESTMultiThreadQueue_HH_
#define TESTMultiThreadQueue_HH_

#include <cppunit/extensions/HelperMacros.h>


#ifdef CPPUNIT_HAVE_NAMESPACES
using namespace CPPUNIT_NS;
#endif

class TestMultiThreadQueue :
    public TestFixture
{

    CPPUNIT_TEST_SUITE(TestMultiThreadQueue);
    CPPUNIT_TEST(run);
    CPPUNIT_TEST_SUITE_END();

public:
    void run();
};


#endif /* TESTMultiThreadQueue_HH_ */
