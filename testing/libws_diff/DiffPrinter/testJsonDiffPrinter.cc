/*
 * jsonDiffPrinter_test.cpp
 *
 *  Created on: Apr 1, 2015
 *      Author: cvallee
 */

#include "../../src/libws_diff/DiffPrinter/jsonDiffPrinter.hh"
#include "testJsonDiffPrinter.hh"
#include<boost/scoped_ptr.hpp>

CPPUNIT_TEST_SUITE_REGISTRATION( TestJsonDiffPrinter );

void TestJsonDiffPrinter::testBasicPrint(){
	LibWsDiff::jsonDiffPrinter test("1");
	std::string json;
	CPPUNIT_ASSERT(!test.retrieveDiff(json));
	CPPUNIT_ASSERT_EQUAL(std::string("{\"id\":\"1\"}\n"),json);

}

void TestJsonDiffPrinter::testNormalJsonPrint(){
	LibWsDiff::jsonDiffPrinter test("2");
	test.addHeaderDiff("myHeaderKey",std::string("srcValue"),std::string("dstValue"));
	test.addRequestUri("/spp/main?version=1.0.0&country=FR&sid=ADVNEC&request=getPNS&infoS=AdviseCapping");
	test.addRequestHeader("X-ACCEPTED","Hello");
	test.addRequestHeader("X-Nawak","Bouh");
	test.addStatus("DUP",400);
	test.addStatus("COMP",200);

	std::string json;
	CPPUNIT_ASSERT(test.retrieveDiff(json));
	std::string expected="{\"id\":\"2\",\"diff\":{\"header\":{\"myHeaderKey\":"
			"{\"src\":\"srcValue\",\"dst\":\"dstValue\"}}},\"request\":"
			"{\"uri\":\"/spp/main?version=1.0.0&country=FR&sid=ADVNEC&"
			"request=getPNS&infoS=AdviseCapping\",\"url\":\"/spp/main\","
			"\"args\":{\"VERSION\":\"1.0.0\",\"COUNTRY\":\"FR\",\"SID\":"
			"\"ADVNEC\",\"REQUEST\":\"getPNS\",\"INFOS\":\"AdviseCapping\"},"
			"\"header\":{\"X-ACCEPTED\":\"Hello\",\"X-NAWAK\":\"Bouh\"}},"
			"\"status\":{\"DUP\":400,\"COMP\":200}}\n";
	CPPUNIT_ASSERT_EQUAL(expected,json);
}

void TestJsonDiffPrinter::testFullTooLong(){
	LibWsDiff::jsonDiffPrinter test("3");

	std::vector<std::string> myBody;
	std::string grbStr("garbageString",10000);
	for(int i=0;i<10;i++){
		myBody.push_back(std::string("<hello>")+grbStr);
	}
	test.addFullDiff(myBody,20,"XML");

	std::string json;
	CPPUNIT_ASSERT(test.retrieveDiff(json));
	std::string expected="{\"id\":\"3\",\"diff\":{\"body\":{\"full\":"
			"\"<hello>garbageString\\n<hello>garbageString\\n<hello>garbageString\\n"
			"<hello>garbageString\\n<hello>garbageString\\n<hello>garbageString\\n"
			"<hello>garbageString\\n<hello>garbageString\\n<hello>garbageString\\n"
			"<hello>garbageString\\n\"}}}\n";
	CPPUNIT_ASSERT_EQUAL(expected,json);
}

void TestJsonDiffPrinter::testFullExtractionOfXmlTags(){
	LibWsDiff::jsonDiffPrinter test("4");

	std::vector<std::string> myBody;
	std::string grbStr("garbageString",10000);
	myBody.push_back(std::string("<hello>")+grbStr);
	myBody.push_back(std::string("+<hello>")+grbStr);
	myBody.push_back(std::string("<he+llo>")+grbStr);
	myBody.push_back(std::string("+<he+llo>")+grbStr);
	myBody.push_back(std::string("   -<hello>")+grbStr);
	myBody.push_back(std::string("- -<hello>")+grbStr);
	myBody.push_back(std::string(" - <hello>")+grbStr);
	myBody.push_back(std::string("<hello>")+grbStr);
	test.addFullDiff(myBody,20,"XML");

	std::string json;
	CPPUNIT_ASSERT(test.retrieveDiff(json));
	std::string expected="{\"id\":\"4\",\"diff\":{\"body\":{\"posDiff\":2,"
			"\"posList\":[\"<he+llo>\",\"<hello>\"],\"negDiff\":3,\"negList\":"
			"[\"<hello>\"],\"full\":\"<hello>garbageString\\n+<hello>garbageStrin"
			"\\n<he+llo>garbageStrin\\n+<he+llo>garbageStri\\n   -<hello>garbageSt"
			"\\n- -<hello>garbageStr\\n - <hello>garbageStr\\n<hello>garbageString"
			"\\n\"}}}\n";

	CPPUNIT_ASSERT_EQUAL(expected,json);
}

void TestJsonDiffPrinter::testNullInHeader(){
	LibWsDiff::jsonDiffPrinter test("4");
	test.addHeaderDiff("nullDst",std::string("srcValue"), boost::none);
	test.addHeaderDiff("nullSrc",boost::none,std::string("dstValue"));

	std::string json;
	CPPUNIT_ASSERT(test.retrieveDiff(json));
	std::string expected="{\"id\":\"4\",\"diff\":{\"header\":{\"nullDst\":"
			"{\"src\":\"srcValue\"},\"nullSrc\":{\"dst\":\"dstValue\"}}}}\n";
	CPPUNIT_ASSERT_EQUAL(expected,json);
}

void TestJsonDiffPrinter::testUTF_8_OK(){
	boost::scoped_ptr<LibWsDiff::diffPrinter> test(LibWsDiff::diffPrinter::createDiffPrinter("5\xe6",LibWsDiff::diffPrinter::UTF8JSON));
	test->addHeaderDiff(std::string("\xe6\x97\xa5\xd1\x88\xfa"),std::string("\xe6\x97\xa5\xd1\x88\xfa"),boost::none);

	std::string json;
	CPPUNIT_ASSERT(test->retrieveDiff(json));
	std::string expected="{\"id\":\"5�\",\"diff\":{\"header\":{\"日ш�\":{\"src\":\"日ш�\"}}}}\n";
	CPPUNIT_ASSERT_EQUAL(expected,json);
}

