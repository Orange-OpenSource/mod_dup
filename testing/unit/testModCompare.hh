/*
* mod_compare - duplicates apache requests
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

class TestModCompare :
    public TestFixture
{

    CPPUNIT_TEST_SUITE(TestModCompare);

    CPPUNIT_TEST(testInit);
    CPPUNIT_TEST(testConfig);
    CPPUNIT_TEST(testPrintRequest);
    CPPUNIT_TEST(testCheckCassandraDiff);
    CPPUNIT_TEST(testGetLength);
    CPPUNIT_TEST(testDeserializeBody);
    CPPUNIT_TEST(testMap2string);
    CPPUNIT_TEST(testIterOverHeader);
    CPPUNIT_TEST(testWriteDifferences);
//    CPPUNIT_TEST(testInputFilterHandler);
//    CPPUNIT_TEST(tearDown);
//    CPPUNIT_TEST(testOutputFilterHandler);
    CPPUNIT_TEST_SUITE_END();

public:
    cmd_parms* getParms();
    void tearDown();
    void testInit();
    void testConfig();
    void testPrintRequest();
    void testCheckCassandraDiff();
    void testGetLength();
    void testDeserializeBody();
    void testInputFilterHandler();
    void testMap2string();
    void testIterOverHeader();
    void testWriteDifferences();
    void testOutputFilterHandler();

};
