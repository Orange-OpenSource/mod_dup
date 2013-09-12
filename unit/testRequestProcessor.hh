#ifndef TESTRequestProcessor_HH_
#define TESTRequestProcessor_HH_

#include <cppunit/extensions/HelperMacros.h>


#ifdef CPPUNIT_HAVE_NAMESPACES
using namespace CPPUNIT_NS;
#endif

class TestRequestProcessor :
    public TestFixture
{

    CPPUNIT_TEST_SUITE(TestRequestProcessor);
    CPPUNIT_TEST(init);
    CPPUNIT_TEST(testUrlEncodeDecode);
    CPPUNIT_TEST(testParseArgs);
    CPPUNIT_TEST(testFilter);
    CPPUNIT_TEST(testSubstitution);
    CPPUNIT_TEST(testFilterAndSubstitution);
    CPPUNIT_TEST(testRun);
    CPPUNIT_TEST(testFilterBasic);
    CPPUNIT_TEST(testRawSubstitution);
    CPPUNIT_TEST_SUITE_END();

public:
    void init();
    void testUrlEncodeDecode();
    void testFilter();
    void testSubstitution();
    void testFilterAndSubstitution();
    void testParseArgs();
    void testRun();
    void testFilterBasic();
    void testRawSubstitution();
};


#endif /* TESTRequestProcessor_HH_ */
