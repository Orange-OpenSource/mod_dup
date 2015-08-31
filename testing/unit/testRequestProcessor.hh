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

#include <cppunit/extensions/HelperMacros.h>

#ifdef CPPUNIT_HAVE_NAMESPACES
using namespace CPPUNIT_NS;
#endif

class TestRequestProcessor :
    public TestFixture
{

    CPPUNIT_TEST_SUITE(TestRequestProcessor);
    CPPUNIT_TEST(testParseArgs);
    CPPUNIT_TEST(testAddHeadersIn);
    CPPUNIT_TEST(testFilter);
    CPPUNIT_TEST(testSubstitution);
    CPPUNIT_TEST(testRun);
    CPPUNIT_TEST(testFilterBasic);
    CPPUNIT_TEST(testRawSubstitution);
    CPPUNIT_TEST(testDupFormat);
    CPPUNIT_TEST(testRequestInfo);
    CPPUNIT_TEST(testKeySubstitutionOnBody);
    CPPUNIT_TEST(testTimeout);
    CPPUNIT_TEST(testFilterOnNotMatching);
    CPPUNIT_TEST(testMultiDestination);

    CPPUNIT_TEST_SUITE_END();

public:
    void testUrlEncodeDecode();
    void testFilter();
    void testSubstitution();
    void testParseArgs();
    void testAddHeadersIn();
    void testRun();
    void testFilterBasic();
    void testRawSubstitution();
    void testRequestInfo();
    void testTimeout();
    void testFilterOnNotMatching();

    /**
     * Tests the key substitution on the request body
     */
    void testKeySubstitutionOnBody();

    /**
     * Tests that the a duplicated request respects the dup format
     */
    void testDupFormat();

    /**
     * @brief Tests that a single request can be duplicated several times depending on the location
     */
    void testMultiDestination();

};
