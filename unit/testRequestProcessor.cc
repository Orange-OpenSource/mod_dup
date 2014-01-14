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
#include "mod_dup.hh"

// cppunit
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

CPPUNIT_TEST_SUITE_REGISTRATION( TestRequestProcessor );

using namespace DupModule;

void TestRequestProcessor::testRun()
{
    RequestProcessor proc;
    MultiThreadQueue<const RequestInfo *> queue;

    DupConf conf;
    conf.currentApplicationScope = ApplicationScope::ALL;
    conf.currentDupDestination = "Honolulu:8080";
    // Filter
    proc.addFilter("/toto", "INFO", "[my]+", conf);

    // This request won't go anywhere, but at least we exersize the loop in proc.run()
    queue.push(new RequestInfo(1,"/spp/main", "/spp/main", "SID=ID_REQ&CREDENTIAL=1,toto&"));
    queue.push(&POISON_REQUEST);

    // If the poison pill would not work, this call would hang forever
    proc.run(queue);

    // We could hack a web server with nc to test the rest of this method,
    // but this might be overkill for a unit test
}

void TestRequestProcessor::testFilterAndSubstitution()
{
    RequestProcessor proc;
    std::string query;
    DupConf conf;
    conf.currentApplicationScope = ApplicationScope::ALL;
    // Filter
    proc.addFilter("/toto", "INFO", "[my]+", conf);
    proc.addRawFilter("/toto", "[my]+", conf);
    conf.currentApplicationScope = ApplicationScope::HEADER;
    proc.addSubstitution("/toto", "INFO", "[i]", "f", conf);

    query = "titi=tata&tutu";

    RequestInfo ri = RequestInfo(1,"/toto", "/toto/pws/titi/", "INFO=myinfo");
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("INFO=myfnfo"));
}

void TestRequestProcessor::testSubstitution()
{
    RequestProcessor proc;
    std::string query;
    DupConf conf;
    proc.addRawFilter("/toto", ".*", conf);

    proc.addSubstitution("/toto", "titi", "[ae]", "-", conf);

    query = "titi=tatae&tutu=tatae";
    RequestInfo ri = RequestInfo(10, "/toto", "/toto", query);


    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=t-t--&TUTU=tatae"), ri.mArgs);

    //  Empty fields are preserved
    query = "titi=tatae&tutu";
    ri = RequestInfo(1,"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=t-t--&TUTU"), ri.mArgs);

    //  Empty fields can be substituted
    proc.addSubstitution("/toto", "tutu", "^$", "titi", conf);
    query = "titi=tatae&tutu";
    ri = RequestInfo(1,"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=t-t--&TUTU=titi"), ri.mArgs);

    // Substitutions are case-sensitive
    query = "titi=TATAE&tutu=TATAE";
    ri = RequestInfo(1,"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=TATAE&TUTU=TATAE"), ri.mArgs);

    // Substitutions on the same path and field are executed in the order they are added
    proc.addSubstitution("/toto", "titi", "-(.*)-", "T\\1", conf);
    query = "titi=tatae&tutu=tatae";
    ri = RequestInfo(1, "/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=tTt-&TUTU=tatae"), ri.mArgs);

    // Substituions in other field but same path
    proc.addSubstitution("/toto", "tutu", "ata", "W", conf);
    query = "titi=tatae&tutu=tatae";
    ri = RequestInfo(1,"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=tTt-&TUTU=tWe"), ri.mArgs);

    // No Substitution on another path
    proc.addSubstitution("/x/y/z", "tutu", "ata", "W", conf);
    query = "titi=tatae&tutu=tatae";
    ri = RequestInfo(1,"/x/y/z", "/x/y/z", query);
    CPPUNIT_ASSERT(!proc.processRequest("/x/y/z", ri));
    CPPUNIT_ASSERT_EQUAL(std::string("titi=tatae&tutu=tatae"), ri.mArgs);

    // ... doesn't affect previous path
    query = "titi=tatae&tutu=tatae";
    ri = RequestInfo(1,"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=tTt-&TUTU=tWe"), ri.mArgs);

    // ... nor unknow path
    query = "titi=tatae&tutu=tatae";
    ri = RequestInfo(1,"/UNKNOWN", "/UNKNOWN", query);
    CPPUNIT_ASSERT(!proc.processRequest("/UNKNOWN", ri));
    CPPUNIT_ASSERT_EQUAL(std::string("titi=tatae&tutu=tatae"), ri.mArgs);

    // Substitute escaped characters
    proc.addSubstitution("/toto", "titi", ",", "/", conf);
    query = "titi=1%2C2%2C3";
    ri = RequestInfo(1,"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("TITI=1%2f2%2f3"));

    // Keys should be compared case-insensitively
    query = "TiTI=1%2C2%2C3";
    ri = RequestInfo(1,"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("TITI=1%2f2%2f3"));
}

void TestRequestProcessor::init()
{
    apr_initialize();
    Log::init(0);
}

void TestRequestProcessor::testFilterBasic()
{
    DupConf conf;
    conf.currentApplicationScope = ApplicationScope::ALL;

    {
        // Simple Filter MATCH
        RequestProcessor proc;
        proc.addFilter("/toto", "INFO", "[my]+", conf);
        RequestInfo ri = RequestInfo(1,"/toto", "/toto/pws/titi/", "INFO=myinfo");
        CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    }

    {
        // Simple Filter NO MATCH
        RequestProcessor proc;
        proc.addFilter("/toto", "INFO", "KIDO", conf);
        RequestInfo ri = RequestInfo(1,"/toto", "/toto/pws/titi/", "INFO=myinfo");
        CPPUNIT_ASSERT(!proc.processRequest("/toto", ri));
    }

    {
    conf.currentApplicationScope = ApplicationScope::BODY;
        // Filter applied on body only NO MATCH
        RequestProcessor proc;
        proc.addFilter("/toto", "INFO", "my", conf);
        RequestInfo ri = RequestInfo(1,"/toto", "/toto/pws/titi/", "INFO=myinfo");
        CPPUNIT_ASSERT(!proc.processRequest("/toto", ri));
    }

    {
        // Filter applied on body only MATCH
        RequestProcessor proc;
        proc.addFilter("/bb", "BODY", "hello", conf);
        std::string body = "BODY=hello";
        RequestInfo ri = RequestInfo(1,"/bb", "/bb/pws/titi/", "INFO=myinfo", &body);
        CPPUNIT_ASSERT(proc.processRequest("/bb", ri));
    }



}

void TestRequestProcessor::testFilter()
{
    RequestProcessor proc;

    // // // proc.addFilter("/toto", "INFO", "[my]+", conf);
    // // // RequestInfo ri = RequestInfo(1,"/toto", "/toto/pws/titi/", "INFO=myinfo");
    // // // CPPUNIT_ASSERT(proc.processRequest("/toto", ri));

    DupConf conf;

    std::string query;

    // No filter, so nothing should pass
    query = "titi=tata&tutu";
    RequestInfo ri = RequestInfo(1,"/toto", "/toto/pws/titi/", query);

    CPPUNIT_ASSERT(!proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(std::string("titi=tata&tutu"), ri.mArgs);

    query = "";
    ri = RequestInfo(1,"", "", query);

    CPPUNIT_ASSERT(!proc.processRequest("", ri));
    CPPUNIT_ASSERT_EQUAL(std::string(""), ri.mArgs);

    // Filter
    conf.currentApplicationScope = ApplicationScope::HEADER;
    proc.addFilter("/toto", "titi", "^ta", conf);
    query = "titi=tata&tutu";
    ri = RequestInfo(1,"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("titi=tata&tutu"));

    query = "tata&tutu";
    ri = RequestInfo(1,"/toto", "/toto", query);
    CPPUNIT_ASSERT(!proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("tata&tutu"));

    query = "tititi=tata&tutu";
    ri = RequestInfo(1,"/toto", "/toto", query);
    CPPUNIT_ASSERT(!proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("tititi=tata&tutu"));

    // Filters are case-insensitive
    query = "TITi=tata&tutu";
    ri = RequestInfo(1,"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("TITi=tata&tutu"));

    // On other paths, no filter is applied
    query = "titi=tata&tutu";
    ri = RequestInfo(1,"/to", "/to", query);
    CPPUNIT_ASSERT(!proc.processRequest("/to", ri));
    CPPUNIT_ASSERT_EQUAL(query, std::string("titi=tata&tutu"));

    query = "tata&tutu";
    ri = RequestInfo(1,"/toto", "/toto/bla", query);
    CPPUNIT_ASSERT(!proc.processRequest("/toto/bla", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("tata&tutu"));

    // Two filters on same path - either of them has to match
    conf.currentApplicationScope = ApplicationScope::HEADER;
    proc.addFilter("/toto", "titi", "[tu]{2,15}", conf);
    query = "titi=tata&tutu";
    ri = RequestInfo(1,"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("titi=tata&tutu"));

    query = "titi=tutu";
    ri = RequestInfo(1,"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("titi=tutu"));

    query = "titi=t";
    ri = RequestInfo(1,"/toto", "/toto", query);
    CPPUNIT_ASSERT(!proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("titi=t"));

    // Two filters on different paths
    proc.addFilter("/some/path", "x", "^.{3,5}$", conf);
    query = "x=1234";
    ri = RequestInfo(1,"/some/path", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/some/path", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("x=1234"));

    query = "x=123456";
    ri = RequestInfo(1,"/some/path", "/some/path", query);
    CPPUNIT_ASSERT(!proc.processRequest("/some/path", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("x=123456"));

    // New filter should not change filter on other path
    query = "titi=tutu";
    ri = RequestInfo(1,"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("titi=tutu"));

    query = "ti=tu";
    ri = RequestInfo(1,"/toto", "/toto", query);
    CPPUNIT_ASSERT(!proc.processRequest("/toto", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("ti=tu"));

    // Unknown paths still shouldn't have a filter applied
    query = "ti=tu";
    ri = RequestInfo(1,"/waaazzaaaa", "/toto", query);
    CPPUNIT_ASSERT(!proc.processRequest("/waaazzaaaa", ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("ti=tu"));

    // // Filter escaped characters
    proc.addFilter("/escaped", "y", "^ ", conf);
    query = "y=%20";
    ri = RequestInfo(1,"/escaped", "/toto", query);
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
        RequestInfo ri = RequestInfo(1,"/toto", "/toto/titi/", query, &body);
        DupConf conf;
        conf.currentApplicationScope = ApplicationScope::HEADER;

        proc.addRawFilter("/toto", ".*", conf);
        proc.addRawSubstitution("/toto", "1", "2", conf);
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
        RequestInfo ri = RequestInfo(1,"/toto", "/toto/titi/", query, &body);
        DupConf conf;
        conf.currentApplicationScope = ApplicationScope::BODY;

        proc.addRawFilter("/toto", ".*", conf);
        proc.addRawSubstitution("/toto", "1", "2", conf);
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
        RequestInfo ri = RequestInfo(1,"/toto", "/toto/titi/", query, &body);
        DupConf conf;
        conf.currentApplicationScope = ApplicationScope::ALL;

        proc.addRawFilter("/toto", ".*", conf);
        proc.addRawSubstitution("/toto", "1", "2", conf);
        CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
        CPPUNIT_ASSERT_EQUAL(std::string("arg2=myarg2"), ri.mArgs);
        CPPUNIT_ASSERT_EQUAL(std::string("mybody2test"), ri.mBody);
    }

    {
        // Different substitutions on body AND HEADER
        RequestProcessor proc;
        std::string query;
        DupConf conf;
        query = "arg1=myarg1";
        std::string body = "mybody1test";
        RequestInfo ri = RequestInfo(1,"/toto", "/toto/titi/", query, &body);

        proc.addRawFilter("/toto", ".*", conf);
        conf.currentApplicationScope = ApplicationScope::HEADER;
        proc.addRawSubstitution("/toto", "1", "2", conf);
        conf.currentApplicationScope = ApplicationScope::BODY;
        proc.addRawSubstitution("/toto", "1", "3", conf);
        CPPUNIT_ASSERT(proc.processRequest("/toto", ri));
        CPPUNIT_ASSERT_EQUAL(std::string("arg2=myarg2"), ri.mArgs);
        CPPUNIT_ASSERT_EQUAL(std::string("mybody3test"), ri.mBody);
    }

}


void TestRequestProcessor::testContextEnrichment()
{
    {
        // Simple substitution on a header
        // Body untouched
        RequestProcessor proc;
        std::string query;

        query = "joker=robin";
        std::string body = "mybody1test";
        RequestInfo ri = RequestInfo(1,"/mypath", "/mypath/wb", query, &body);
        DupConf conf;
        conf.currentApplicationScope = ApplicationScope::HEADER;

        proc.addEnrichContext("/mypath", "batman", "joker=robin", "bruce=wayne", conf);


        tRequestProcessorCommands &c = proc.mCommands["/mypath"];

        CPPUNIT_ASSERT(c.mEnrichContext.size() == 1);
        tContextEnrichment &e = c.mEnrichContext.front();
        CPPUNIT_ASSERT_EQUAL(e.mVarName, std::string("batman"));
        CPPUNIT_ASSERT_EQUAL(e.mSetValue, std::string("bruce=wayne"));
        CPPUNIT_ASSERT_EQUAL(e.mScope, ApplicationScope::HEADER);
        CPPUNIT_ASSERT_EQUAL(e.mRegex.str(), std::string("joker=robin"));

    }
}
