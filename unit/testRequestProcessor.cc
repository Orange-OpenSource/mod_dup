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

#include "RequestProcessor.hh"
#include "MultiThreadQueue.hh"
#include "testRequestProcessor.hh"

// cppunit
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

CPPUNIT_TEST_SUITE_REGISTRATION( TestRequestProcessor );

using namespace DupModule;

void TestRequestProcessor::testRun()
{
    RequestProcessor proc;
    MultiThreadQueue<RequestInfo> queue;

    // No destination set yet, so will return straight away even without poison request
    proc.run(queue);

    // This request won't go anywhere, but at least we exersize the loop in proc.run()
    queue.push(RequestInfo("/spp/main", "/spp/main", "SID=ID_REQ&CREDENTIAL=1,toto&"));
    queue.push(POISON_REQUEST);

    // We don't actually test that the value is set
    proc.setDestination("localhost:8080");

    // If the poison pill would not work, this call would hang forever
    proc.run(queue);

    // We could hack a web server with nc to test the rest of this method,
    // but this might be overkill for a unit test
}

void TestRequestProcessor::testFilterAndSubstitution()
{
    RequestProcessor proc;
    std::string query;

    // Filter
    proc.addFilter("/toto", "INFO", "[my]+", ApplicationScope::ALL);
    proc.addRawFilter("/toto", "[my]+", ApplicationScope::ALL);
    proc.addSubstitution("/toto", "INFO", "[i]", "f", tFilterBase::eFilterScope::HEADER);

    query = "titi=tata&tutu";

    RequestInfo ri = RequestInfo("/toto", "/toto/pws/titi/", "INFO=myinfo");
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("INFO=myfnfo"));
}

void TestRequestProcessor::testSubstitution()
{
    RequestProcessor proc;
    std::string query;

    proc.addSubstitution("/toto", "titi", "[ae]", "-", tFilterBase::eFilterScope::HEADER);

    query = "titi=tatae&tutu=tatae";
    RequestInfo ri = RequestInfo("/toto", "/toto", query);


    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=t-t--&TUTU=tatae"), ri.mArgs);

    //  Empty fields are preserved
    query = "titi=tatae&tutu";
    ri = RequestInfo("/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=t-t--&TUTU"), ri.mArgs);

    //  Empty fields can be substituted
    proc.addSubstitution("/toto", "tutu", "^$", "titi", tFilterBase::eFilterScope::HEADER);
    query = "titi=tatae&tutu";
    ri = RequestInfo("/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=t-t--&TUTU=titi"), ri.mArgs);

    // Substitutions are case-sensitive
    query = "titi=TATAE&tutu=TATAE";
    ri = RequestInfo("/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=TATAE&TUTU=TATAE"), ri.mArgs);

    // Substitutions on the same path and field are executed in the order they are added
    proc.addSubstitution("/toto", "titi", "-(.*)-", "T\\1", tFilterBase::eFilterScope::HEADER);
    query = "titi=tatae&tutu=tatae";
    ri = RequestInfo("/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=tTt-&TUTU=tatae"), ri.mArgs);

    // Substituions in other field but same path
    proc.addSubstitution("/toto", "tutu", "ata", "W", tFilterBase::eFilterScope::HEADER);
    query = "titi=tatae&tutu=tatae";
    ri = RequestInfo("/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=tTt-&TUTU=tWe"), ri.mArgs);

    // Substitution on another path
    proc.addSubstitution("/x/y/z", "tutu", "ata", "W", tFilterBase::eFilterScope::HEADER);
    query = "titi=tatae&tutu=tatae";
    ri = RequestInfo("/x/y/z", "/x/y/z", query);
    CPPUNIT_ASSERT(proc.processRequest("/x/y/z", ri));
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=tatae&TUTU=tWe"), ri.mArgs);

    // ... doesn't affect previous path
    query = "titi=tatae&tutu=tatae";
    ri = RequestInfo("/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=tTt-&TUTU=tWe"), ri.mArgs);

    // ... nor unknow path
    query = "titi=tatae&tutu=tatae";
    ri = RequestInfo("/UNKNOWN", "/UNKNOWN", query);
    CPPUNIT_ASSERT(proc.processRequest("/UNKNOWN", ri));
    CPPUNIT_ASSERT_EQUAL(std::string("titi=tatae&tutu=tatae"), ri.mArgs);

    // Substitute escaped characters
    proc.addSubstitution("/escaped", "titi", ",", "/", tFilterBase::eFilterScope::HEADER);
    query = "titi=1%2C2%2C3";
    ri = RequestInfo("/escaped", "/escaped", query);
    CPPUNIT_ASSERT(proc.processRequest("/escaped", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("TITI=1%2f2%2f3"));

    // Keys should be compared case-insensitively
    query = "TiTI=1%2C2%2C3";
    ri = RequestInfo("/UNKNOWN", "/UNKNOWN", query);
    CPPUNIT_ASSERT(proc.processRequest("/escaped", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("TITI=1%2f2%2f3"));
}

void TestRequestProcessor::init()
{
    apr_initialize();
    Log::init(NULL);
}

void TestRequestProcessor::testFilterBasic()
{
    {
        // Simple Filter MATCH
        RequestProcessor proc;
        proc.addFilter("/toto", "INFO", "[my]+", ApplicationScope::ALL);
        RequestInfo ri = RequestInfo("/toto", "/toto/pws/titi/", "INFO=myinfo");
        CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    }

    {
        // Simple Filter NO MATCH
        RequestProcessor proc;
        proc.addFilter("/toto", "INFO", "KIDO", ApplicationScope::ALL);
        RequestInfo ri = RequestInfo("/toto", "/toto/pws/titi/", "INFO=myinfo");
        CPPUNIT_ASSERT(!proc.processRequest("/toto", ri));
    }

    {
        // Filter applied on body only NO MATCH
        RequestProcessor proc;
        proc.addFilter("/toto", "INFO", "my", ApplicationScope::BODY);
        RequestInfo ri = RequestInfo("/toto", "/toto/pws/titi/", "INFO=myinfo");
        CPPUNIT_ASSERT(!proc.processRequest("/toto", ri));
    }

    {
        // Filter applied on body only MATCH
        RequestProcessor proc;
        proc.addFilter("/bb", "BODY", "hello", ApplicationScope::BODY);
        std::string body = "BODY=hello";
        RequestInfo ri = RequestInfo("/bb", "/bb/pws/titi/", "INFO=myinfo", &body);
        CPPUNIT_ASSERT(proc.processRequest("/bb", ri));
    }



}

void TestRequestProcessor::testFilter()
{
    RequestProcessor proc;

    // // // proc.addFilter("/toto", "INFO", "[my]+", ApplicationScope::ALL);
    // // // RequestInfo ri = RequestInfo("/toto", "/toto/pws/titi/", "INFO=myinfo");
    // // // CPPUNIT_ASSERT(proc.processRequest("/toto", ri));

    std::string query;

    // No filter, so everything should pass
    query = "titi=tata&tutu";
    RequestInfo ri = RequestInfo("/toto", "/toto/pws/titi/", query);

    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(std::string("titi=tata&tutu"), ri.mArgs);

    query = "";
    ri = RequestInfo("", "", query);

    CPPUNIT_ASSERT(proc.processRequest("", ri));
    CPPUNIT_ASSERT_EQUAL(std::string(""), ri.mArgs);

    // Filter
    proc.addFilter("/toto", "titi", "^ta", ApplicationScope::HEADER);
    query = "titi=tata&tutu";
    ri = RequestInfo("/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("titi=tata&tutu"));

    query = "tata&tutu";
    ri = RequestInfo("/toto", "/toto", query);
    CPPUNIT_ASSERT(!proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("tata&tutu"));

    query = "tititi=tata&tutu";
    ri = RequestInfo("/toto", "/toto", query);
    CPPUNIT_ASSERT(!proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("tititi=tata&tutu"));

    // Filters are case-insensitive
    query = "TITi=tata&tutu";
    ri = RequestInfo("/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("TITi=tata&tutu"));

    // On other paths, no filter is applied
    query = "titi=tata&tutu";
    ri = RequestInfo("/to", "/to", query);
    CPPUNIT_ASSERT(proc.processRequest("/to", ri));
    CPPUNIT_ASSERT_EQUAL(query, std::string("titi=tata&tutu"));

    query = "tata&tutu";
    ri = RequestInfo("/toto", "/toto/bla", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto/bla", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("tata&tutu"));

    // Two filters on same path - either of them has to match
    proc.addFilter("/toto", "titi", "[tu]{2,15}", ApplicationScope::HEADER);
    query = "titi=tata&tutu";
    ri = RequestInfo("/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("titi=tata&tutu"));

    query = "titi=tutu";
    ri = RequestInfo("/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("titi=tutu"));

    query = "titi=t";
    ri = RequestInfo("/toto", "/toto", query);
    CPPUNIT_ASSERT(!proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("titi=t"));

    // Two filters on different paths
    proc.addFilter("/some/path", "x", "^.{3,5}$", ApplicationScope::HEADER);
    query = "x=1234";
    ri = RequestInfo("/some/path", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/some/path", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("x=1234"));

    query = "x=123456";
    ri = RequestInfo("/some/path", "/some/path", query);
    CPPUNIT_ASSERT(!proc.processRequest("/some/path", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("x=123456"));

    // New filter should not change filter on other path
    query = "titi=tutu";
    ri = RequestInfo("/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("titi=tutu"));

    query = "ti=tu";
    ri = RequestInfo("/toto", "/toto", query);
    CPPUNIT_ASSERT(!proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("ti=tu"));

    // Unknown paths still shouldn't have a filter applied
    query = "ti=tu";
    ri = RequestInfo("/waaazzaaaa", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/waaazzaaaa", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("ti=tu"));

    // // Filter escaped characters
    proc.addFilter("/escaped", "y", "^ ", ApplicationScope::HEADER);
    query = "y=%20";
    ri = RequestInfo("/escaped", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/escaped", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("y=%20"));
}

void TestRequestProcessor::testParseArgs()
{
    RequestProcessor proc;
    std::string query;
    std::list<std::pair<std::string, std::string> > lParsedArgs;

    query = "titi=tAta1,2#&tutu";
    proc.parseArgs(lParsedArgs, query);
    CPPUNIT_ASSERT_EQUAL(lParsedArgs.front().first, std::string("TITI"));
    CPPUNIT_ASSERT_EQUAL(lParsedArgs.front().second, std::string("tAta1,2#"));
    lParsedArgs.pop_front();

    CPPUNIT_ASSERT_EQUAL(lParsedArgs.front().first, std::string("TUTU"));
    CPPUNIT_ASSERT_EQUAL(lParsedArgs.front().second, std::string(""));
    lParsedArgs.pop_front();
}

void TestRequestProcessor::testRawSubstitution()
{
    {
        // Simple substitution on a header
        // Body untouched
        RequestProcessor proc;
        std::string query;

        query = "arg1=myarg1";
        std::string body = "mybody1test";
        RequestInfo ri = RequestInfo("/toto", "/toto/titi/", query, &body);

        proc.addRawSubstitution("/toto", "1", "2", tFilterBase::eFilterScope::HEADER);
        CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
        CPPUNIT_ASSERT_EQUAL(std::string("arg2=myarg2"), ri.mArgs);
        CPPUNIT_ASSERT_EQUAL(body, ri.mBody);
    }

    {
        // Simple substitution on a body
        // Header untouched
        RequestProcessor proc;
        std::string query;

        query = "arg1=myarg1";
        std::string body = "mybody1test";
        RequestInfo ri = RequestInfo("/toto", "/toto/titi/", query, &body);

        proc.addRawSubstitution("/toto", "1", "2", tFilterBase::eFilterScope::BODY);
        CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
        CPPUNIT_ASSERT_EQUAL(query, ri.mArgs);
        CPPUNIT_ASSERT_EQUAL(std::string("mybody2test"), ri.mBody);
    }

    {
        // Simple substitution on body AND HEADER
        RequestProcessor proc;
        std::string query;

        query = "arg1=myarg1";
        std::string body = "mybody1test";
        RequestInfo ri = RequestInfo("/toto", "/toto/titi/", query, &body);

        proc.addRawSubstitution("/toto", "1", "2", ApplicationScope::ALL);
        CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
        CPPUNIT_ASSERT_EQUAL(std::string("arg2=myarg2"), ri.mArgs);
        CPPUNIT_ASSERT_EQUAL(std::string("mybody2test"), ri.mBody);
    }

    {
        // Different substitutions on body AND HEADER
        RequestProcessor proc;
        std::string query;

        query = "arg1=myarg1";
        std::string body = "mybody1test";
        RequestInfo ri = RequestInfo("/toto", "/toto/titi/", query, &body);

        proc.addRawSubstitution("/toto", "1", "2", ApplicationScope::HEADER);
        proc.addRawSubstitution("/toto", "1", "3", ApplicationScope::BODY);
        CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
        CPPUNIT_ASSERT_EQUAL(std::string("arg2=myarg2"), ri.mArgs);
        CPPUNIT_ASSERT_EQUAL(std::string("mybody3test"), ri.mBody);
    }

}
