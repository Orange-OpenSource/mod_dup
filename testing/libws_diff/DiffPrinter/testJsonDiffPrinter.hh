/*
 * jsonDiffPrinter_test.hh
 *
 *  Created on: Apr 2, 2015
 *      Author: cvallee
 */

#pragma once

#include "../../../src/libws_diff/DiffPrinter/diffPrinter.hh"
#include "../../../src/libws_diff/DiffPrinter/jsonDiffPrinter.hh"

#include <cppunit/extensions/HelperMacros.h>

#ifdef CPPUNIT_HAVE_NAMESPACES
using namespace CPPUNIT_NS;
#endif

class TestJsonDiffPrinter :
    public TestFixture
{
    CPPUNIT_TEST_SUITE(TestJsonDiffPrinter);
    CPPUNIT_TEST(testBasicPrint);
    CPPUNIT_TEST(testNormalJsonPrint);
    CPPUNIT_TEST(testFullTooLong);
    CPPUNIT_TEST(testNullInHeader);
    CPPUNIT_TEST_SUITE_END();

public:
    void testBasicPrint();
    void testNormalJsonPrint();
    void testFullTooLong();
    void testNullInHeader();
};
