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
#include "TfyTestRunner.hh"

// cppunit
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>
#include <boost/shared_ptr.hpp>

CPPUNIT_TEST_SUITE_REGISTRATION( TestRequestProcessor );

using namespace DupModule;

static boost::shared_ptr<RequestInfo> POISON_REQUEST(new RequestInfo());

void TestRequestProcessor::testRun()
{
    RequestProcessor proc;
    MultiThreadQueue<boost::shared_ptr<RequestInfo> > queue;

    DupConf conf;
    conf.currentApplicationScope = ApplicationScope::ALL;
    conf.currentDupDestination = "Honolulu:8080";
    // Filter
    proc.addFilter("/toto", "INFO", "[my]+", conf, tFilter::eFilterTypes::REGULAR);

    // This request won't go anywhere, but at least we exersize the loop in proc.run()
    queue.push(boost::shared_ptr<RequestInfo>(new RequestInfo(std::string("42"),"/spp/main", "/spp/main", "SID=ID_REQ&CREDENTIAL=1,toto&")));
    queue.push(POISON_REQUEST);

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
    proc.addFilter("/toto", "INFO", "[my]+", conf, tFilter::eFilterTypes::REGULAR);
    proc.addRawFilter("/toto", "[my]+", conf, tFilter::eFilterTypes::REGULAR);
    conf.currentApplicationScope = ApplicationScope::HEADER;
    proc.addSubstitution("/toto", "INFO", "[i]", "f", conf);

    query = "titi=tata&tutu";

    RequestInfo ri = RequestInfo(std::string("42"), "/toto", "/toto/pws/titi/", "INFO=myinfo");
    CPPUNIT_ASSERT(proc.processRequest(ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("INFO=myfnfo"));
}

void TestRequestProcessor::testSubstitution()
{
    RequestProcessor proc;
    std::string query;
    DupConf conf;
    proc.addRawFilter("/toto", ".*", conf, tFilter::eFilterTypes::REGULAR);

    proc.addSubstitution("/toto", "titi", "[ae]", "-", conf);

    query = "titi=tatae&tutu=tatae";
    RequestInfo ri = RequestInfo(std::string("42"), "/toto", "/toto", query);


    CPPUNIT_ASSERT(proc.processRequest(ri));
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=t-t--&TUTU=tatae"), ri.mArgs);

    //  Empty fields are preserved
    query = "titi=tatae&tutu";
    ri = RequestInfo(std::string("42"),"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest(ri));
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=t-t--&TUTU"), ri.mArgs);

    //  Empty fields can be substituted
    proc.addSubstitution("/toto", "tutu", "^$", "titi", conf);
    query = "titi=tatae&tutu";
    ri = RequestInfo(std::string("42"),"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest(ri));
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=t-t--&TUTU=titi"), ri.mArgs);

    // Substitutions are case-sensitive
    query = "titi=TATAE&tutu=TATAE";
    ri = RequestInfo(std::string("42"),"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest(ri));
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=TATAE&TUTU=TATAE"), ri.mArgs);

    // Substitutions on the same path and field are executed in the order they are added
    proc.addSubstitution("/toto", "titi", "-(.*)-", "T\\1", conf);
    query = "titi=tatae&tutu=tatae";
    ri = RequestInfo(std::string("42"), "/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest(ri));
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=tTt-&TUTU=tatae"), ri.mArgs);

    // Substituions in other field but same path
    proc.addSubstitution("/toto", "tutu", "ata", "W", conf);
    query = "titi=tatae&tutu=tatae";
    ri = RequestInfo(std::string("42"),"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest(ri));
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=tTt-&TUTU=tWe"), ri.mArgs);

    // No Substitution on another path
    proc.addSubstitution("/x/y/z", "tutu", "ata", "W", conf);
    query = "titi=tatae&tutu=tatae";
    ri = RequestInfo(std::string("42"),"/x/y/z", "/x/y/z", query);
    CPPUNIT_ASSERT(!proc.processRequest(ri));
    CPPUNIT_ASSERT_EQUAL(std::string("titi=tatae&tutu=tatae"), ri.mArgs);

    // ... doesn't affect previous path
    query = "titi=tatae&tutu=tatae";
    ri = RequestInfo(std::string("42"),"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest( ri));
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=tTt-&TUTU=tWe"), ri.mArgs);

    // ... nor unknow path
    query = "titi=tatae&tutu=tatae";
    ri = RequestInfo(std::string("42"),"/UNKNOWN", "/UNKNOWN", query);
    CPPUNIT_ASSERT(!proc.processRequest( ri));
    CPPUNIT_ASSERT_EQUAL(std::string("titi=tatae&tutu=tatae"), ri.mArgs);

    // Substitute escaped characters
    proc.addSubstitution("/toto", "titi", ",", "/", conf);
    query = "titi=1%2C2%2C3";
    ri = RequestInfo(std::string("42"),"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest( ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("TITI=1%2f2%2f3"));

    // Keys should be compared case-insensitively
    query = "TiTI=1%2C2%2C3";
    ri = RequestInfo(std::string("42"),"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest( ri));
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
        proc.addFilter("/toto", "INFO", "[my]+", conf, tFilter::eFilterTypes::REGULAR);
        RequestInfo ri = RequestInfo(std::string("42"),"/toto", "/toto/pws/titi/", "INFO=myinfo");
        CPPUNIT_ASSERT(proc.processRequest( ri));
    }

    {
        // Simple Filter NO MATCH
        RequestProcessor proc;
        proc.addFilter("/toto", "INFO", "KIDO", conf, tFilter::eFilterTypes::REGULAR);
        RequestInfo ri = RequestInfo(std::string("42"),"/toto", "/toto/pws/titi/", "INFO=myinfo");
        CPPUNIT_ASSERT(!proc.processRequest( ri));
    }

    {
    conf.currentApplicationScope = ApplicationScope::BODY;
        // Filter applied on body only NO MATCH
        RequestProcessor proc;
        proc.addFilter("/toto", "INFO", "my", conf, tFilter::eFilterTypes::REGULAR);
        RequestInfo ri = RequestInfo(std::string("42"),"/toto", "/toto/pws/titi/", "INFO=myinfo");
        CPPUNIT_ASSERT(!proc.processRequest( ri));
    }

    {
        // Filter applied on body only MATCH
        RequestProcessor proc;
        proc.addFilter("/bb", "BODY", "hello", conf, tFilter::eFilterTypes::REGULAR);
        std::string body = "BODY=hello";
        RequestInfo ri = RequestInfo(std::string("42"),"/bb", "/bb/pws/titi/", "INFO=myinfo", &body);
        CPPUNIT_ASSERT(proc.processRequest( ri));
    }

    {
        // Filter applied on body only MATCH stopped by PREVENT filter type
        RequestProcessor proc;
        proc.addFilter("/bb", "BODY", "hello", conf, tFilter::eFilterTypes::REGULAR);
        proc.addFilter("/bb", "STOP", "true", conf, tFilter::eFilterTypes::PREVENT_DUPLICATION);
        std::string body = "BODY=hello&STOP=true";
        RequestInfo ri = RequestInfo(std::string("42"),"/bb", "/bb/pws/titi/", "INFO=myinfo", &body);
        CPPUNIT_ASSERT(!proc.processRequest( ri));
    }

}

void TestRequestProcessor::testFilterOnNotMatching()
{
    DupConf conf;
    conf.currentApplicationScope = ApplicationScope::ALL;

    {
    	// Exemple of negativ look ahead matching
        RequestProcessor proc;
        proc.addFilter("/toto","DATAS", "^(?!.*WelcomePanel)(?!.*Bmk(Video){0,1}PortailFibre)"
                       "(?!.*MobileStartCommitment)(?!.*InternetCompositeOfferIds)(?!.*FullCompositeOffer)"
                       "(?!.*AppNat(Version|SubDate|NoUnReadMails|NextEMailID|OS|ISE))",conf,
                       tFilter::eFilterTypes::REGULAR);

        RequestInfo ri = RequestInfo(std::string("42"),"/toto", "/toto/pws/titi/", "DATAS=fdlskjqdfWelcomefdsfd");
        CPPUNIT_ASSERT(proc.processRequest( ri));

        std::cout << "IN" << std::endl;
        RequestInfo ri2 = RequestInfo(std::string("42"),"/toto", "/toto/pws/titi/", "DATAS=fdlskjqdffdsfBmkPortailFibred");
        CPPUNIT_ASSERT(!proc.processRequest(ri2));

        RequestInfo ri3 = RequestInfo(std::string("42"),"/toto", "/toto/pws/titi/", "DATAS=fdlskjqdffdsfdsfqsfgsAppNatSubDateqf");
        CPPUNIT_ASSERT(!proc.processRequest(ri3));

        RequestInfo ri4 = RequestInfo(std::string("42"),"/toto", "/toto/pws/titi/", "DATAS=fdlskBmkVideoPortailFibrejqdffdsfdsfqsfgsqf");
        CPPUNIT_ASSERT(!proc.processRequest(ri4));

        RequestInfo ri5 = RequestInfo(std::string("42"),"/toto", "/toto/pws/titi/", "DATAS=fd,FullCompositeOffer,lskFibrejqdffdsfdsfqsfgsqf");
        CPPUNIT_ASSERT(!proc.processRequest(ri5));

        RequestInfo ri6 = RequestInfo(std::string("42"),"/toto", "/toto/pws/titi/", "DATAS=AdviseCapping,FullCompositeOffer,InternetBillList,InternetBillList/Bill,InternetBillList/Date,InternetInvoiceTypePay,MSISDN-SI,MobileBillList/Bill,MobileBillList/Date,MobileBillingAccount,MobileDeviceTac,MobileLoyaltyDRE,MobileLoyaltyDRO,MobileLoyaltyDebutDate,MobileLoyaltyPcmNonAnnuleDate,MobileLoyaltyPoints,MobileLoyaltySeuilPcm,MobileLoyaltyProgrammeFid,MobileStartContractDate,OOPSApplications,TlmMobileTac,TlmMode&credential=2,161232061&sid=ADVSCO&version=1.0.0&country=FR");
        CPPUNIT_ASSERT(!proc.processRequest(ri6));

        RequestInfo ri7 = RequestInfo(std::string("42"),"/toto", "/toto/pws/titi/", "REQUEST=getPNS&DATAS=AdviseCapping,FullCompositeOffer,InternetBillList,InternetBillList/Bill,InternetBillList/Date,InternetInvoiceTypePay,MSISDN-SI,MobileBillList/Bill,MobileBillList/Date,MobileBillingAccount,MobileDeviceTac,MobileLoyaltyDRE,MobileLoyaltyDRO,MobileLoyaltyDebutDate,MobileLoyaltyPcmNonAnnuleDate,MobileLoyaltyPoints,MobileLoyaltySeuilPcm,MobileLoyaltyProgrammeFid,MobileStartContractDate,OOPSApplications,TlmMobileTac,TlmMode&credential=2,161232061&sid=ADVSCO&version=1.0.0&country=FR");
        CPPUNIT_ASSERT(!proc.processRequest(ri7));
    }
}

void TestRequestProcessor::testFilter()
{
    RequestProcessor proc;

    // // // proc.addFilter("/toto", "INFO", "[my]+", conf, tFilter::eFilterTypes::REGULAR);
    // // // RequestInfo ri = RequestInfo(1,"/toto", "/toto/pws/titi/", "INFO=myinfo");
    // // // CPPUNIT_ASSERT(proc.processRequest( ri));

    DupConf conf;

    std::string query;

    // No filter, so nothing should pass
    query = "titi=tata&tutu";
    RequestInfo ri = RequestInfo(std::string("42"),"/toto", "/toto/pws/titi/", query);

    CPPUNIT_ASSERT(!proc.processRequest( ri));
    CPPUNIT_ASSERT_EQUAL(std::string("titi=tata&tutu"), ri.mArgs);

    query = "";
    ri = RequestInfo(std::string("42"),"", "", query);

    CPPUNIT_ASSERT(!proc.processRequest( ri));
    CPPUNIT_ASSERT_EQUAL(std::string(""), ri.mArgs);

    // Filter
    conf.currentApplicationScope = ApplicationScope::HEADER;
    proc.addFilter("/toto", "titi", "^ta", conf, tFilter::eFilterTypes::REGULAR);
    query = "titi=tata&tutu";
    ri = RequestInfo(std::string("42"),"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest( ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("titi=tata&tutu"));

    query = "tata&tutu";
    ri = RequestInfo(std::string("42"),"/toto", "/toto", query);
    CPPUNIT_ASSERT(!proc.processRequest( ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("tata&tutu"));

    query = "tititi=tata&tutu";
    ri = RequestInfo(std::string("42"),"/toto", "/toto", query);
    CPPUNIT_ASSERT(!proc.processRequest( ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("tititi=tata&tutu"));

    // Filters are case-insensitive
    query = "TITi=tata&tutu";
    ri = RequestInfo(std::string("42"),"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest( ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("TITi=tata&tutu"));

    // On other paths, no filter is applied
    query = "titi=tata&tutu";
    ri = RequestInfo(std::string("42"),"/to", "/to", query);
    CPPUNIT_ASSERT(!proc.processRequest( ri));
    CPPUNIT_ASSERT_EQUAL(query, std::string("titi=tata&tutu"));

    query = "tata&tutu";
    ri = RequestInfo(std::string("42"),"/toto", "/toto/bla", query);
    CPPUNIT_ASSERT(!proc.processRequest( ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("tata&tutu"));

    // Two filters on same path - either of them has to match
    conf.currentApplicationScope = ApplicationScope::HEADER;
    proc.addFilter("/toto", "titi", "[tu]{2,15}", conf, tFilter::eFilterTypes::REGULAR);
    query = "titi=tata&tutu";
    ri = RequestInfo(std::string("42"),"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest( ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("titi=tata&tutu"));

    query = "titi=tutu";
    ri = RequestInfo(std::string("42"),"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest( ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("titi=tutu"));

    query = "titi=t";
    ri = RequestInfo(std::string("42"),"/toto", "/toto", query);
    CPPUNIT_ASSERT(!proc.processRequest( ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("titi=t"));

    // Two filters on different paths
    proc.addFilter("/some/path", "x", "^.{3,5}$", conf, tFilter::eFilterTypes::REGULAR);
    query = "x=1234";
    ri = RequestInfo(std::string("42"),"/some/path", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest( ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("x=1234"));

    query = "x=123456";
    ri = RequestInfo(std::string("42"),"/some/path", "/some/path", query);
    CPPUNIT_ASSERT(!proc.processRequest( ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("x=123456"));

    // New filter should not change filter on other path
    query = "titi=tutu";
    ri = RequestInfo(std::string("42"),"/toto", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest( ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("titi=tutu"));

    query = "ti=tu";
    ri = RequestInfo(std::string("42"),"/toto", "/toto", query);
    CPPUNIT_ASSERT(!proc.processRequest( ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("ti=tu"));

    // Unknown paths still shouldn't have a filter applied
    query = "ti=tu";
    ri = RequestInfo(std::string("42"),"/waaazzaaaa", "/toto", query);
    CPPUNIT_ASSERT(!proc.processRequest( ri));
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("ti=tu"));

    // // Filter escaped characters
    proc.addFilter("/escaped", "y", "^ ", conf, tFilter::eFilterTypes::REGULAR);
    query = "y=%20";
    ri = RequestInfo(std::string("42"),"/escaped", "/toto", query);
    CPPUNIT_ASSERT(proc.processRequest( ri));
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
        RequestInfo ri = RequestInfo(std::string("42"),"/toto", "/toto/titi/", query, &body);
        DupConf conf;
        conf.currentApplicationScope = ApplicationScope::HEADER;

        proc.addRawFilter("/toto", ".*", conf, tFilter::eFilterTypes::REGULAR);
        proc.addRawSubstitution("/toto", "1", "2", conf);
        CPPUNIT_ASSERT(proc.processRequest( ri));
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
        RequestInfo ri = RequestInfo(std::string("42"),"/toto", "/toto/titi/", query, &body);
        DupConf conf;
        conf.currentApplicationScope = ApplicationScope::BODY;

        proc.addRawFilter("/toto", ".*", conf, tFilter::eFilterTypes::REGULAR);
        proc.addRawSubstitution("/toto", "1", "2", conf);
        CPPUNIT_ASSERT(proc.processRequest( ri));
        CPPUNIT_ASSERT_EQUAL(query, ri.mArgs);
        CPPUNIT_ASSERT_EQUAL(std::string("mybody2test"), ri.mBody);
    }

    {
        // Simple substitution on body AND HEADER
        RequestProcessor proc;
        std::string query;

        query = "arg1=myarg1";
        std::string body = "mybody1test";
        RequestInfo ri = RequestInfo(std::string("42"),"/toto", "/toto/titi/", query, &body);
        DupConf conf;
        conf.currentApplicationScope = ApplicationScope::ALL;

        proc.addRawFilter("/toto", ".*", conf, tFilter::eFilterTypes::REGULAR);
        proc.addRawSubstitution("/toto", "1", "2", conf);
        CPPUNIT_ASSERT(proc.processRequest( ri));
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
        RequestInfo ri = RequestInfo(std::string("42"),"/toto", "/toto/titi/", query, &body);

        proc.addRawFilter("/toto", ".*", conf, tFilter::eFilterTypes::REGULAR);
        conf.currentApplicationScope = ApplicationScope::HEADER;
        proc.addRawSubstitution("/toto", "1", "2", conf);
        conf.currentApplicationScope = ApplicationScope::BODY;
        proc.addRawSubstitution("/toto", "1", "3", conf);
        CPPUNIT_ASSERT(proc.processRequest(ri));
        CPPUNIT_ASSERT_EQUAL(std::string("arg2=myarg2"), ri.mArgs);
        CPPUNIT_ASSERT_EQUAL(std::string("mybody3test"), ri.mBody);
    }

}


void TestRequestProcessor::testContextEnrichment()
{
    // Simple substitution on a header
    // Body untouched
    RequestProcessor proc;
    std::string query;

    query = "joker=robin";
    std::string body = "mybody1test";
    RequestInfo ri = RequestInfo(std::string("42"),"/mypath", "/mypath/wb", query, &body);
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

void TestRequestProcessor::testDupFormat() {

    // sendDupFormat test
    RequestProcessor proc;
    std::string query = "theBig=Lebowski";
    std::string body = "mybody1test";
    RequestInfo ri = RequestInfo(std::string("42"), "/mypath", "/mypath/wb", query, &body);
    CURL * curl = curl_easy_init();
    struct curl_slist *slist = NULL;

    // Just the request body, no answer header or answer body
    std::string *df = proc.sendDupFormat(curl, ri, slist);
    CPPUNIT_ASSERT_EQUAL(std::string("00000011mybody1test0000000000000000"),
                         *df);
    delete df;

    // Request body, + answer header
    ri.mHeadersOut.push_back(std::make_pair(std::string("key"), std::string("val")));
    df = proc.sendDupFormat(curl, ri, slist);
    CPPUNIT_ASSERT_EQUAL(std::string("00000011mybody1test00000009key: val\n00000000"),
                         *df);

    // Request body, + answer header + answer body
    ri.mAnswer = "TheAnswerBody";
    df = proc.sendDupFormat(curl, ri, slist);
    CPPUNIT_ASSERT_EQUAL(std::string("00000011mybody1test00000009key: val\n00000013TheAnswerBody"),
                         *df);

    delete df;

}

void TestRequestProcessor::testRequestInfo() {
    RequestInfo ri = RequestInfo(std::string("42"), "/path", "/path", "arg1=value1");
    CPPUNIT_ASSERT(!ri.hasBody());
    ri.mBody = "sdf";
    CPPUNIT_ASSERT(ri.hasBody());
}

void TestRequestProcessor::testTimeout() {
    RequestProcessor proc;
    MultiThreadQueue<boost::shared_ptr<RequestInfo> > queue;

    DupConf conf;
    conf.currentApplicationScope = ApplicationScope::ALL;
    conf.currentDupDestination = "Honolulu:8080";
    proc.addFilter("/spp/main", "SID", "mySid", conf, tFilter::eFilterTypes::REGULAR);

    queue.push(boost::shared_ptr<RequestInfo>(new RequestInfo(std::string("42"),"/spp/main", "/spp/main", "SID=mySid")));
    queue.push(POISON_REQUEST);
    proc.run(queue);

    CPPUNIT_ASSERT_EQUAL((unsigned int)1, proc.getDuplicatedCount());
}

void TestRequestProcessor::testKeySubstitutionOnBody()
{
    RequestProcessor proc;
    std::string query;
    DupConf conf;
    conf.currentApplicationScope =  ApplicationScope::BODY;
    proc.addRawFilter("/toto", ".*", conf, tFilter::eFilterTypes::REGULAR);
    proc.addSubstitution("/toto", "titi", "value", "replacedValue", conf);

    query = "titi=value&tutu=tatae";
    RequestInfo ri = RequestInfo(std::string("42"), "/toto", "/toto", query);
    ri.mBody = "key1=what??&titi=value";

    CPPUNIT_ASSERT(proc.processRequest(ri));
    CPPUNIT_ASSERT_EQUAL(std::string("titi=value&tutu=tatae"), ri.mArgs);
    CPPUNIT_ASSERT_EQUAL(std::string("KEY1=what%3f%3f&TITI=replacedValue"), ri.mBody);
}

#ifdef UNIT_TESTING
//--------------------------------------
// the main method
//--------------------------------------
int main(int argc, char* argv[])
{
    Log::init();
    apr_initialize();

    TfyTestRunner runner(argv[0]);
    runner.addTest(CppUnit::TestFactoryRegistry::getRegistry().makeTest());
    bool failed = runner.run();

    return !failed;
}
#endif
