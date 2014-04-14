/*
* libws-diff - Custom diffing library - Tests dedicated to string diffing
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

#include "testWsStringDiff.hh"
#include "stringCompare.hh"

#include <boost/assign.hpp>

// cppunit
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

CPPUNIT_TEST_SUITE_REGISTRATION( TestWsStringDiff );

void TestWsStringDiff::testBasicCompare()
{
    LibWsDiff::StringCompare a;

    std::string result;
    CPPUNIT_ASSERT(a.retrieveDiff("essai no diff","essai no diff",result));
    CPPUNIT_ASSERT(result.empty());

    CPPUNIT_ASSERT(a.retrieveDiff("essai no diff","essai ni diff",result));
    CPPUNIT_ASSERT(std::string("@@ -5,7 +5,7 @@\ni n\n-o\n+i\n  di")==result);
}

void TestWsStringDiff::testBasicIgnore()
{
	LibWsDiff::StringCompare a;
	a.addIgnoreRegex("\\s*duplicate=fal[sS]e");
	std::string myDiff;
	CPPUNIT_ASSERT(a.retrieveDiff("mysuper diff matching duplicate=false","mysuper diff matching",myDiff));
	CPPUNIT_ASSERT(myDiff.empty());
	CPPUNIT_ASSERT(a.retrieveDiff("mysuper diff matching duplicate=falSe","mysuper diff matching",myDiff));
	CPPUNIT_ASSERT(myDiff.empty());
	CPPUNIT_ASSERT(a.retrieveDiff("mysuper diff matching duplicate=fals","mysuper diff matching",myDiff));
	CPPUNIT_ASSERT(!myDiff.empty());
}

void TestWsStringDiff::testBasicStop()
{
	LibWsDiff::StringCompare a;
	a.addStopRegex("duplicate=fal[sS]e");
	std::string myDiff;
	CPPUNIT_ASSERT(!a.retrieveDiff("mysuper diff matching duplicate=false","",myDiff));
	CPPUNIT_ASSERT(myDiff.empty());
	CPPUNIT_ASSERT(!a.retrieveDiff("mysuper diff matching duplicate=falSe","",myDiff));
	CPPUNIT_ASSERT(myDiff.empty());
	CPPUNIT_ASSERT(!a.retrieveDiff("","mysuper diff matching duplicate=falSe",myDiff));
	CPPUNIT_ASSERT(myDiff.empty());
	CPPUNIT_ASSERT(a.retrieveDiff("mysuper diff matching duplicate=fals","",myDiff));
	CPPUNIT_ASSERT(!myDiff.empty());
}

void TestWsStringDiff::testHeaderStringDiff(){
	std::vector<std::string> stopRe = boost::assign::list_of("duplicate=False")("stopregex");
	std::vector<std::string> igRe = boost::assign::list_of("test");
	std::string diff;

	LibWsDiff::StringCompareHeader b;
	LibWsDiff::StringCompareHeader a(stopRe,igRe);
	CPPUNIT_ASSERT(a.retrieveDiff("line1\nline2\nline3","line1\nline4\nline3",diff));
	CPPUNIT_ASSERT(std::string("@@ -1,3 +1,3 @@\n line1\n-line2\n+line4\n line3\n")==diff);
	//validation stop regexes processed
	CPPUNIT_ASSERT(!a.retrieveDiff("line1\nline2\nline3 stopregex","line1\nline4\nline3",diff));
}

void TestWsStringDiff::testBodyStringDiff(){
	std::vector<std::string> stopRe = boost::assign::list_of("duplicate=False")("stopregex");
	std::vector<std::string> igRe = boost::assign::list_of("test");
	std::string diff;

	LibWsDiff::StringCompareBody a(stopRe,igRe);

	CPPUNIT_ASSERT(a.retrieveDiff("<line1test><line2>content<line3><line4>","<line1><line4>content<line3><line4>",diff));
	CPPUNIT_ASSERT(std::string("@@ -1,3 +1,3 @@\n <line1>\n-<line2>content<line3>\n+<line4>content<line3>\n <line4>\n")==diff);

	//validation stop regexes
	CPPUNIT_ASSERT(!a.retrieveDiff("<line1test><line2 duplicate=Fals>content<line3stopregex><line4>","",diff));

}

void TestWsStringDiff::testBodyVectDiff(){
	std::vector<std::string> stopRe = boost::assign::list_of("duplicate=False")("stopregex");
	std::vector<std::string> igRe = boost::assign::list_of("test");
	std::string diff;

	LibWsDiff::StringCompareBody b;
	LibWsDiff::StringCompareBody a(stopRe,igRe);
	std::vector<std::string> input1 = boost::assign::list_of("<line1test>")("<line2>content<line3>")("<line4>");
	std::vector<std::string> input2 = boost::assign::list_of("<line1>")("<line4>content<line3>")("<line4>");
	CPPUNIT_ASSERT(a.retrieveDiff(input1,input2,diff));
	CPPUNIT_ASSERT(std::string("@@ -1,3 +1,3 @@\n <line1>\n-<line2>content<line3>\n+<line4>content<line3>\n <line4>\n")==diff);
	//Validation stop regexes process
	input1[0]="<line1test duplicate=Fals stopregex>";
	CPPUNIT_ASSERT(!a.retrieveDiff(input1,input2,diff));
}
