/*
 * jsonDiffPrinter_test.cpp
 *
 *  Created on: Apr 1, 2015
 *      Author: cvallee
 */

#include "../../src/libws_diff/DiffPrinter/jsonDiffPrinter.hh"
#include "testJsonDiffPrinter.hh"

CPPUNIT_TEST_SUITE_REGISTRATION( TestJsonDiffPrinter );

void TestJsonDiffPrinter::testBasicPrint(){
	LibWsDiff::jsonDiffPrinter test(10);
	std::string json;
	CPPUNIT_ASSERT(!test.retrieveDiff(json));
	CPPUNIT_ASSERT_EQUAL(std::string("{\"id\":10}\n"),json);

}

void TestJsonDiffPrinter::testNormalJsonPrint(){
	LibWsDiff::jsonDiffPrinter test(10);
	test.addHeaderDiff("myHeaderKey","srcValue","dstValue");
	test.addRequestUri("/spp/main?version=1.0.0&country=FR&sid=ADVNEC&request=getPNS&infos=AdviseCapping");
	test.addRequestHeader("X-ACCEPTED","Hello");
	test.addStatus("DUP",400);
	test.addStatus("COMP",200);

	std::string json;
	CPPUNIT_ASSERT(test.retrieveDiff(json));
	std::cout << json;
	std::string expected="{\"id\":10,\"diff\":{\"header\":{\"myHeaderKey\":"
			"{\"src\":\"srcValue\",\"dst\":\"dstValue\"}}},\"request\":"
			"{\"uri\":\"/spp/main?version=1.0.0&country=FR&sid=ADVNEC&request="
			"getPNS&infos=AdviseCapping\",\"url\":\"/spp/main\",\"args\":"
			"{\"version\":\"1.0.0\",\"country\":\"FR\",\"sid\":\"ADVNEC\","
			"\"request\":\"getPNS\",\"infos\":\"AdviseCapping\"},\"header\":"
			"{\"X-ACCEPTED\":\"Hello\"}},\"status\":{\"DUP\":400,\"COMP\":200}}\n";
	CPPUNIT_ASSERT_EQUAL(expected,json);
}

void TestJsonDiffPrinter::TestFullTooLong(){

}
