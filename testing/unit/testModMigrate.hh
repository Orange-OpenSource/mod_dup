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

class TestModMigrate :
    public TestFixture
{

    CPPUNIT_TEST_SUITE( TestModMigrate );
    CPPUNIT_TEST( testEnrichContext );
    CPPUNIT_TEST( testTranslateHook );
    CPPUNIT_TEST( testInputFilterBody2Brigade );
    CPPUNIT_TEST( testConfig );
    CPPUNIT_TEST( testScope );
    CPPUNIT_TEST( testMigrateEnv );
    CPPUNIT_TEST( testInitAndCleanUp );
    CPPUNIT_TEST_SUITE_END();

public:
    void testEnrichContext();
    void testTranslateHook();
    void testInputFilterBody2Brigade();
    void testConfig();
    void testScope();
    void testMigrateEnv();
    void testInitAndCleanUp();
};
