/*
* libws-diff - Custom diffing library - Tests dedicated to map diffing
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

class TestWsMapDiff :
    public TestFixture
{
    CPPUNIT_TEST_SUITE(TestWsMapDiff);
    CPPUNIT_TEST(testAddingStopRegex);
    CPPUNIT_TEST(testAddingIgnoreRegex);
    CPPUNIT_TEST(testMapDiff);
    CPPUNIT_TEST(testMapDiffPrinterJson);
    CPPUNIT_TEST(testMapDiffPrinterMultiline);
    CPPUNIT_TEST_SUITE_END();

public:
    void testAddingStopRegex();
    void testAddingIgnoreRegex();

    void testMapDiff();
    void testMapDiffPrinterJson();
    void testMapDiffPrinterMultiline();

};
