#ifndef TESTModDup_HH_
#define TESTModDup_HH_

#include <httpd.h>
#include <http_config.h>  // for the config functions
#include <http_request.h>
#include <http_protocol.h>
#include <apr_pools.h>
#include <apr_hooks.h>

#include <cppunit/extensions/HelperMacros.h>


#ifdef CPPUNIT_HAVE_NAMESPACES
using namespace CPPUNIT_NS;
#endif

class TestModDup :
    public TestFixture
{

    CPPUNIT_TEST_SUITE(TestModDup);
    CPPUNIT_TEST(testInit);
    CPPUNIT_TEST(testConfig);
    CPPUNIT_TEST(testRequestHandler);
    CPPUNIT_TEST(testInitAndCleanUp);
    CPPUNIT_TEST_SUITE_END();

public:
	cmd_parms* getParms();
	void testInit();
	void testConfig();
	void testRequestHandler();
	void testInitAndCleanUp();
};


#endif /* TESTModDup_HH_ */
