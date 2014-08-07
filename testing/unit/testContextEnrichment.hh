#if 0
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

#pragma once

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


class TestContextEnrichment :
    public TestFixture
{

    CPPUNIT_TEST_SUITE(TestContextEnrichment);
    CPPUNIT_TEST(testConfiguration);
    CPPUNIT_TEST(testEnrichBody);
    CPPUNIT_TEST(testEnrichHeader);
    CPPUNIT_TEST(testEnrichBoth);
    CPPUNIT_TEST_SUITE_END();

public:

    /**
     * Tests that the configuration behavior is correct
     */
    void testConfiguration();

    /**
     * Tests that the enrichement is OK with the application on request headers
     */
    void testEnrichHeader();

    /**
     * Tests that the enrichement is OK with the application on the request body
     */
    void testEnrichBody();

    /**
     * Tests that the enrichement is OK with the application on request headers and body
     */
    void testEnrichBoth();


    virtual void setUp();
    virtual void tearDown();

private:
    cmd_parms * mParms;
};

#endif
