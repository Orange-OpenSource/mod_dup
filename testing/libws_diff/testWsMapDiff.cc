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

#include "testWsMapDiff.hh"
#include "mapCompare.hh"

#include <boost/assign.hpp>

// cppunit
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

CPPUNIT_TEST_SUITE_REGISTRATION( TestWsMapDiff );

void TestWsMapDiff::testAddingStopRegex(){
	LibWsDiff::MapCompare myCmp;
	myCmp.addStopRegex("agent-type","my-agent-stopper");
	myCmp.addStopRegex("duplicate","[fF]alse");
	std::map<std::string,std::string> test = boost::assign::map_list_of("date","today")("agent-type","superAgent")("sexyAttribute","Sexy")("ignore","ignoretest");
	CPPUNIT_ASSERT(!myCmp.checkStop(test));
	test["agent-type"]="my-agent-stopper";
	CPPUNIT_ASSERT(myCmp.checkStop(test));
	test.erase("agent-type");
	test["duplicate"]="False";
	CPPUNIT_ASSERT(myCmp.checkStop(test));

	std::map<std::string,std::string> test2;
	std::string diff;
	CPPUNIT_ASSERT(!myCmp.retrieveDiff(test,test2,diff));
	CPPUNIT_ASSERT(diff.empty());
	CPPUNIT_ASSERT(!myCmp.retrieveDiff(test2,test,diff));
	CPPUNIT_ASSERT(diff.empty());

}

void TestWsMapDiff::testAddingIgnoreRegex(){
	LibWsDiff::MapCompare myCmp;
	myCmp.addIgnoreRegex("agent-type",".*");
	myCmp.addIgnoreRegex("ignore","test");
	std::map<std::string,std::string> test = boost::assign::map_list_of("date","today")("agent-type","superAgent")("sexyAttribute","Sexy")("ignore","ignoretest");
	myCmp.applyIgnoreRegex(test);
	CPPUNIT_ASSERT(test.find("ignore")->second=="ignore");
	CPPUNIT_ASSERT(test.find("agent-type")==test.end());
}

void TestWsMapDiff::testMapDiff(){
	std::vector<std::string> stopRe = boost::assign::list_of("duplicate=False")("stopregex");
	std::vector<std::string> igRe = boost::assign::list_of("test");
	std::string diff;

	std::map<std::string,std::string> test = boost::assign::map_list_of("date","today")("agent-type","superAgent")("sexyAttribute","Sexy")("ignore","ignoretest");
	std::map<std::string,std::string> test2 = boost::assign::map_list_of("date","20140211T13141516")("agent-type","superAgent2")("newSexyAttribute","Sexy")("ignore","ignore");

	LibWsDiff::MapCompare a;
	a.addIgnoreRegex("ignore","test");
	a.addIgnoreRegex("date",".*");
	CPPUNIT_ASSERT(a.retrieveDiff(test,test2,diff));

	CPPUNIT_ASSERT(std::string("Key missing in the destination map :\n"
			"'sexyAttribute' ==> 'Sexy'\n"
			"Key missing in src map :\n"
			"'newSexyAttribute' ==> 'Sexy'\n"
			"Key with value differences :\n"
			"'agent-type' ==> 'superAgent'/'superAgent2'\n")==diff);
}
