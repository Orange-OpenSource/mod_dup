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
#include "CurlStubs.cc"

// cppunit
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>
#include <boost/shared_ptr.hpp>
#include <curl/curl.h>

CPPUNIT_TEST_SUITE_REGISTRATION( TestRequestProcessor );

using namespace DupModule;

static boost::shared_ptr<RequestInfo> POISON_REQUEST(new RequestInfo());


static int curlTrace(CURL *handle, curl_infotype type, char *data, size_t size, void *userp)
{
  (void)handle;
  char* inpData = nullptr;

  if (type == CURLINFO_HEADER_OUT)
  {
      inpData = (char*)malloc(sizeof(char)*size+1);
      memcpy (inpData,data,size);
      inpData[size]='\0';
  }
  else if(type == CURLINFO_DATA_OUT)
  {
      inpData = (char*)malloc(sizeof(char)*size+1);
      memcpy (inpData,data,size);
      inpData[size]='\0';
  }

  if(inpData)
  {
      std::string inpDataString(inpData);
      std::string* inpPointer = reinterpret_cast<std::string*> (userp);
      inpPointer->append(inpDataString);
      if (inpDataString.find("X-DUPLICATED-REQUEST") != std::string::npos)
          Log::debug("DEBUG MANU OK");
      free(inpData);
  }

  return 0;
}

void TestRequestProcessor::testToDuplicate()
{
    Commands c;
    c.mDuplicationPercentage = 100;
    CPPUNIT_ASSERT(!c.toDuplicate());
    c.mDuplicationPercentage = 500;
    CPPUNIT_ASSERT(!c.toDuplicate());
    c.mDuplicationPercentage = 501;
    if ( c.toDuplicate() ) { // should happen only 1% of the time
        CPPUNIT_ASSERT(!c.toDuplicate()); // should be ok except 1/10000 times
    }
    c.mDuplicationPercentage = 599;
    if ( ! c.toDuplicate() ) { // should happen only 1% of the time   
        CPPUNIT_ASSERT(c.toDuplicate());
    }
    
    
}

void TestRequestProcessor::testToDuplicateInt()
{
    Commands c;
    c.mDuplicationPercentage = 100;
    CPPUNIT_ASSERT_EQUAL(1U, c.toDuplicateInt());
    c.mDuplicationPercentage = 200;
    CPPUNIT_ASSERT_EQUAL(2U, c.toDuplicateInt());
    c.mDuplicationPercentage = 500;
    CPPUNIT_ASSERT_EQUAL(5U, c.toDuplicateInt());
    int sum = 0;
    c.mDuplicationPercentage = 550;
    for (int i = 0; i < 100; ++i) {
        sum += c.toDuplicateInt();
    }
    CPPUNIT_ASSERT(sum > 530);
    CPPUNIT_ASSERT(sum < 570);
    
}


void TestRequestProcessor::testRun()
{
    RequestProcessor proc;
    MultiThreadQueue<boost::shared_ptr<RequestInfo> > queue;

    DupConf conf;
    conf.currentApplicationScope = ApplicationScope::ALL;
    conf.currentDupDestination = "Honolulu:8080";
    proc.setDestinationDuplicationPercentage(conf, conf.currentDupDestination, 300);

    // This request should be run 3 times (300%) for each push, so 6 times
    proc.addRawFilter("SID>(.*)<", conf, tFilter::eFilterTypes::REGULAR);
    boost::shared_ptr<RequestInfo> ri(new RequestInfo("42","/toto", "/toto/pws/titi/", "<SID>ID-REQ</SID>"));
    ri->mConf = &conf;
    queue.push(ri);
    queue.push(ri);
    queue.push(POISON_REQUEST);

    // If the poison pill would not work, this call would hang forever
    proc.run(queue);
    
    volatile unsigned int val = 6U;
    CPPUNIT_ASSERT_EQUAL(val,proc.mDuplicatedCount);

    // make sure we can restart the queue and requests are sent
    queue.push(ri);
    queue.push(POISON_REQUEST);
    // If the poison pill would not work, this call would hang forever
    proc.run(queue);
    val = 9U;
    CPPUNIT_ASSERT_EQUAL(val,proc.mDuplicatedCount);
    
    // Stop the queue before run and check that no additionnal request is sent (fast exit)
    queue.push(ri);
    queue.push(POISON_REQUEST);
    queue.stop();
    proc.run(queue);
    CPPUNIT_ASSERT_EQUAL(val,proc.mDuplicatedCount);
    // We could hack a web server with nc to test the rest of this method,
    // but this might be overkill for a unit test
}

void TestRequestProcessor::testSubstitution()
{
    RequestProcessor proc;
    std::string query;
    DupConf conf;
    conf.currentDupDestination = "Honolulu:8080";

    proc.addRawFilter(".*", conf, tFilter::eFilterTypes::REGULAR);

    conf.currentApplicationScope = ApplicationScope::QUERY_STRING;
    proc.addSubstitution("titi", "[ae]", "-", conf);
    conf.currentApplicationScope = ApplicationScope::HEADERS;
    proc.addSubstitution("H1", "[Aa]", "*", conf);
    conf.currentApplicationScope = ApplicationScope::QUERY_STRING;
    
    query = "titi=tatae&tutu=tatae";
    RequestInfo ri = RequestInfo("42", "/toto", "/toto", query);
    ri.mConf = &conf;
    proc.parseArgs(ri.mParsedArgs, ri.mArgs);
    RequestProcessor::tCommandsByDestination &cbd = proc.mCommands.at(ri.mConf);
    Commands &c = cbd.at(conf.currentDupDestination);
    proc.substituteRequest(ri, c);
    CPPUNIT_ASSERT_EQUAL(std::string("TITI=t-t--&TUTU=tatae"), ri.mArgs);

    {
        //  Empty fields are preserved
        query = "titi=tatae&tutu";
        ri = RequestInfo("42","/toto", "/toto", query);
        ri.mConf = &conf;
        proc.parseArgs(ri.mParsedArgs, ri.mArgs);
        RequestProcessor::tCommandsByDestination &cbd = proc.mCommands.at(&conf);
        Commands &c = cbd.at(conf.currentDupDestination);
        proc.substituteRequest(ri, c);
        CPPUNIT_ASSERT_EQUAL(std::string("TITI=t-t--&TUTU"), ri.mArgs);
    }

    {
        //  Header substitution
        query = "titi=tatae&tutu";
        ri = RequestInfo("42","/toto", "/toto", query);
        ri.mConf = &conf;
        ri.mHeadersIn.push_back(tKeyVal(std::string("H1"), std::string("tAta1,2#")));
        ri.mHeadersIn.push_back(tKeyVal(std::string("H2"), std::string(""))); ;
        proc.parseArgs(ri.mParsedArgs, ri.mArgs);
        RequestProcessor::tCommandsByDestination &cbd = proc.mCommands.at(&conf);
        Commands &c = cbd.at(conf.currentDupDestination);
        proc.substituteRequest(ri, c);
        CPPUNIT_ASSERT_EQUAL(ri.mHeadersIn.front().first, std::string("H1"));
        CPPUNIT_ASSERT_EQUAL(ri.mHeadersIn.front().second, std::string("t*t*1,2#"));
        ri.mHeadersIn.pop_front();
        CPPUNIT_ASSERT_EQUAL(ri.mHeadersIn.front().first, std::string("H2"));
        CPPUNIT_ASSERT_EQUAL(ri.mHeadersIn.front().second, std::string(""));
        ri.mHeadersIn.pop_front();
        CPPUNIT_ASSERT_EQUAL(std::string("TITI=t-t--&TUTU"), ri.mArgs);
    }

    {
        //  Empty fields can be substituted
        proc.addSubstitution("tutu", "^$", "titi", conf);
        query = "titi=tatae&tutu";
        ri = RequestInfo("42","/toto", "/toto", query);
        ri.mConf = &conf;
        proc.parseArgs(ri.mParsedArgs, ri.mArgs);
        RequestProcessor::tCommandsByDestination &cbd = proc.mCommands.at(&conf);
        Commands &c = cbd.at(conf.currentDupDestination);
        proc.substituteRequest(ri, c);
        CPPUNIT_ASSERT_EQUAL(std::string("TITI=t-t--&TUTU=titi"), ri.mArgs);
    }

    {
        // Substitutions are case-sensitive
        query = "titi=TATAE&tutu=TATAE";
        ri = RequestInfo("42","/toto", "/toto", query);
        ri.mConf = &conf;
        proc.parseArgs(ri.mParsedArgs, ri.mArgs);
        RequestProcessor::tCommandsByDestination &cbd = proc.mCommands.at(&conf);
        Commands &c = cbd.at(conf.currentDupDestination);
        proc.substituteRequest(ri, c);
        CPPUNIT_ASSERT_EQUAL(std::string("TITI=TATAE&TUTU=TATAE"), ri.mArgs);
    }

    {
        // Substitutions on the same path and field are executed in the order they are added
        proc.addSubstitution("titi", "-(.*)-", "T\\1", conf);
        query = "titi=tatae&tutu=tatae";
        ri = RequestInfo("42", "/toto", "/toto", query);
        ri.mConf = &conf;
        proc.parseArgs(ri.mParsedArgs, ri.mArgs);
        RequestProcessor::tCommandsByDestination &cbd = proc.mCommands.at(&conf);
        Commands &c = cbd.at(conf.currentDupDestination);
        proc.substituteRequest(ri, c);
        CPPUNIT_ASSERT_EQUAL(std::string("TITI=tTt-&TUTU=tatae"), ri.mArgs);
    }

    {
        // Substituions in other field but same path
        proc.addSubstitution("tutu", "ata", "W", conf);
        query = "titi=tatae&tutu=tatae";
        ri = RequestInfo("42","/toto", "/toto", query);
        ri.mConf = &conf;
        proc.parseArgs(ri.mParsedArgs, ri.mArgs);
        RequestProcessor::tCommandsByDestination &cbd = proc.mCommands.at(ri.mConf);
        Commands &c = cbd.at(conf.currentDupDestination);
        proc.substituteRequest(ri, c);
        CPPUNIT_ASSERT_EQUAL(std::string("TITI=tTt-&TUTU=tWe"), ri.mArgs);
    }

    {
        // ... doesn't affect previous path
        query = "titi=tatae&tutu=tatae";
        ri = RequestInfo("42","/toto", "/toto", query);
        ri.mConf = &conf;
        proc.parseArgs(ri.mParsedArgs, ri.mArgs);
        RequestProcessor::tCommandsByDestination &cbd = proc.mCommands.at(&conf);
        Commands &c = cbd.at(conf.currentDupDestination);
        proc.substituteRequest(ri, c);
        CPPUNIT_ASSERT_EQUAL(std::string("TITI=tTt-&TUTU=tWe"), ri.mArgs);
    }

    {
        // Substitute escaped characters
        proc.addSubstitution( "titi", ",", "/", conf);
        query = "titi=1%2C2%2C3";
        ri = RequestInfo("42","/toto", "/toto", query);
        ri.mConf = &conf;
        proc.parseArgs(ri.mParsedArgs, ri.mArgs);
        RequestProcessor::tCommandsByDestination &cbd = proc.mCommands.at(&conf);
        Commands &c = cbd.at(conf.currentDupDestination);
        proc.substituteRequest(ri, c);
        CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("TITI=1%2f2%2f3"));
    }

    {
        // Keys should be compared case-insensitively
        query = "TiTI=1%2C2%2C3";
        ri = RequestInfo("42","/toto", "/toto", query);
        ri.mConf = &conf;
        proc.parseArgs(ri.mParsedArgs, ri.mArgs);
        RequestProcessor::tCommandsByDestination &cbd = proc.mCommands.at(&conf);
        Commands &c = cbd.at(conf.currentDupDestination);
        proc.substituteRequest(ri, c);
        CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("TITI=1%2f2%2f3"));
    }
}

void TestRequestProcessor::testFilterBasic()
{
    DupConf conf;
    conf.currentApplicationScope = ApplicationScope::ALL;
    conf.currentDupDestination = "Honolulu:8080";

    {
        // Simple Filter MATCH
        RequestProcessor proc;
        proc.addFilter( "INFO", "[my]+", conf, tFilter::eFilterTypes::REGULAR);
        RequestInfo ri = RequestInfo("42","/toto", "/toto/pws/titi/", "INFO=myinfo");
        ri.mConf = &conf;
        proc.parseArgs(ri.mParsedArgs, ri.mArgs);
        CPPUNIT_ASSERT(!proc.processRequest(ri).empty());
    }

    {
        // Raw Filter MATCH
        RequestProcessor proc;
        proc.addRawFilter("SID>(.*)<", conf, tFilter::eFilterTypes::REGULAR);
        RequestInfo ri = RequestInfo("42","/toto", "/toto/pws/titi/", "<SID>ID-REQ</SID>");
        ri.mConf = &conf;
        proc.parseArgs(ri.mParsedArgs, ri.mArgs);
        CPPUNIT_ASSERT(!proc.processRequest(ri).empty());
    }

    {
        // Simple Filter NO MATCH
        RequestProcessor proc;
        proc.addFilter("INFO", "KIDO", conf, tFilter::eFilterTypes::REGULAR);
        RequestInfo ri = RequestInfo("42","/toto", "/toto/pws/titi/", "INFO=myinfo");
        ri.mConf = &conf;
        proc.parseArgs(ri.mParsedArgs, ri.mArgs);
        CPPUNIT_ASSERT(proc.processRequest(ri).empty());
    }

    {
        conf.currentApplicationScope = ApplicationScope::BODY;
        // Filter applied on body only NO MATCH
        RequestProcessor proc;
        proc.addFilter("INFO", "my", conf, tFilter::eFilterTypes::REGULAR);
        RequestInfo ri = RequestInfo("42","/toto", "/toto/pws/titi/", "INFO=myinfo");
        ri.mConf = &conf;
        proc.parseArgs(ri.mParsedArgs, ri.mArgs);
        CPPUNIT_ASSERT(proc.processRequest(ri).empty());
    }

    {
        // Filter applied on body only MATCH
        RequestProcessor proc;
        proc.addFilter("BODY", "hello", conf, tFilter::eFilterTypes::REGULAR);
        std::string body = "BODY=hello";
        RequestInfo ri = RequestInfo("42","/bb", "/bb/pws/titi/", "INFO=myinfo", &body);
        ri.mConf = &conf;
        proc.parseArgs(ri.mParsedArgs, ri.mArgs);
        CPPUNIT_ASSERT(!proc.processRequest(ri).empty());
    }

    {
        // Filter applied on body only MATCH stopped by PREVENT filter type
        RequestProcessor proc;
        proc.addFilter( "BODY", "hello", conf, tFilter::eFilterTypes::REGULAR);
        proc.addFilter( "STOP", "true", conf, tFilter::eFilterTypes::PREVENT_DUPLICATION);
        std::string body = "BODY=hello&STOP=true";
        RequestInfo ri = RequestInfo("42","/bb", "/bb/pws/titi/", "INFO=myinfo", &body);
        ri.mConf = &conf;
        proc.parseArgs(ri.mParsedArgs, ri.mArgs);
        CPPUNIT_ASSERT(proc.processRequest(ri).empty());
    }

    {
        // Filter applied on body only MATCH stopped by RAW PREVENT filter type on body
        RequestProcessor proc;
        proc.addFilter( "BODY", "hello", conf, tFilter::eFilterTypes::REGULAR);
        proc.addRawFilter( "STOP=true", conf, tFilter::eFilterTypes::PREVENT_DUPLICATION);
        std::string body = "BODY=hello&STOP=true";
        RequestInfo ri = RequestInfo("42","/bb", "/bb/pws/titi/", "INFO=myinfo", &body);
        ri.mConf = &conf;
        proc.parseArgs(ri.mParsedArgs, ri.mArgs);
        CPPUNIT_ASSERT(proc.processRequest(ri).empty());
    }

    {
        // MATCH on body stopped by RAW PREVENT filter type on header
        conf.currentApplicationScope = ApplicationScope::ALL;
        RequestProcessor proc;
        proc.addFilter( "BODY", "hello", conf, tFilter::eFilterTypes::REGULAR);
        proc.addRawFilter( "STOP=true", conf, tFilter::eFilterTypes::PREVENT_DUPLICATION);
        std::string body = "BODY=hello";
        RequestInfo ri = RequestInfo("42","/bb", "/bb/pws/titi/", "STOP=true", &body);
        ri.mConf = &conf;
        proc.parseArgs(ri.mParsedArgs, ri.mArgs);
        CPPUNIT_ASSERT(proc.processRequest(ri).empty());
    }

}


#define MAKE_REQ_INFO(id, pConfPath, pPath, pArgs, conf, proc) \
RequestInfo ri = RequestInfo(id, pConfPath, pPath, pArgs);\
ri.mConf = &conf;\
proc.parseArgs(ri.mParsedArgs, ri.mArgs);

#define MAKE_REQ_INFO_BODY(id, pConfPath, pPath, pArgs, conf, proc, body) \
RequestInfo ri = RequestInfo(id, pConfPath, pPath, pArgs, body);\
ri.mConf = &conf;\
proc.parseArgs(ri.mParsedArgs, ri.mArgs);


RequestInfo makeReqInfo(std::string id, const std::string &pConfPath, const std::string &pPath,
                        const std::string &pArgs, DupConf *conf, RequestProcessor &proc, const std::string *body = 0) {
    
    RequestInfo ri = RequestInfo(id, pConfPath, pPath, pArgs, body);
    ri.mConf = &conf;
    proc.parseArgs(ri.mParsedArgs, ri.mArgs);
    return ri;
}

void TestRequestProcessor::testFilterOnNotMatching()
{
    DupConf conf;
    conf.currentApplicationScope = ApplicationScope::ALL;
    conf.currentDupDestination = "Honolulu:8080";

    
        // Exemple of negativ look ahead matching
        RequestProcessor proc;
        proc.addFilter("DATAS", "^(?!.*WelcomePanel)(?!.*Bmk(Video){0,1}PortailFibre)"
                       "(?!.*MobileStartCommitment)(?!.*InternetCompositeOfferIds)(?!.*FullCompositeOffer)"
                       "(?!.*AppNat(Version|SubDate|NoUnReadMails|NextEMailID|OS|ISE))",conf,
                       tFilter::eFilterTypes::REGULAR);
        { // should match, not found, not empty
        MAKE_REQ_INFO("42","/toto", "/toto/pws/titi/", "DATAS=fdlskjqdfWelcomefdsfd", conf, proc);
        CPPUNIT_ASSERT(!proc.processRequest(ri).empty());
        }
    {
        MAKE_REQ_INFO("42","/toto", "/toto/pws/titi/", "DATAS=fdlskjqdffdsfBmkPortailFibred", conf, proc);
        CPPUNIT_ASSERT(proc.processRequest(ri).empty());
    }
    {
        MAKE_REQ_INFO("42","/toto", "/toto/pws/titi/", "DATAS=fdlskjqdffdsfdsfqsfgsAppNatSubDateqf", conf, proc);
        CPPUNIT_ASSERT(proc.processRequest(ri).empty());
    }{
        MAKE_REQ_INFO("42","/toto", "/toto/pws/titi/", "DATAS=fdlskBmkVideoPortailFibrejqdffdsfdsfqsfgsqf", conf, proc);
         CPPUNIT_ASSERT(proc.processRequest(ri).empty());
    }{

        MAKE_REQ_INFO("42","/toto", "/toto/pws/titi/", "DATAS=fd,FullCompositeOffer,lskFibrejqdffdsfdsfqsfgsqf", conf, proc);
        CPPUNIT_ASSERT(proc.processRequest(ri).empty());
    }{
        MAKE_REQ_INFO("42","/toto", "/toto/pws/titi/", "DATAS=AdviseCapping,FullCompositeOffer,InternetBillList,InternetBillList/Bill,InternetBillList/Date,InternetInvoiceTypePay,MSISDN-SI,MobileBillList/Bill,MobileBillList/Date,MobileBillingAccount,MobileDeviceTac,MobileLoyaltyDRE,MobileLoyaltyDRO,MobileLoyaltyDebutDate,MobileLoyaltyPcmNonAnnuleDate,MobileLoyaltyPoints,MobileLoyaltySeuilPcm,MobileLoyaltyProgrammeFid,MobileStartContractDate,OOPSApplications,TlmMobileTac,TlmMode&credential=2,161232061&sid=ADVSCO&version=1.0.0&country=FR", conf, proc);
        CPPUNIT_ASSERT(proc.processRequest(ri).empty());
    }{
        MAKE_REQ_INFO("42","/toto", "/toto/pws/titi/", "REQUEST=getPNS&DATAS=AdviseCapping,FullCompositeOffer,InternetBillList,InternetBillList/Bill,InternetBillList/Date,InternetInvoiceTypePay,MSISDN-SI,MobileBillList/Bill,MobileBillList/Date,MobileBillingAccount,MobileDeviceTac,MobileLoyaltyDRE,MobileLoyaltyDRO,MobileLoyaltyDebutDate,MobileLoyaltyPcmNonAnnuleDate,MobileLoyaltyPoints,MobileLoyaltySeuilPcm,MobileLoyaltyProgrammeFid,MobileStartContractDate,OOPSApplications,TlmMobileTac,TlmMode&credential=2,161232061&sid=ADVSCO&version=1.0.0&country=FR", conf, proc);
        CPPUNIT_ASSERT(proc.processRequest(ri).empty());
    }
     
}

void TestRequestProcessor::testFilter()
{
    RequestProcessor proc;

    DupConf conf;
    conf.currentApplicationScope = ApplicationScope::ALL;
    conf.currentDupDestination = "Honolulu:8080";

    std::string query;

    {
    // No filter, so nothing should pass
    query = "titi=tata&tutu";
    MAKE_REQ_INFO("42","/toto", "/toto/pws/titi/", query, conf, proc);
    CPPUNIT_ASSERT(proc.processRequest(ri).empty());
    CPPUNIT_ASSERT_EQUAL(std::string("titi=tata&tutu"), ri.mArgs);
    } {
    query = "";
    MAKE_REQ_INFO("42","", "", query, conf, proc);
    CPPUNIT_ASSERT(proc.processRequest(ri).empty());
    }{
    // Filter
    conf.currentApplicationScope = ApplicationScope::QUERY_STRING;
    proc.addFilter("titi", "^ta", conf, tFilter::eFilterTypes::REGULAR);
    query = "titi=tata&tutu";
    MAKE_REQ_INFO("42","/toto", "/toto", query, conf, proc);
    CPPUNIT_ASSERT(!proc.processRequest(ri).empty());
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("titi=tata&tutu"));
    }{
    query = "tata&tutu";
    MAKE_REQ_INFO("42","/toto", "/toto", query, conf, proc);
    CPPUNIT_ASSERT(proc.processRequest(ri).empty());
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("tata&tutu"));
    }{
    query = "tititi=tata&tutu";
    MAKE_REQ_INFO("42","/toto", "/toto", query, conf, proc);
    CPPUNIT_ASSERT(proc.processRequest(ri).empty());
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("tititi=tata&tutu"));
    }{
    // Filters are case-insensitive
    query = "TITi=tata&tutu";
    MAKE_REQ_INFO("42","/toto", "/toto", query, conf, proc);
    CPPUNIT_ASSERT(!proc.processRequest(ri).empty());
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("TITi=tata&tutu"));
    }
    DupConf conf2;
    {
    // On other paths, no filter is applied
    query = "titi=tata&tutu";
    MAKE_REQ_INFO("42","/to", "/to", query, conf2, proc);
     CPPUNIT_ASSERT(proc.processRequest(ri).empty());
    CPPUNIT_ASSERT_EQUAL(query, std::string("titi=tata&tutu"));
    }{
    query = "tata&tutu";
    MAKE_REQ_INFO("42","/toto", "/toto/bla", query, conf, proc);
    CPPUNIT_ASSERT(proc.processRequest(ri).empty());
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("tata&tutu"));
    }{
    // Two filters on same path - either of them has to match
    conf.currentApplicationScope = ApplicationScope::QUERY_STRING;
    proc.addFilter("titi", "[tu]{2,15}", conf, tFilter::eFilterTypes::REGULAR);
    query = "titi=tata&tutu";
    MAKE_REQ_INFO("42","/toto", "/toto", query, conf, proc);
    CPPUNIT_ASSERT(!proc.processRequest(ri).empty());
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("titi=tata&tutu"));
    }{
    query = "titi=tutu";
    MAKE_REQ_INFO("42","/toto", "/toto", query, conf, proc);
    CPPUNIT_ASSERT(!proc.processRequest(ri).empty());
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("titi=tutu"));
    }{
    query = "titi=t";
    MAKE_REQ_INFO("42","/toto", "/toto", query, conf, proc);
    CPPUNIT_ASSERT(proc.processRequest(ri).empty());
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("titi=t"));
    }{
    // Two filters on different paths matches
    proc.addFilter("x", "^.{3,5}$", conf2, tFilter::eFilterTypes::REGULAR);
    query = "x=1234";
    MAKE_REQ_INFO("42","/some/path", "/toto", query, conf2, proc);
    CPPUNIT_ASSERT(!proc.processRequest(ri).empty());
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("x=1234"));
    }{// Two filters on different paths does not match
    query = "x=123456";
    MAKE_REQ_INFO("42","/some/path", "/some/path", query, conf2, proc);
    CPPUNIT_ASSERT(proc.processRequest(ri).empty());
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("x=123456"));
    }{
    // New filter should not change filter on other path
    query = "titi=tutu";
    MAKE_REQ_INFO("42","/toto", "/toto", query, conf, proc);
    CPPUNIT_ASSERT(!proc.processRequest(ri).empty());
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("titi=tutu"));
    }{
    query = "ti=tu";
    MAKE_REQ_INFO("42","/toto", "/toto", query, conf, proc);
    CPPUNIT_ASSERT(proc.processRequest(ri).empty());
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("ti=tu"));
    }{
    // Unknown paths still shouldn't have a filter applied
    query = "ti=tu";
    DupConf conf3;
    MAKE_REQ_INFO("42","/waaazzaaaa", "/toto", query, conf3, proc);
    CPPUNIT_ASSERT(proc.processRequest(ri).empty());
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("ti=tu"));
    }{
    // Filter escaped characters
    proc.addFilter( "y", "^ ", conf, tFilter::eFilterTypes::REGULAR);
    query = "y=%20";
    MAKE_REQ_INFO("42","/escaped", "/toto", query, conf, proc);
    CPPUNIT_ASSERT(!proc.processRequest(ri).empty());
    CPPUNIT_ASSERT_EQUAL(ri.mArgs, std::string("y=%20"));
    }
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


void TestRequestProcessor::testAddValidationHeaders()
{
    RequestProcessor proc;
    RequestInfo lInfo;
    {
        tFilter matchedFilter("papalino", ApplicationScope::ALL, "Alger", DuplicationType::COMPLETE_REQUEST, boost::regex("nomatch"));
        matchedFilter.mDestination = "Alger";
        matchedFilter.mDuplicationType = DuplicationType::COMPLETE_REQUEST;
        matchedFilter.mMatch = "papalino";
        matchedFilter.mScope = ApplicationScope::ALL;
        lInfo.mHeadersIn.push_back(tKeyVal(std::string("X_DUP_LOG"), std::string("ON")));
        std::list<const tFilter *> matchedFilters;
        matchedFilters.push_back(&matchedFilter);
        proc.addValidationHeadersDup(lInfo, matchedFilters,0,0);
        CPPUNIT_ASSERT_EQUAL(std::string("X-MATCHED-PATTERN"), lInfo.mHeadersOut.front().first);
        CPPUNIT_ASSERT_EQUAL(std::string("papalino"), lInfo.mHeadersOut.front().second);
        lInfo.mHeadersOut.pop_front();
        CPPUNIT_ASSERT_EQUAL(std::string("The request is duplicated, ALL filter: \"papalino\" matched: \"papalino\" Destination: Alger"), lInfo.mHeadersOut.front().second);
    }
    
    lInfo.mHeadersOut.pop_front();
    {
        tFilter matchedFilter(".*", ApplicationScope::ALL, "Alger", DuplicationType::COMPLETE_REQUEST, boost::regex("nomatch"));
        matchedFilter.mMatch = "papalino";
        matchedFilter.mScope = ApplicationScope::HEADERS;
        matchedFilter.mDestination = "Napoli";
        matchedFilter.mDuplicationType = DuplicationType::REQUEST_WITH_ANSWER;
        tFilter matchedFilter2(".*", ApplicationScope::ALL, "Alger", DuplicationType::COMPLETE_REQUEST, boost::regex("nomatch"));
        matchedFilter2.mMatch = "pikolinos";
        matchedFilter2.mScope = ApplicationScope::URL_AND_HEADERS;
        matchedFilter2.mDestination = "Torino";
        matchedFilter2.mDuplicationType = DuplicationType::REQUEST_WITH_ANSWER;
        std::list<const tFilter *> matchedFilters;
        matchedFilters.push_back(&matchedFilter);
        matchedFilters.push_back(&matchedFilter2);
        proc.addValidationHeadersDup(lInfo, matchedFilters,0,0);

        CPPUNIT_ASSERT_EQUAL(std::string("X-MATCHED-PATTERN"), lInfo.mHeadersOut.front().first);
        CPPUNIT_ASSERT_EQUAL(std::string("papalino"), lInfo.mHeadersOut.front().second);

        lInfo.mHeadersOut.pop_front();
        CPPUNIT_ASSERT_EQUAL(std::string("X-MATCHED-PATTERN"), lInfo.mHeadersOut.front().first);
        CPPUNIT_ASSERT_EQUAL(std::string("pikolinos"), lInfo.mHeadersOut.front().second);
        
        lInfo.mHeadersOut.pop_front();
        
        CPPUNIT_ASSERT_EQUAL(std::string("The request is duplicated, HEADERS filter: \".*\" matched: \"papalino\" Destination: Napoli AND URL_AND_HEADERS filter: \".*\" matched: \"pikolinos\" Destination: Torino"), lInfo.mHeadersOut.front().second);
        lInfo.mHeadersOut.pop_front();
        matchedFilters.clear();
        proc.addValidationHeadersDup(lInfo, matchedFilters,1,2);
        CPPUNIT_ASSERT_EQUAL(std::string("The request is not duplicated, having found 1 DupDestination(s) and attempted to match 2 DupFilter or DupRawFilter"), lInfo.mHeadersOut.front().second);
        
        struct curl_slist *slist = NULL;
        lInfo.mValidationHeaderDup = true;
        proc.addValidationHeadersCompare(lInfo, matchedFilter, slist);
        CPPUNIT_ASSERT_EQUAL(true, lInfo.mValidationHeaderComp);
    }

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
        DupConf conf;
        MAKE_REQ_INFO_BODY("42","/toto", "/toto/titi/", query, conf, proc, &body);
        conf.currentApplicationScope = ApplicationScope::QUERY_STRING;

        proc.addRawFilter(".*", conf, tFilter::eFilterTypes::REGULAR);
        proc.addRawSubstitution( "1", "2", conf);
        std::list<const tFilter *> matches = proc.processRequest(ri);
        CPPUNIT_ASSERT(!matches.empty());
        const tFilter *match = *matches.begin();

        RequestProcessor::tCommandsByDestination &cbd = proc.mCommands.at(&conf);
        Commands &c = cbd.at(match->mDestination);

        proc.substituteRequest(ri, c);

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
        DupConf conf;
        MAKE_REQ_INFO_BODY("42","/toto", "/toto/titi/", query, conf, proc, &body);
        conf.currentApplicationScope = ApplicationScope::BODY;

        proc.addRawFilter( ".*", conf, tFilter::eFilterTypes::REGULAR);
        proc.addRawSubstitution( "1", "2", conf);

        std::list<const tFilter *> matches = proc.processRequest(ri);
        CPPUNIT_ASSERT(!matches.empty());
        const tFilter *match = *matches.begin();

        RequestProcessor::tCommandsByDestination &cbd = proc.mCommands.at(&conf);
        Commands &c = cbd.at(match->mDestination);

        proc.substituteRequest(ri, c);

        CPPUNIT_ASSERT_EQUAL(query, ri.mArgs);
        CPPUNIT_ASSERT_EQUAL(std::string("mybody2test"), ri.mBody);
    }

    {
        // Simple substitution on body AND HEADER
        RequestProcessor proc;
        std::string query;

        query = "arg1=myarg1";
        std::string body = "mybody1test";
        DupConf conf;
        MAKE_REQ_INFO_BODY("42","/toto", "/toto/titi/", query, conf, proc, &body);
        conf.currentApplicationScope = ApplicationScope::ALL;

        proc.addRawFilter(".*", conf, tFilter::eFilterTypes::REGULAR);
        proc.addRawSubstitution("1", "2", conf);

        std::list<const tFilter *> matches = proc.processRequest(ri);
        CPPUNIT_ASSERT(!matches.empty());
        const tFilter *match = *matches.begin();

        RequestProcessor::tCommandsByDestination &cbd = proc.mCommands.at(&conf);
        Commands &c = cbd.at(match->mDestination);

        proc.substituteRequest(ri, c);

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
        MAKE_REQ_INFO_BODY("42","/toto", "/toto/titi/", query, conf, proc, &body);

        proc.addRawFilter(".*", conf, tFilter::eFilterTypes::REGULAR);
        conf.currentApplicationScope = ApplicationScope::QUERY_STRING;
        proc.addRawSubstitution( "1", "2", conf);
        conf.currentApplicationScope = ApplicationScope::BODY;
        proc.addRawSubstitution( "1", "3", conf);

        std::list<const tFilter *> matches = proc.processRequest(ri);
        CPPUNIT_ASSERT(!matches.empty());
        const tFilter *match = *matches.begin();

        RequestProcessor::tCommandsByDestination &cbd = proc.mCommands.at(&conf);
        Commands &c = cbd.at(match->mDestination);

        proc.substituteRequest(ri, c);

        CPPUNIT_ASSERT_EQUAL(std::string("arg2=myarg2"), ri.mArgs);
        CPPUNIT_ASSERT_EQUAL(std::string("mybody3test"), ri.mBody);
    }
}

void TestRequestProcessor::testDupFormat() {

    // sendDupFormat test
    RequestProcessor proc;
    std::string query = "theBig=Lebowski";
    std::string body = "mybody1test";
    RequestInfo ri = RequestInfo("42", "/mypath", "/mypath/wb", query, &body);
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
    RequestInfo ri = RequestInfo("42", "/path", "/path", "arg1=value1");
    CPPUNIT_ASSERT(!ri.hasBody());
    ri.mBody = "sdf";
    CPPUNIT_ASSERT(ri.hasBody());
}

void TestRequestProcessor::testTimeout() {
    RequestProcessor proc;
    proc.setTimeout(200);
    MultiThreadQueue<boost::shared_ptr<RequestInfo> > queue;

    DupConf conf;
    conf.currentApplicationScope = ApplicationScope::ALL;
    conf.currentDupDestination = "Honolulu:8080";
    proc.addFilter("SID", "mySid", conf, tFilter::eFilterTypes::REGULAR);

    boost::shared_ptr<RequestInfo> ri = boost::shared_ptr<RequestInfo>(new RequestInfo("42","/spp/main", "/spp/main", "SID=mySid"));
    ri->mConf = &conf;
    queue.push(ri);
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
    proc.addRawFilter( ".*", conf, tFilter::eFilterTypes::REGULAR);
    proc.addSubstitution( "titi", "value", "replacedValue", conf);

    query = "titi=value&tutu=tatae";
    RequestInfo ri = makeReqInfo("42", "/toto", "/toto", query, &conf, proc);
    ri.mConf = &conf;
    ri.mBody = "key1=what??&titi=value";

    std::list<const tFilter *> matches = proc.processRequest(ri);
    CPPUNIT_ASSERT(!matches.empty());
    const tFilter *match = *matches.begin();

    RequestProcessor::tCommandsByDestination &cbd = proc.mCommands.at(&conf);
    Commands &c = cbd.at(match->mDestination);

    proc.substituteRequest(ri, c);

    CPPUNIT_ASSERT_EQUAL(std::string("titi=value&tutu=tatae"), ri.mArgs);
    CPPUNIT_ASSERT_EQUAL(std::string("KEY1=what%3f%3f&TITI=replacedValue"), ri.mBody);
}

void TestRequestProcessor::testMultiDestination() {

    DupConf conf;
    conf.currentApplicationScope = ApplicationScope::ALL;
    {
        // 2 filters that match on one destination
        RequestProcessor proc;
        conf.currentDupDestination = "Honolulu:8080";

        proc.addFilter( "INFO", "[my]+", conf, tFilter::eFilterTypes::REGULAR);
        proc.addFilter("INFO", "myinfo", conf, tFilter::eFilterTypes::REGULAR);

        MAKE_REQ_INFO("42","/match", "/match/pws/titi/", "INFO=myinfo", conf, proc);
        CPPUNIT_ASSERT_EQUAL(1, (int)proc.processRequest(ri).size());
    }

    {
        // 2 filters that match 2 different destinations
        RequestProcessor proc;
        conf.currentDupDestination = "Honolulu:8080";

        proc.addFilter( "INFO", "[my]+", conf, tFilter::eFilterTypes::REGULAR);
        conf.currentDupDestination = "Hikkaduwa:8090";
        proc.addFilter( "INFO", "myinfo", conf, tFilter::eFilterTypes::REGULAR);

        MAKE_REQ_INFO("42","/match", "/match/pws/titi/", "INFO=myinfo", conf, proc);
        std::list<const tFilter *> ff = proc.processRequest(ri);
        CPPUNIT_ASSERT_EQUAL(2, (int)ff.size());

        // Check the first filter
        const tFilter *first = ff.front();
        ff.pop_front();
        const tFilter *second = ff.front();

        // Order is not guaranteed
        if (second->mDestination == "Honolulu:8080") {
            std::swap(first, second);
        }

        CPPUNIT_ASSERT_EQUAL(std::string("Honolulu:8080"), first->mDestination);
        CPPUNIT_ASSERT_EQUAL(std::string("Hikkaduwa:8090"), second->mDestination);

        CPPUNIT_ASSERT_EQUAL(tFilter::eFilterTypes::REGULAR, first->mFilterType);
        CPPUNIT_ASSERT_EQUAL(tFilter::eFilterTypes::REGULAR, second->mFilterType);
    }

    {
        Log::debug("### 2 filters that match 2 different destinations but one destination has a prevent filter that matches too  ###");
        RequestProcessor proc;
        conf.currentDupDestination = "Honolulu:8080";

        proc.addFilter("INFO", "[my]+", conf, tFilter::eFilterTypes::REGULAR);
        conf.currentDupDestination = "Hikkaduwa:8090";
        proc.addFilter( "INFO", "myinfo", conf, tFilter::eFilterTypes::REGULAR);
        proc.addFilter( "NO", "dup", conf, tFilter::eFilterTypes::PREVENT_DUPLICATION);

        MAKE_REQ_INFO("42","/match", "/match/pws/titi/", "INFO=myinfo&NO=dup", conf, proc);
        std::list<const tFilter *> ff = proc.processRequest(ri);
        CPPUNIT_ASSERT_EQUAL(1, (int)ff.size());

        // Check the filter that matched
        const tFilter *first = ff.front();
        CPPUNIT_ASSERT_EQUAL(std::string("Honolulu:8080"), first->mDestination);
        CPPUNIT_ASSERT_EQUAL(tFilter::eFilterTypes::REGULAR, first->mFilterType);
    }

}

void TestRequestProcessor::testPerformCurlCall() {

    DupConf lConf;
    CURL* lCurl = curl_easy_init();

    std::string data;
    curl_easy_setopt(lCurl, CURLOPT_DEBUGFUNCTION, curlTrace);
    curl_easy_setopt(lCurl, CURLOPT_DEBUGDATA, (void*)&data);
    curl_easy_setopt(lCurl, CURLOPT_VERBOSE, 1L);

    std::string lDestination = "localhost:8050";
    std::string lQueryArgs = "arg=myarg";
    std::string lBody = "mybodytest";

    RequestProcessor requestProcessor;
    RequestInfo requestInfo = RequestInfo("42","/test", "/test/path/", lQueryArgs, &lBody);
    tFilter matchedFilter("test", ApplicationScope::ALL,lDestination, DuplicationType::REQUEST_WITH_ANSWER,boost::regex("nomatch"));


    matchedFilter.mDestination = lDestination;
    requestInfo.mConf = &lConf;
    lConf.currentApplicationScope = ApplicationScope::ALL;

    //Case 1: DuplicationType::REQUEST_WITH_ANSWER and comparison is true X_COMP_LOG is set
    {
        matchedFilter.mDuplicationType = DuplicationType::REQUEST_WITH_ANSWER;
        requestInfo.mValidationHeaderDup = true;
        requestProcessor.performCurlCall(lCurl, matchedFilter, requestInfo);
        std::cout << data;

        /**********AddCommonHeaders**********/
       // CPPUNIT_ASSERT(data.find("ELAPSED_TIME_BY_DUP:") != std::string::npos);
       // CPPUNIT_ASSERT(data.find("Expect:") != std::string::npos);
        // CPPUNIT_ASSERT(data.find("X-DUPLICATED-REQUEST: 1:") != std::string::npos);
        // CPPUNIT_ASSERT(data.find("User-RealAgent: mod-dup:") != std::string::npos);
        /**********AddCommonHeaders**********/

        /**********AddValidationHeadersCompare**********/
        //CPPUNIT_ASSERT(data.find("X_COMP_LOG: ON") != std::string::npos);
        /**********AddValidationHeadersCompare**********/
    }

    //Case 2: DuplicationType::REQUEST_WITH_ANSWER and comparison is false X_COMP_LOG is not set
    {
        matchedFilter.mDuplicationType = DuplicationType::REQUEST_WITH_ANSWER;
        requestInfo.mValidationHeaderDup = false;
        requestProcessor.performCurlCall(lCurl, matchedFilter, requestInfo);

        /**********AddCommonHeaders**********/
//        CPPUNIT_ASSERT(data.find("ELAPSED_TIME_BY_DUP:") != std::string::npos);
//        CPPUNIT_ASSERT(data.find("Expect:") != std::string::npos);
//        CPPUNIT_ASSERT(data.find("X-DUPLICATED-REQUEST: 1:") != std::string::npos);
//        CPPUNIT_ASSERT(data.find("User-RealAgent: mod-dup:") != std::string::npos);
        /**********AddCommonHeaders**********/

        /**********AddValidationHeadersCompare**********/
//        CPPUNIT_ASSERT(data.find("X_COMP_LOG: ON")  == std::string::npos);
        /**********AddValidationHeadersCompare**********/
    }

    //Case 3: DuplicationType::COMPLETE_REQUEST
    {
        matchedFilter.mDuplicationType = DuplicationType::COMPLETE_REQUEST;
        requestProcessor.performCurlCall(lCurl, matchedFilter, requestInfo);

        /**********AddCommonHeaders**********/
//        CPPUNIT_ASSERT(data.find("ELAPSED_TIME_BY_DUP:") != std::string::npos);
//        CPPUNIT_ASSERT(data.find("Expect:") != std::string::npos);
//        CPPUNIT_ASSERT(data.find("X-DUPLICATED-REQUEST: 1:") != std::string::npos);
//        CPPUNIT_ASSERT(data.find("User-RealAgent: mod-dup:") != std::string::npos);
        /**********AddCommonHeaders**********/

        /**********AddValidationHeadersCompare**********/
//        CPPUNIT_ASSERT(data.find("X_COMP_LOG: ON") == std::string::npos);
        /**********AddValidationHeadersCompare**********/

    }

    //Case 4: Simple GET
    {
        matchedFilter.mDuplicationType = DuplicationType::NONE;
        requestProcessor.performCurlCall(lCurl, matchedFilter, requestInfo);

        /**********AddCommonHeaders**********/
//        CPPUNIT_ASSERT(data.find("ELAPSED_TIME_BY_DUP:") != std::string::npos);
//        CPPUNIT_ASSERT(data.find("Expect:") != std::string::npos);
//        CPPUNIT_ASSERT(data.find("X-DUPLICATED-REQUEST: 1:") != std::string::npos);
//        CPPUNIT_ASSERT(data.find("User-RealAgent: mod-dup:") != std::string::npos);
        /**********AddCommonHeaders**********/

        /**********AddValidationHeadersCompare**********/
//        CPPUNIT_ASSERT(data.find("X_COMP_LOG: ON") == std::string::npos);
        /**********AddValidationHeadersCompare**********/
    }

}

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

